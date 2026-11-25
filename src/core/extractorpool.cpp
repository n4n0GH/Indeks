// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "extractorpool.h"

#include "indeks_core_debug.h"
#include "paths.h"
#include "task.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QThread>

#include <deque>

using namespace Qt::StringLiterals;

namespace Indeks
{

namespace
{
constexpr int MaxHelpers = 16;
// Small files travel together, so that a folder of notes is not one round trip per note.
constexpr int MaxBatchFiles = 24;
constexpr qint64 MaxBatchBytes = 2LL * 1024 * 1024;
// A helper that has been silent on one file for this long is given up on. Generous: turning a
// large Office document into text by way of LibreOffice takes a while.
constexpr int DefaultFileTimeoutMs = 180000;

int fileTimeoutMs()
{
    bool ok = false;
    const int fromEnvironment = qEnvironmentVariableIntValue("INDEKS_EXTRACTOR_TIMEOUT_MS", &ok);
    return ok && fromEnvironment > 0 ? fromEnvironment : DefaultFileTimeoutMs;
}
}

// ---------------------------------------------------------------------------------------------
// Protocol: messages are a length followed by that many bytes, in both directions. The bytes
// are a QDataStream of the message kind and its fields.

namespace ExtractorProtocol
{
namespace
{
constexpr auto StreamVersion = QDataStream::Qt_6_5;
}

QByteArray frame(const QByteArray &payload)
{
    QByteArray framed;
    QDataStream stream(&framed, QIODevice::WriteOnly);
    stream.setVersion(StreamVersion);
    stream << quint32(payload.size());
    framed.append(payload);
    return framed;
}

bool takeFrame(QByteArray &buffer, QByteArray &payload)
{
    if (buffer.size() < 4) {
        return false;
    }
    QDataStream stream(buffer);
    stream.setVersion(StreamVersion);
    quint32 size = 0;
    stream >> size;
    if (buffer.size() < qsizetype(size) + 4) {
        return false;
    }
    payload = buffer.mid(4, size);
    buffer.remove(0, qsizetype(size) + 4);
    return true;
}

QByteArray batchMessage(const IndexOptions &options, const QList<ExtractorPool::Request> &tasks, const QList<int> &positions)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(StreamVersion);
    stream << quint8(Batch) << options.indexContent << options.ocrEnabled << options.officeConversion << qint32(options.maxContentSizeMb) << quint32(positions.size());
    for (const int position : positions) {
        const ExtractorPool::Request &task = tasks.at(position);
        stream << qint32(position) << task.reading.contentFile << task.reading.name << task.reading.keepTextIfHash << qint64(task.reading.partOffset)
               << qint64(task.reading.partLength) << task.reading.lookOnly;
    }
    return frame(payload);
}

bool readBatch(const QByteArray &payload, IndexOptions &options, QList<ExtractorPool::Request> &tasks)
{
    QDataStream stream(payload);
    stream.setVersion(StreamVersion);
    quint8 kind = 0;
    qint32 maxContentSizeMb = 0;
    quint32 count = 0;
    stream >> kind >> options.indexContent >> options.ocrEnabled >> options.officeConversion >> maxContentSizeMb >> count;
    if (kind != Batch || stream.status() != QDataStream::Ok) {
        return false;
    }
    options.maxContentSizeMb = maxContentSizeMb;
    for (quint32 i = 0; i < count; ++i) {
        ExtractorPool::Request task;
        qint32 id = 0;
        qint64 partOffset = 0;
        qint64 partLength = 0;
        stream >> id >> task.reading.contentFile >> task.reading.name >> task.reading.keepTextIfHash >> partOffset >> partLength >> task.reading.lookOnly;
        task.reading.partOffset = partOffset;
        task.reading.partLength = partLength;
        task.id = id;
        tasks.append(task);
    }
    return stream.status() == QDataStream::Ok;
}

QByteArray startedMessage(int id)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(StreamVersion);
    stream << quint8(Started) << qint32(id);
    return frame(payload);
}

QByteArray resultMessage(int id, const Indexing::Prepared &prepared)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(StreamVersion);
    stream << quint8(Result) << qint32(id) << prepared.mimeType << qint32(prepared.kind) << prepared.hash << prepared.textKept << prepared.extracted.text
           << prepared.extracted.metadata << qint32(prepared.extracted.pages) << prepared.extracted.wantsOcr << prepared.looked << qint32(prepared.look.shape)
           << qint32(prepared.look.ink);
    for (const quint32 word : prepared.look.bits) {
        stream << word;
    }
    return frame(payload);
}

