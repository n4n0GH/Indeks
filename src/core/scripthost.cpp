// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "scripthost.h"

#include "database.h"
#include "indeks_core_debug.h"
#include "paths.h"
#include "rules.h"
#include "scriptbridge.h"
#include "scripts.h"
#include "store.h"

#include <KLocalizedString>

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QProcess>
#include <QStandardPaths>

#include <functional>
#include <memory>

using namespace Qt::StringLiterals;

namespace Indeks
{

namespace
{
// Starting Python inside the sandbox and reading the script.
constexpr int StartTimeoutMs = 20000;
// How long a script may take over one document unless setTimeLimit() says otherwise.
std::atomic_int s_timeLimitMs = 30000;
// How often a wait looks whether it was called off.
constexpr int PollMs = 50;
// More than this in one answer is not an answer.
constexpr qint64 MaxAnswerBytes = 16 * 1024 * 1024;
// Documents are looked up this many at a time, to know which files the sandbox has to show.
constexpr int ChunkSize = 200;
// Of a long run, only so many documents with something to tell are reported one by one.
constexpr int MaxEntries = 200;
constexpr int ProgressIntervalMs = 100;
constexpr int ChangedIntervalMs = 300;

QString pythonPath()
{
    static const QString path = QStandardPaths::findExecutable(u"python3"_s);
    return path;
}

QString bwrapPath()
{
    static const QString path = QStandardPaths::findExecutable(u"bwrap"_s);
    return path;
}

bool isUnder(const QString &path, const QStringList &folders)
{
    for (const QString &folder : folders) {
        if (path == folder || path.startsWith(folder.endsWith(u'/') ? folder : QString(folder + u'/'))) {
            return true;
        }
    }
    return false;
}

/**
 * The arguments for bubblewrap that make the sandbox a script runs in. Nothing of the computer
 * is in it but what is listed here, all of it read-only: the installed system, so that Python
 * and its libraries are there, and @p visible, the documents. No network, no other processes,
 * none of the user's own files, and no way to talk to the desktop or to another program.
 */
QStringList sandboxArguments(const QStringList &visible)
{
    QStringList arguments = {u"--unshare-all"_s, u"--die-with-parent"_s, u"--new-session"_s};
    QStringList system;
    for (const QString &folder : {u"/usr"_s, u"/etc"_s, u"/opt"_s, u"/nix"_s, u"/gnu"_s}) {
        if (QFileInfo(folder).isDir()) {
            arguments << u"--ro-bind"_s << folder << folder;
            system.append(folder);
        }
    }
    // On most systems these are links into /usr by now.
    for (const QString &folder : {u"/bin"_s, u"/sbin"_s, u"/lib"_s, u"/lib32"_s, u"/lib64"_s, u"/libx32"_s}) {
        const QFileInfo info(folder);
        if (info.isSymLink()) {
            arguments << u"--symlink"_s << info.symLinkTarget() << folder;
        } else if (info.isDir()) {
            arguments << u"--ro-bind"_s << folder << folder;
            system.append(folder);
        }
    }
    arguments << u"--proc"_s << u"/proc"_s << u"--dev"_s << u"/dev"_s << u"--tmpfs"_s << u"/tmp"_s;

    // A Python that was installed somewhere else brings its own folder.
    const QString python = QFileInfo(pythonPath()).canonicalFilePath();
    if (!python.isEmpty() && !isUnder(python, system)) {
        const QString prefix = QDir::cleanPath(QFileInfo(python).path() + "/.."_L1);
        if (prefix != "/"_L1) {
            arguments << u"--ro-bind"_s << prefix << prefix;
        }
    }
    // Of the user's own folder, only the Python packages installed for them.
    const QString home = QDir::homePath();
    arguments << u"--dir"_s << home;
    const QString userLibraries = home + "/.local/lib"_L1;
    if (QFileInfo(userLibraries).isDir()) {
        arguments << u"--ro-bind"_s << userLibraries << userLibraries;
    }

    // Folders before what is inside them.
    QStringList paths = visible;
    paths.removeDuplicates();
    paths.sort();
    for (const QString &path : std::as_const(paths)) {
        if (QFileInfo::exists(path)) {
            arguments << u"--ro-bind"_s << path << path;
        }
    }
    arguments << u"--chdir"_s << u"/"_s << u"--"_s;
    return arguments;
}

QProcessEnvironment sandboxEnvironment()
{
    const QProcessEnvironment system = QProcessEnvironment::systemEnvironment();
    QProcessEnvironment environment;
    environment.insert(u"PATH"_s, u"/usr/local/bin:/usr/bin:/bin"_s);
    environment.insert(u"HOME"_s, QDir::homePath());
    environment.insert(u"LANG"_s, system.value(u"LANG"_s, u"C.UTF-8"_s));
    for (const QString &name : {u"LC_ALL"_s, u"LANGUAGE"_s, u"TZ"_s}) {
        if (system.contains(name)) {
            environment.insert(name, system.value(name));
        }
    }
    environment.insert(u"PYTHONIOENCODING"_s, u"utf-8"_s);
    environment.insert(u"PYTHONDONTWRITEBYTECODE"_s, u"1"_s);
    environment.insert(u"PYTHONUNBUFFERED"_s, u"1"_s);
    return environment;
}

QString lastLines(const QByteArray &output, int count)
{
    const QStringList lines = QString::fromUtf8(output).trimmed().split(u'\n', Qt::SkipEmptyParts);
    return lines.mid(qMax(0, int(lines.size()) - count)).join(u'\n');
}

/** One Python in its sandbox, with one script loaded. Waits; to be used off the main thread. */
class ScriptProcess
{
public:
    struct Answer {
        QVariantList actions;
        QString output;
        QString error; ///< the script raised this on the document
        QString problem; ///< the script is gone: it took too long, crashed or made no sense
        bool cancelled = false;
    };

