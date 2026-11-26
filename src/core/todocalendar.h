// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QByteArray>
#include <QDate>
#include <QList>
#include <QString>
#include <QUrl>

/**
 * The to-dos that have a deadline, as a calendar file (iCalendar, RFC 5545) that calendar
 * programs can show: one VTODO for each, due on its day.
 *
 * The file is Indeks' to write and nobody's to change: it is put together anew from the
 * to-do list whenever that changes, so what a calendar program wrote into it would be gone
 * with the next change. A to-do that is done or lost its deadline is simply no longer in it.
 */
namespace Indeks::TodoCalendar
{

struct Entry {
    qint64 id = 0;
    QString text;
    QDate due;
    qint64 created = 0; ///< seconds since the epoch
    QString documentName; ///< of the document it is about; empty for none
    QUrl documentUrl;
};

/** What the file is called, in the folder of the database. */
QString fileName();

/**
 * The calendar of @p entries. @p name is what calendar programs call it. @p id tells the
 * to-dos of one database from those of another that have the same numbers; it is part of
 * what each to-do is known by to the calendar programs.
 *
 * The same entries always give the same bytes, so that a file that is written again has
 * not changed unless a to-do has: nothing in it says when it was written.
 */
QByteArray render(const QList<Entry> &entries, const QString &name, const QString &id);

/** Whether @p content is a calendar that render() made. */
bool isOwn(const QByteArray &content);

/**
 * Puts @p calendar at @p path, unless that is what is there already. A file at @p path that
 * Indeks did not write is left as it is. Returns what went wrong, or nothing.
 */
QString write(const QString &path, const QByteArray &calendar);

/** Removes the calendar at @p path, if it is one that Indeks wrote. */
void remove(const QString &path);

}
