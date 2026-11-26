// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "todocalendar.h"

#include <KLocalizedString>

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTimeZone>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace Indeks::TodoCalendar
{

namespace
{
/** What says that a calendar is one of ours; see isOwn(). */
const QByteArray ProductLine = "PRODID:-//n4n0//Indeks//EN"_ba;

/** A text as the value of a property: what would end or split it is escaped. */
QString escaped(const QString &text)
{
    QString result;
    result.reserve(text.size() + 8);
    for (const QChar c : text) {
        switch (c.unicode()) {
        case u'\\':
            result += u"\\\\"_s;
            break;
        case u';':
            result += u"\\;"_s;
            break;
        case u',':
            result += u"\\,"_s;
            break;
        case u'\n':
            result += u"\\n"_s;
            break;
        case u'\r':
            break;
        default:
            // Other control characters have no place in a calendar.
            result += c.unicode() < 0x20 && c != u'\t' ? QChar(u' ') : c;
        }
    }
    return result;
}

/**
 * Adds a line to @p out. A line is 75 bytes at most; what is longer goes on in the next
 * one, which begins with a space. A character of several bytes is never cut in two.
 */
void addLine(QByteArray &out, const QString &line)
{
    const QByteArray bytes = line.toUtf8();
    qsizetype from = 0;
    qsizetype room = 75;
    while (bytes.size() - from > room) {
        qsizetype end = from + room;
        while (end > from + 1 && (uchar(bytes.at(end)) & 0xC0) == 0x80) {
            --end;
        }
        out += bytes.mid(from, end - from) + "\r\n "_ba;
        from = end;
        room = 74;
    }
    out += bytes.mid(from) + "\r\n"_ba;
}

QString stamp(qint64 seconds)
{
    return QDateTime::fromSecsSinceEpoch(qMax<qint64>(0, seconds), QTimeZone::UTC).toString(u"yyyyMMdd'T'HHmmss'Z'"_s);
}

/** Addresses a calendar program can do something with: not the places inside a mailbox. */
bool isAddress(const QUrl &url)
{
    return url.isValid() && !url.scheme().isEmpty() && url.scheme() != "indeks-part"_L1;
}
}

QString fileName()
{
    return u"to-dos.ics"_s;
}

QByteArray render(const QList<Entry> &entries, const QString &name, const QString &id)
{
    QList<Entry> sorted = entries;
    std::sort(sorted.begin(), sorted.end(), [](const Entry &a, const Entry &b) {
        return a.due != b.due ? a.due < b.due : a.id < b.id;
    });

    QByteArray out;
    out += "BEGIN:VCALENDAR\r\nVERSION:2.0\r\n"_ba + ProductLine + "\r\nCALSCALE:GREGORIAN\r\n"_ba;
    if (!name.isEmpty()) {
        addLine(out, u"X-WR-CALNAME:"_s + escaped(name));
    }
    for (const Entry &entry : std::as_const(sorted)) {
        if (!entry.due.isValid() || entry.text.isEmpty()) {
            continue;
        }
        const QString written = stamp(entry.created);
        out += "BEGIN:VTODO\r\n"_ba;
        addLine(out, u"UID:todo-%1@%2.indeks"_s.arg(entry.id).arg(id.isEmpty() ? u"database"_s : id));
        addLine(out, u"DTSTAMP:"_s + written);
        addLine(out, u"CREATED:"_s + written);
        addLine(out, u"SUMMARY:"_s + escaped(entry.text));
        // A day, not a moment: the to-do is due on that day wherever the calendar is read.
        addLine(out, u"DUE;VALUE=DATE:"_s + entry.due.toString(u"yyyyMMdd"_s));
        out += "STATUS:NEEDS-ACTION\r\n"_ba;
        if (!entry.documentName.isEmpty()) {
            QString about = i18nc("@info in a calendar entry: the document a to-do is about", "Document in Indeks: %1", entry.documentName);
            if (isAddress(entry.documentUrl)) {
                about += u'\n' + entry.documentUrl.toDisplayString(QUrl::PreferLocalFile | QUrl::RemoveUserInfo);
                addLine(out, u"URL:"_s + entry.documentUrl.toString(QUrl::FullyEncoded | QUrl::RemoveUserInfo));
            }
            addLine(out, u"DESCRIPTION:"_s + escaped(about));
        }
        out += "END:VTODO\r\n"_ba;
    }
    out += "END:VCALENDAR\r\n"_ba;
    return out;
}

bool isOwn(const QByteArray &content)
{
    return content.startsWith("BEGIN:VCALENDAR"_ba) && content.left(512).contains(ProductLine);
}

QString write(const QString &path, const QByteArray &calendar)
{
    QFile existing(path);
    if (existing.exists()) {
        if (!existing.open(QIODevice::ReadOnly)) {
            return i18nc("@info", "The calendar file “%1” could not be read: %2", path, existing.errorString());
        }
        const QByteArray content = existing.readAll();
        existing.close();
        if (content == calendar) {
            return {};
        }
        if (!content.isEmpty() && !isOwn(content)) {
            return i18nc("@info", "“%1” is not a calendar that Indeks wrote, so it was left as it is. Move it elsewhere to have the to-dos written there.", path);
        }
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(calendar) != calendar.size() || !file.commit()) {
        return i18nc("@info", "The calendar file “%1” could not be written: %2", path, file.errorString());
    }
    return {};
}

void remove(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const bool own = isOwn(file.read(512));
    file.close();
    if (own) {
        file.remove();
    }
}

}