    explicit ScriptProcess(std::function<bool()> cancelled)
        : m_cancelled(std::move(cancelled))
    {
    }

    ~ScriptProcess()
    {
        stop();
    }

    bool isRunning() const
    {
        return m_process && m_process->state() == QProcess::Running;
    }

    /** Whether the sandbox that is running shows all of @p paths. */
    bool sees(const QStringList &paths) const
    {
        for (const QString &path : paths) {
            if (!isUnder(path, m_visible)) {
                return false;
            }
        }
        return true;
    }

    /** Starts Python in a sandbox that shows @p visible, and has it read the script. */
    bool start(const QString &name, const QString &source, const QStringList &visible, QString *problem)
    {
        stop();
        m_visible = visible;
        m_process = std::make_unique<QProcess>();
        m_process->setProcessEnvironment(sandboxEnvironment());
        m_process->start(bwrapPath(), sandboxArguments(visible) + QStringList{pythonPath(), u"-u"_s, u"-B"_s, u"-c"_s, QString::fromUtf8(scriptBridgeSource)});
        if (!m_process->waitForStarted(StartTimeoutMs)) {
            *problem = i18nc("@info", "The sandbox for scripts could not be started: %1", m_process->errorString());
            stop();
            return false;
        }
        send({{u"name"_s, name}, {u"source"_s, source}});
        QJsonObject reply;
        bool cancelled = false;
        if (!receive(&reply, StartTimeoutMs, problem, &cancelled)) {
            if (cancelled) {
                problem->clear();
            }
            stop();
            return false;
        }
        if (!reply.value("ready"_L1).toBool()) {
            *problem = i18nc("@info %1 is Python's own description of the error", "The script cannot be run as it is:\n%1", reply.value("failed"_L1).toString());
            stop();
            return false;
        }
        return true;
    }

