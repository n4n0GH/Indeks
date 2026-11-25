// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "searchworker.h"

#include "database.h"
#include "excerpt.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QUrl>

using namespace Indeks;
using namespace Qt::StringLiterals;

namespace
{
// Passages are handed over as they are found, so that the first rows have theirs while the
// text of a large document further down is still being read.
constexpr int ExcerptPortionMs = 60;
}

SearchWorker::SearchWorker(const SearchTickets *tickets, SearchHandover *handover)
    : m_tickets(tickets)
    , m_handover(handover)
{
}

QList<DocumentRow> SearchWorker::fetch(const QString &sql, const QVariantList &binds, int maxRows, bool *truncated)
{
    QList<DocumentRow> rows;
    QSqlQuery select = Database::exec(sql, binds);
    while (select.next()) {
        DocumentRow row;
        row.id = select.value(0).toLongLong();
        row.name = select.value(1).toString();
        row.mime = select.value(2).toString();
        row.kind = select.value(3).toInt();
        row.size = select.value(4).toLongLong();
        row.mtime = select.value(5).toLongLong();
        row.added = select.value(6).toLongLong();
        row.sourceId = select.value(7).toLongLong();
        row.missing = select.value(8).toBool();
        row.flagged = select.value(9).toBool();
        row.inbox = select.value(10).toBool();
        row.tags = select.value(11).toString().split(QChar(31), Qt::SkipEmptyParts);
        row.tags.sort(Qt::CaseInsensitive);
        row.annotated = select.value(12).toBool();
        row.parentId = select.value(13).toLongLong();
        row.parentName = select.value(14).toString();
        row.parts = select.value(15).toInt();
        row.pages = select.value(16).toInt();
        row.comment = select.value(17).toString();
        row.groups = select.value(18).toString().split(QChar(31), Qt::SkipEmptyParts);
        row.groups.sort(Qt::CaseInsensitive);
        row.sender = select.value(19).toString();
        // Where it is: for a file its folder, for a message its mailbox.
        const QUrl url(select.value(20).toString());
        if (row.parentId != 0) {
            row.location = row.parentName;
        } else if (url.isLocalFile()) {
            static const QString home = QDir::homePath();
            row.path = url.toLocalFile();
            const QString folder = QFileInfo(row.path).path();
            row.location = folder == home || folder.startsWith(home + u'/') ? QString(u'~' + folder.mid(home.size())) : folder;
        } else {
            row.location = url.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash).toDisplayString();
        }
        rows.append(row);
    }
    *truncated = rows.size() > maxRows;
    if (*truncated) {
        rows.removeLast();
    }
    return rows;
}

void SearchWorker::search(quint64 ticket, const QString &sql, const QVariantList &binds, int maxRows)
{
    if (ticket != m_tickets->search) {
        return; // the user has typed on in the meantime
    }
    bool truncated = false;
    const QList<DocumentRow> rows = fetch(sql, binds, maxRows, &truncated);
    {
        // If the model is still waiting for this very list, it takes it from here.
        QMutexLocker locker(&m_handover->mutex);
        if (m_handover->awaited == ticket) {
            m_handover->rows = rows;
            m_handover->truncated = truncated;
            m_handover->there = true;
            m_handover->arrived.wakeAll();
            return;
        }
    }
    Q_EMIT found(ticket, rows, truncated);
}

void SearchWorker::excerpts(quint64 ticket, const QList<qint64> &ids, const QStringList &terms)
{
    QHash<qint64, QString> portion;
    QElapsedTimer timer;
    timer.start();
    for (const qint64 id : ids) {
        if (ticket != m_tickets->excerpts) {
            return; // another search by now; nobody waits for these
        }
        const QString text = Database::value(u"SELECT content FROM doc_fts WHERE rowid = ?"_s, {id}).toString();
        portion.insert(id, excerpt(text, terms));
        if (timer.elapsed() >= ExcerptPortionMs) {
            Q_EMIT excerptsReady(ticket, portion, false);
            portion.clear();
            timer.restart();
        }
    }
    Q_EMIT excerptsReady(ticket, portion, true);
}

void SearchWorker::close()
{
    Database::closeConnection();
}

#include "moc_searchworker.cpp"
