// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mailbox.h"

#include "paths.h"
#include "task.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include <cstring>

using namespace Qt::StringLiterals;

namespace Indeks::Mailbox
{

namespace
{
constexpr qint64 BufferSize = 1024 * 1024;
// A "line" longer than this is not text; it is cut so that the buffer stays what it is.
constexpr qint64 MaxCarry = 8 * BufferSize;
// No message is read whole that is larger than this; it would be an archive in disguise.
constexpr qint64 MaxMessageBytes = 256LL * 1024 * 1024;

/**
 * Whether the line is one that separates messages: "From sender Thu Jan  1 00:00:00 1970".
 * Text that happens to begin with "From " has no time of day in it; well-behaved programs
 * mark such lines with ">" anyway, but not all do.
 */
bool isSeparator(const char *line, qsizetype length)
{
    if (length < 12 || std::memcmp(line, "From ", 5) != 0) {
        return false;
    }
    for (qsizetype i = 6; i + 3 < length; ++i) {
        if (line[i] == ':' && std::isdigit(uchar(line[i - 1])) && std::isdigit(uchar(line[i + 1])) && std::isdigit(uchar(line[i + 2]))) {
            return true;
        }
    }
    return false;
}
}

QList<Entry> scan(const QString &path, const std::function<void(double)> &progress)
{
    QList<Entry> entries;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return entries;
    }
    const qint64 size = file.size();

    QCryptographicHash hash(QCryptographicHash::Sha1);
    bool open = false; // inside a message
    qint64 messageStart = 0;
    auto closeMessage = [&](qint64 end) {
        if (open && end > messageStart) {
            entries.append({messageStart, end - messageStart, hash.result().toHex()});
        }
        hash.reset();
        open = false;
    };

    QByteArray data; // what is being looked at: whole lines, but for the end of the file
    qint64 dataOffset = 0; // where `data` begins in the file
    bool previousBlank = true; // the beginning of the file counts as one
    bool first = true;

    while (!file.atEnd() && !Task::aborted()) {
        const QByteArray chunk = file.read(BufferSize);
        if (chunk.isEmpty()) {
            break;
        }
        data += chunk;
        // Up to the last line that is complete; the rest waits for more.
        qsizetype usable = data.lastIndexOf('\n') + 1;
        if (file.atEnd() || (usable == 0 && data.size() >= MaxCarry)) {
            usable = data.size();
        }
        if (usable == 0) {
            continue;
        }

        const char *bytes = data.constData();
        qsizetype segment = 0; // from here on not yet given to the hash
        qsizetype line = 0;
        while (line < usable) {
            const char *newline = static_cast<const char *>(std::memchr(bytes + line, '\n', usable - line));
            const qsizetype next = newline ? qsizetype(newline - bytes) + 1 : usable;
            qsizetype length = next - line - (newline ? 1 : 0);
            if (length > 0 && bytes[line + length - 1] == '\r') {
                --length;
            }
            if (bytes[line] == 'F' && previousBlank && isSeparator(bytes + line, length)) {
                if (open) {
                    hash.addData(QByteArrayView(bytes + segment, line - segment));
                }
                closeMessage(dataOffset + line);
                open = true;
                messageStart = dataOffset + next;
                segment = next;
            } else if (first) {
                // Not a mailbox by its first line: one message, all of the file.
                open = true;
                messageStart = 0;
            }
            first = false;
            previousBlank = length == 0;
            line = next;
        }
        if (open) {
            hash.addData(QByteArrayView(bytes + segment, usable - segment));
        }
        dataOffset += usable;
        data.remove(0, usable);
        if (progress && size > 0) {
            progress(double(dataOffset) / double(size));
        }
    }
    if (!Task::aborted()) {
        closeMessage(size);
    }
    return entries;
}

QByteArray readMessage(const QString &path, qint64 offset, qint64 length)
{
    QFile file(path);
    if (length <= 0 || length > MaxMessageBytes || !file.open(QIODevice::ReadOnly) || offset < 0 || offset + length > file.size() || !file.seek(offset)) {
        return {};
    }
    const QByteArray raw = file.read(length);
    if (!raw.contains("\n>")) {
        return raw;
    }
    // ">From " and ">>From " lose the mark they were given on the way in.
    QByteArray message;
    message.reserve(raw.size());
    qsizetype line = 0;
    while (line < raw.size()) {
        qsizetype next = raw.indexOf('\n', line);
        next = next < 0 ? raw.size() : next + 1;
        qsizetype marks = 0;
        while (line + marks < next && raw.at(line + marks) == '>') {
            ++marks;
        }
        const bool quoted = marks > 0 && next - line - marks >= 5 && std::memcmp(raw.constData() + line + marks, "From ", 5) == 0;
        message.append(raw.constData() + line + (quoted ? 1 : 0), next - line - (quoted ? 1 : 0));
        line = next;
    }
    return message;
}

bool writeMessage(const QString &path, qint64 offset, qint64 length, const QString &target)
{
    const QByteArray message = readMessage(path, offset, length);
    if (message.isEmpty() || !QDir().mkpath(QFileInfo(target).path())) {
        return false;
    }
    QSaveFile file(target);
    return file.open(QIODevice::WriteOnly) && file.write(message) == message.size() && file.commit();
}

void pruneCache(int days)
{
    const QDateTime limit = QDateTime::currentDateTime().addDays(-days);
    const QFileInfoList files = QDir(Paths::cacheDir() + "/parts"_L1).entryInfoList({u"*.eml"_s}, QDir::Files);
    for (const QFileInfo &info : files) {
        if (info.lastRead() < limit && info.lastModified() < limit) {
            QFile::remove(info.filePath());
        }
    }
}

}