QByteArray batchDoneMessage()
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(StreamVersion);
    stream << quint8(BatchDone);
    return frame(payload);
}

Message readReply(const QByteArray &payload, int &id, Indexing::Prepared &prepared)
{
    QDataStream stream(payload);
    stream.setVersion(StreamVersion);
    quint8 kind = 0;
    stream >> kind;
    if (kind == Started || kind == Result) {
        qint32 number = 0;
        stream >> number;
        id = number;
    }
    if (kind == Result) {
        qint32 documentKind = 0;
        qint32 pages = 0;
        stream >> prepared.mimeType >> documentKind >> prepared.hash >> prepared.textKept >> prepared.extracted.text >> prepared.extracted.metadata >> pages
            >> prepared.extracted.wantsOcr;
        qint32 shape = 0;
        qint32 ink = 0;
        stream >> prepared.looked >> shape >> ink;
        for (quint32 &word : prepared.look.bits) {
            stream >> word;
        }
        prepared.look.shape = Fingerprint::Shape(shape);
        prepared.look.ink = ink;
        prepared.kind = Kind(documentKind);
        prepared.extracted.pages = pages;
    }
    return Message(kind);
}
}

// ---------------------------------------------------------------------------------------------

ExtractorPool::ExtractorPool() = default;

ExtractorPool::~ExtractorPool()
{
    shutdown();
}

QString ExtractorPool::helperPath()
{
    static const QString path = [] {
        // Beside the program when run from the build folder, in the library programs folder
        // when installed.
        const QString beside = QCoreApplication::applicationDirPath() + "/indeks-extractor"_L1;
        if (QFileInfo(beside).isExecutable()) {
            return beside;
        }
        const QString installed = QStringLiteral(INDEKS_LIBEXEC_DIR) + "/indeks-extractor"_L1;
        if (QFileInfo(installed).isExecutable()) {
            return installed;
        }
        return QStandardPaths::findExecutable(u"indeks-extractor"_s);
    }();
    return path;
}

int ExtractorPool::defaultProcesses()
{
    return qBound(1, QThread::idealThreadCount() / 2, 8);
}

bool ExtractorPool::start(Helper &helper, int slot)
{
    if (helper.process && helper.process->state() == QProcess::Running) {
        return true;
    }
    const QString program = helperPath();
    if (program.isEmpty()) {
        return false;
    }
    helper.process = std::make_unique<QProcess>();
    // What the libraries have to say goes where Indeks' own messages go.
    helper.process->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    helper.process->start(program, {u"--database-dir"_s, Paths::dataDir(), u"--slot"_s, QString::number(slot)});
    if (!helper.process->waitForStarted(10000)) {
        qCWarning(INDEKS_CORE) << "Cannot start" << program << helper.process->errorString();
        helper.process.reset();
        return false;
    }
    helper.buffer.clear();
    helper.inFlight.clear();
    helper.current = -1;
    helper.busy = false;
    return true;
}

void ExtractorPool::release(Helper &helper)
{
    if (helper.process) {
        if (helper.process->state() != QProcess::NotRunning) {
            helper.process->kill();
            helper.process->waitForFinished(2000);
        }
        helper.process.reset();
    }
    helper.buffer.clear();
    helper.inFlight.clear();
    helper.current = -1;
    helper.busy = false;
}

void ExtractorPool::shutdown()
{
    for (Helper &helper : m_helpers) {
        release(helper);
    }
    m_helpers.clear();
}