    /** Hands the script one document. @p text is asked for the document's text if the script wants it. */
    Answer process(const QJsonObject &document, const std::function<QString()> &text)
    {
        Answer answer;
        send({{u"document"_s, document}});
        const int limit = s_timeLimitMs;
        QElapsedTimer timer;
        timer.start();
        while (true) {
            QJsonObject reply;
            const int left = limit - int(timer.elapsed());
            if (left <= 0) {
                answer.problem = i18ncp("@info", "The script took longer than %1 second over this document and was stopped.",
                                        "The script took longer than %1 seconds over this document and was stopped.", qMax(1, limit / 1000));
            } else if (receive(&reply, left, &answer.problem, &answer.cancelled)) {
                if (reply.value("need"_L1).toString() == "text"_L1) {
                    send({{u"text"_s, text()}});
                    continue;
                }
                answer.actions = reply.value("actions"_L1).toArray().toVariantList();
                answer.output = reply.value("output"_L1).toString();
                answer.error = reply.value("error"_L1).toString();
                return answer;
            } else if (!answer.cancelled && timer.elapsed() >= limit) {
                continue; // said in the words for a document that takes too long
            }
            stop();
            return answer;
        }
    }

    void stop()
    {
        if (!m_process) {
            return;
        }
        if (m_process->state() != QProcess::NotRunning) {
            // Everything inside the sandbox goes with the process that made it.
            m_process->kill();
            m_process->waitForFinished(3000);
        }
        m_process.reset();
        m_visible.clear();
    }

private:
    void send(const QJsonObject &message)
    {
        m_process->write(QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n');
    }

    // One line from the script. False with the reason in @p problem if none comes.
    bool receive(QJsonObject *message, int timeoutMs, QString *problem, bool *cancelled)
    {
        QElapsedTimer timer;
        timer.start();
        while (!m_process->canReadLine()) {
            if (m_cancelled()) {
                *cancelled = true;
                return false;
            }
            if (m_process->state() == QProcess::NotRunning) {
                const QString said = lastLines(m_process->readAllStandardError(), 12);
                *problem = said.isEmpty() ? i18nc("@info", "The script ended before it was done.")
                                          : i18nc("@info %1 is what the script or Python printed last", "The script ended before it was done:\n%1", said);
                return false;
            }
            if (timer.elapsed() >= timeoutMs) {
                *problem = i18ncp("@info", "The script did not answer within %1 second and was stopped.", "The script did not answer within %1 seconds and was stopped.",
                                  qMax(1, timeoutMs / 1000));
                return false;
            }
            if (m_process->bytesAvailable() > MaxAnswerBytes) {
                *problem = i18nc("@info", "The script answered with more than Indeks can take and was stopped.");
                return false;
            }
            m_process->waitForReadyRead(PollMs);
        }
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(m_process->readLine(), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            *problem = i18nc("@info", "The script answered with something Indeks does not understand and was stopped.");
            return false;
        }
        *message = document.object();
        return true;
    }

    const std::function<bool()> m_cancelled;
    std::unique_ptr<QProcess> m_process;
    QStringList m_visible;
};

QString kindName(Kind kind)
{
    // The words the search knows the kinds by.
    switch (kind) {
    case Kind::PlainText:
        return u"text"_s;
    case Kind::RichText:
        return u"rich"_s;
    case Kind::Pdf:
        return u"pdf"_s;
    case Kind::Image:
        return u"image"_s;
    case Kind::Email:
        return u"email"_s;
    case Kind::Canvas:
        return u"canvas"_s;
    case Kind::Other:
        break;
    }
    return u"other"_s;
}

/** A document as the script is told about it; see Document in scriptbridge.py. */
QJsonObject describe(const Document &doc, const QString &path, const QHash<qint64, QStringList> &groupPaths, const QHash<qint64, QString> &locations)
{
    QJsonArray tags;
    QSqlQuery tagQuery =
        Database::exec(u"SELECT t.name FROM document_tags dt JOIN tags t ON t.id = dt.tag_id WHERE dt.doc_id = ? ORDER BY t.name COLLATE NOCASE"_s, {doc.id});
    while (tagQuery.next()) {
        tags.append(tagQuery.value(0).toString());
    }
    QJsonArray groups;
    QSqlQuery groupQuery = Database::exec(u"SELECT folder_id FROM document_folders WHERE doc_id = ?"_s, {doc.id});
    while (groupQuery.next()) {
        const QStringList groupPath = groupPaths.value(groupQuery.value(0).toLongLong());
        if (!groupPath.isEmpty()) {
            groups.append(QJsonArray::fromStringList(groupPath));
        }
    }
    QJsonArray notes;
    QSqlQuery noteQuery = Database::exec(u"SELECT data FROM annotations WHERE doc_id = ? AND kind = 'note' ORDER BY page, id"_s, {doc.id});
    while (noteQuery.next()) {
        notes.append(noteQuery.value(0).toString());
    }
    return {
        {u"id"_s, doc.id},
        {u"name"_s, doc.name},
        {u"path"_s, path.isEmpty() ? QJsonValue() : QJsonValue(path)},
        {u"url"_s, doc.url.toString()},
        {u"mime"_s, doc.mimeType},
        {u"kind"_s, kindName(doc.kind)},
        {u"size"_s, doc.size},
        {u"modified"_s, doc.modified.toSecsSinceEpoch()},
        {u"added"_s, doc.added.toSecsSinceEpoch()},
        {u"pages"_s, doc.pages},
        {u"location"_s, locations.value(doc.sourceId)},
        {u"tags"_s, tags},
        {u"groups"_s, groups},
        {u"flagged"_s, doc.flagged},
        {u"inbox"_s, doc.inbox},
        {u"comment"_s, doc.comment},
        {u"notes"_s, notes},
    };
}
}

// ---------------------------------------------------------------------------------------------
// The thread

ScriptWorker::ScriptWorker(const std::atomic<quint64> *cancelUpTo)
    : m_cancelUpTo(cancelUpTo)
{
}

bool ScriptWorker::cancelled(quint64 ticket) const
{
    return ticket <= m_cancelUpTo->load();
}

void ScriptWorker::close()
{
    Database::closeConnection();
}

void ScriptWorker::noteChanged(const QList<qint64> &ids, bool now)
{
    m_changed.append(ids);
    if (!m_sinceChanged.isValid()) {
        m_sinceChanged.start();
    }
    if (!m_changed.isEmpty() && (now || m_sinceChanged.elapsed() >= ChangedIntervalMs)) {
        Q_EMIT documentsChanged(m_changed);
        m_changed.clear();
        m_sinceChanged.restart();
    }
}

void ScriptWorker::run(const ScriptRequest &request)
{
    int documents = 0;
    bool took = false;
    if (!cancelled(request.ticket) && Database::isOpen()) {
        if (request.rules.isEmpty()) {
            runScript(request.ticket, request.script, request.source, request.trial, request.documents, QString(), &took);
            documents = int(request.documents.size());
        } else {
            for (qint64 ruleId : request.rules) {
                if (cancelled(request.ticket)) {
                    break;
                }
                documents += applyRule(request, ruleId, &took);
            }
        }
    }
    noteChanged({}, true);
    Q_EMIT finished(request.ticket, documents, took);
}

int ScriptWorker::applyRule(const ScriptRequest &request, qint64 ruleId, bool *took)
{
    const QVariantMap rule = Rules::rule(ruleId);
    const QList<qint64> matches = Rules::matching(rule.value(u"query"_s).toString(), request.documents);
    if (matches.isEmpty()) {
        return 0;
    }
    // In the order given, with the scripts in their places: what stands before a script is
    // done before it runs, so the script sees it; what stands after may undo what it did.
    QVariantList plain;
    auto applyPlain = [&] {
        *took = Rules::apply(matches, plain).took || *took;
        plain.clear();
    };
    const QVariantList actions = rule.value(u"actions"_s).toList();
    for (const QVariant &entry : actions) {
        const QVariantMap action = entry.toMap();
        if (action.value(u"type"_s).toString() != "script"_L1) {
            plain.append(action);
            continue;
        }
        applyPlain();
        if (cancelled(request.ticket)) {
            break;
        }
        runScript(request.ticket, action.value(u"name"_s).toString(), QString(), false, matches, rule.value(u"name"_s).toString(), took);
    }
    if (!cancelled(request.ticket)) {
        applyPlain();
    }
    noteChanged(matches);
    return int(matches.size());
}

void ScriptWorker::runScript(quint64 ticket, const QString &file, const QString &givenSource, bool trial, const QList<qint64> &documents, const QString &rule, bool *took)
{
    const QString name = file.endsWith(".py"_L1) ? file.chopped(3) : file;
    QVariantList entries;
    int ran = 0;
    int changed = 0;
    int failed = 0;
    int more = 0;
    bool stopped = false;

    QString source = givenSource;
    QString problem;
    if (!trial) {
        if (!Scripts::read(file, &source)) {
            problem = i18nc("@info", "The script “%1” is not there any more.", name);
        } else if (Scripts::needApproval() && !Scripts::isAllowed(source)) {
            problem = i18nc("@info",
                            "The script “%1” has not been allowed on this computer yet. It was written or changed somewhere else; read it under Scripts and allow it there if it is what you expect.",
                            name);
        }
    }
    if (problem.isEmpty()) {
        problem = ScriptHost::problem();
    }

    if (problem.isEmpty()) {
        // What every sandbox of this run shows: the indexed folders and Indeks' own files.
        // Documents that were added one by one are shown file by file as their turn comes.
        QStringList folders = {Paths::filesDir(), Paths::copiesDir(), Paths::cacheDir()};
        QHash<qint64, QString> locations;
        const QList<Source> sources = Store::sources();
        for (const Source &location : sources) {
            locations.insert(location.id, location.name);
            if (location.isLocal()) {
                folders.append(QDir::cleanPath(location.url.toLocalFile()));
            }
        }
        QHash<qint64, QStringList> groupPaths = Rules::groupPaths();

        ScriptProcess process([this, ticket] {
            return cancelled(ticket);
        });
        QElapsedTimer sinceProgress;
        sinceProgress.start();
        const int total = int(documents.size());

        for (int begin = 0; begin < total && problem.isEmpty() && !stopped; begin += ChunkSize) {
            QList<std::pair<Document, QString>> chunk;
            QStringList singles;
            for (int i = begin; i < qMin(total, begin + ChunkSize); ++i) {
                const Document doc = Store::document(documents.at(i));
                if (!doc.isValid() || doc.trashed) {
                    continue;
                }
                const QString path = Store::contentPath(doc);
                if (!path.isEmpty() && !isUnder(path, folders)) {
                    singles.append(path);
                }
                chunk.append({doc, path});
            }
            if (chunk.isEmpty()) {
                continue;
            }
            if (!process.isRunning() || !process.sees(singles)) {
                if (!process.start(file, source, folders + singles, &problem)) {
                    stopped = problem.isEmpty();
                    break;
                }
            }
            for (int i = 0; i < chunk.size(); ++i) {
                const Document &doc = chunk.at(i).first;
                if (cancelled(ticket)) {
                    stopped = true;
                    break;
                }
                if (ran == 0 || sinceProgress.elapsed() >= ProgressIntervalMs) {
                    Q_EMIT progressed(ticket, name, doc.name, begin + i, total);
                    sinceProgress.restart();
                }
                const ScriptProcess::Answer answer = process.process(describe(doc, chunk.at(i).second, groupPaths, locations), [&doc] {
                    return Store::fullText(doc.id);
                });
                if (answer.cancelled) {
                    stopped = true;
                    break;
                }
                if (!answer.problem.isEmpty()) {
                    problem = i18nc("@info %1 is the name of a document, %2 what went wrong", "At “%1”: %2", doc.name, answer.problem);
                    break;
                }
                ++ran;
                if (!answer.error.isEmpty()) {
                    ++failed;
                } else if (!answer.actions.isEmpty()) {
                    ++changed;
                    if (!trial) {
                        const Rules::Outcome outcome = Rules::apply({doc.id}, answer.actions);
                        *took = *took || outcome.took;
                        if (outcome.made) {
                            groupPaths = Rules::groupPaths();
                        }
                        noteChanged({doc.id});
                    }
                }
                if (trial || !answer.output.isEmpty() || !answer.error.isEmpty()) {
                    if (entries.size() < MaxEntries) {
                        entries.append(QVariantMap{
                            {u"id"_s, doc.id},
                            {u"name"_s, doc.name},
                            {u"output"_s, answer.output},
                            {u"error"_s, answer.error},
                            {u"actions"_s, answer.actions},
                        });
                    } else {
                        ++more;
                    }
                }
            }
        }
    }

    Q_EMIT reported(QVariantMap{
        {u"ticket"_s, ticket},
        {u"script"_s, file},
        {u"name"_s, name},
        {u"rule"_s, rule},
        {u"trial"_s, trial},
        {u"time"_s, QDateTime::currentDateTime()},
        {u"documents"_s, ran},
        {u"changed"_s, changed},
        {u"failed"_s, failed},
        {u"stopped"_s, stopped},
        {u"problem"_s, problem},
        {u"entries"_s, entries},
        {u"more"_s, more},
    });
}

// ---------------------------------------------------------------------------------------------
// The host

ScriptHost::ScriptHost(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<ScriptRequest>();
    m_thread.setObjectName(u"indeks-scripts"_s);
    m_worker = new ScriptWorker(&m_cancelUpTo);
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(this, &ScriptHost::requested, m_worker, &ScriptWorker::run);
    connect(m_worker, &ScriptWorker::documentsChanged, this, &ScriptHost::documentsChanged);
    connect(m_worker, &ScriptWorker::reported, this, &ScriptHost::reported);
    connect(m_worker, &ScriptWorker::progressed, this, [this](quint64 ticket, const QString &script, const QString &document, int done, int total) {
        if (!m_pending.contains(ticket)) {
            return;
        }
        m_script = script;
        m_document = document;
        m_done = done;
        m_total = total;
        Q_EMIT statusChanged();
    });
    connect(m_worker, &ScriptWorker::finished, this, [this](quint64 ticket, int documents, bool took) {
        m_pending.remove(ticket);
        m_script.clear();
        m_document.clear();
        m_done = m_total = 0;
        Q_EMIT statusChanged();
        if (m_pending.isEmpty()) {
            Q_EMIT busyChanged();
        }
        Q_EMIT finished(ticket, documents, took);
    });
    m_thread.start();
}

ScriptHost::~ScriptHost()
{
    shutdown(5000);
}

QString ScriptHost::problem()
{
    static QMutex mutex;
    static bool tried = false;
    static QString found;

    QMutexLocker locker(&mutex);
    if (tried) {
        return found;
    }
    tried = true;
    if (pythonPath().isEmpty()) {
        found = i18nc("@info", "Scripts are run by Python 3, which is not installed on this computer.");
    } else if (bwrapPath().isEmpty()) {
        found = i18nc("@info", "Scripts run in a sandbox that the program bubblewrap (bwrap) provides, which is not installed on this computer.");
    } else {
        // Installed is not the same as allowed to make a sandbox: try it out.
        QProcess probe;
        probe.setProcessEnvironment(sandboxEnvironment());
        probe.start(bwrapPath(), sandboxArguments({}) + QStringList{pythonPath(), u"-c"_s, u"pass"_s});
        if (!probe.waitForFinished(StartTimeoutMs) || probe.exitStatus() != QProcess::NormalExit || probe.exitCode() != 0) {
            probe.kill();
            probe.waitForFinished(1000);
            const QString said = lastLines(probe.readAllStandardError(), 3);
            qCInfo(INDEKS_CORE) << "The sandbox for scripts cannot be used:" << said;
            found = said.isEmpty() ? i18nc("@info", "The sandbox that scripts run in cannot be set up on this computer.")
                                   : i18nc("@info %1 is the error message of the sandbox program", "The sandbox that scripts run in cannot be set up on this computer: %1", said);
        }
    }
    return found;
}

void ScriptHost::setTimeLimit(int ms)
{
    s_timeLimitMs = qMax(100, ms);
}

quint64 ScriptHost::submit(ScriptRequest request)
{
    request.ticket = ++m_lastTicket;
    const bool wasBusy = isBusy();
    m_pending.insert(request.ticket);
    if (!wasBusy) {
        Q_EMIT busyChanged();
    }
    Q_EMIT requested(request);
    return request.ticket;
}

quint64 ScriptHost::runScript(const QString &file, const QList<qint64> &documents)
{
    ScriptRequest request;
    request.script = file;
    request.documents = documents;
    return submit(request);
}

quint64 ScriptHost::tryScript(const QString &name, const QString &source, const QList<qint64> &documents)
{
    ScriptRequest request;
    request.script = name;
    request.source = source;
    request.trial = true;
    request.documents = documents;
    return submit(request);
}

quint64 ScriptHost::applyRules(const QList<qint64> &rules, const QList<qint64> &documents)
{
    ScriptRequest request;
    request.rules = rules;
    request.documents = documents;
    return submit(request);
}

void ScriptHost::cancel()
{
    m_cancelUpTo = m_lastTicket;
}

bool ScriptHost::rest(int timeoutMs)
{
    cancel();
    if (!m_thread.isRunning()) {
        return true;
    }
    // Behind whatever is still queued, all of which gives up at once now.
    QElapsedTimer timer;
    timer.start();
    std::atomic_bool closed = false;
    QMetaObject::invokeMethod(m_worker, [worker = m_worker, &closed] {
        worker->close();
        closed = true;
    });
    while (!closed && timer.elapsed() < timeoutMs) {
        QThread::msleep(5);
    }
    if (!closed) {
        // The call still refers to the flag; wait for it whatever it takes rather than
        // leave it a dangling reference. This only happens if the index itself hangs.
        while (!closed) {
            QThread::msleep(5);
        }
        return false;
    }
    return true;
}

bool ScriptHost::shutdown(int timeoutMs)
{
    if (!m_thread.isRunning()) {
        return true;
    }
    cancel();
    QMetaObject::invokeMethod(m_worker, &ScriptWorker::close);
    m_thread.quit();
    return m_thread.wait(timeoutMs);
}

QString ScriptHost::status() const
{
    if (!isBusy()) {
        return {};
    }
    return m_script.isEmpty() ? i18nc("@info:status", "Running scripts") : i18nc("@info:status", "Running the script “%1”", m_script);
}

QString ScriptHost::statusDetail() const
{
    return isBusy() ? m_document : QString();
}

double ScriptHost::progress() const
{
    return isBusy() && m_total > 0 ? double(m_done) / m_total : -1;
}

QueueItem ScriptHost::queueItem() const
{
    QueueItem item;
    item.name = m_script.isEmpty() ? QString() : QString(m_script + ".py"_L1);
    item.location = m_total > 0 ? i18nc("@info a document and how far along the script is", "%1 (%2 of %3)", m_document, m_done + 1, m_total) : QString();
    item.state = QueueItem::RunningScript;
    item.progress = progress();
    return item;
}

}

#include "moc_scripthost.cpp"