bool ExtractorPool::run(const QList<Request> &tasks,
                        const IndexOptions &options,
                        int processes,
                        const std::function<void(int)> &started,
                        const std::function<void(const Outcome &)> &finished,
                        const std::function<void()> &tick)
{
    if (tasks.isEmpty()) {
        return true;
    }
    std::deque<int> queue;
    for (int position = 0; position < tasks.size(); ++position) {
        queue.push_back(position);
    }
    int remaining = int(tasks.size());

    // Without the helper program: here and now, one after the other.
    auto readHere = [&] {
        while (!queue.empty()) {
            if (Task::aborted()) {
                return false;
            }
            const Request &task = tasks.at(queue.front());
            queue.pop_front();
            if (started) {
                started(task.id);
            }
            finished({task.id, true, Indexing::readFile(task.reading, options)});
            if (tick) {
                tick();
            }
        }
        return true;
    };
    if (m_unavailable || helperPath().isEmpty()) {
        return readHere();
    }

    // No more helpers than there is work for.
    const int wanted = qBound(1, qMin(processes > 0 ? processes : defaultProcesses(), int((tasks.size() + 3) / 4)), MaxHelpers);
    if (int(m_helpers.size()) < wanted) {
        m_helpers.resize(wanted);
    }
    const int timeoutMs = fileTimeoutMs();

    while (remaining > 0) {
        if (Task::aborted()) {
            shutdown();
            return false;
        }
        int busyHelpers = 0;
        for (const Helper &helper : m_helpers) {
            busyHelpers += helper.busy ? 1 : 0;
        }
        for (int slot = 0; slot < wanted; ++slot) {
            Helper &helper = m_helpers[slot];

            if (!helper.busy && !queue.empty()) {
                if (!start(helper, slot)) {
                    // The helper program cannot be run at all. Give the files that are with
                    // helpers back and read everything here.
                    m_unavailable = true;
                    for (Helper &other : m_helpers) {
                        for (auto it = other.inFlight.crbegin(); it != other.inFlight.crend(); ++it) {
                            queue.push_front(*it);
                        }
                    }
                    shutdown();
                    return readHere();
                }
                QList<int> batch;
                qint64 bytes = 0;
                while (!queue.empty() && batch.size() < MaxBatchFiles && (batch.isEmpty() || bytes + tasks.at(queue.front()).size <= MaxBatchBytes)) {
                    bytes += tasks.at(queue.front()).size;
                    batch.append(queue.front());
                    queue.pop_front();
                }
                helper.process->write(ExtractorProtocol::batchMessage(options, tasks, batch));
                helper.inFlight = batch;
                helper.current = -1;
                helper.busy = true;
                helper.timer.start();
                ++busyHelpers;
            }
            if (!helper.busy) {
                continue;
            }

            // Hear what the helper has to say; this is also what sends it its request.
            if (helper.process->bytesAvailable() == 0) {
                helper.process->waitForReadyRead(busyHelpers > 1 ? 2 : 15);
            }
            helper.buffer += helper.process->readAllStandardOutput();
            QByteArray payload;
            while (helper.busy && ExtractorProtocol::takeFrame(helper.buffer, payload)) {
                int position = -1;
                Indexing::Prepared prepared;
                switch (ExtractorProtocol::readReply(payload, position, prepared)) {
                case ExtractorProtocol::Started:
                    helper.current = position;
                    helper.timer.restart();
                    if (started && position >= 0 && position < tasks.size()) {
                        started(tasks.at(position).id);
                    }
                    break;
                case ExtractorProtocol::Result:
                    if (helper.inFlight.removeOne(position)) {
                        finished({tasks.at(position).id, true, prepared});
                        --remaining;
                    }
                    helper.current = -1;
                    helper.timer.restart();
                    break;
                case ExtractorProtocol::BatchDone:
                    helper.busy = false;
                    break;
                default:
                    break;
                }
            }

            const bool gone = helper.process->state() == QProcess::NotRunning;
            const bool stuck = helper.busy && helper.timer.elapsed() > timeoutMs;
            if (helper.busy && (gone || stuck)) {
                // The file it was on is the one to blame; if it had not said, the first.
                const int culprit = helper.current >= 0 ? helper.current : helper.inFlight.value(0, -1);
                qCWarning(INDEKS_CORE) << "The helper" << (stuck ? "did not come back from" : "died on")
                                       << (culprit >= 0 ? tasks.at(culprit).reading.contentFile : QString());
                QList<int> others = helper.inFlight;
                others.removeOne(culprit);
                release(helper);
                if (culprit >= 0) {
                    finished({tasks.at(culprit).id, false, {}});
                    --remaining;
                }
                // The rest of its files did nothing wrong and are read by another helper.
                for (auto it = others.crbegin(); it != others.crend(); ++it) {
                    queue.push_front(*it);
                }
            }
        }
        if (tick) {
            tick();
        }
    }

    // Keep one helper for the next handful of files; a crowd of idle ones only takes memory.
    for (size_t slot = 1; slot < m_helpers.size(); ++slot) {
        release(m_helpers[slot]);
    }
    m_helpers.resize(1);
    return true;
}

}
