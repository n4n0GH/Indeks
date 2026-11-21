// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "maintenance.h"

#include "database.h"
#include "indeks_core_debug.h"
#include "paths.h"
#include "task.h"

#include <QFileInfo>

using namespace Qt::StringLiterals;

namespace Indeks::Maintenance
{

namespace
{
// Free space is worth reclaiming from this much on, if it is a good part of the file.
constexpr qint64 WorthwhileBytes = 64LL * 1024 * 1024;
// So many removed documents leave enough dead words in the full text index to bother.
constexpr int WorthwhileRemovals = 2000;
// Pages of work per step. Small enough for a step to be over in a moment.
constexpr int MergeStep = 400;
constexpr int VacuumStep = 4096;
// Turning on incremental vacuuming for an index made without it means writing it out anew
// once. Above this much content that takes too long to do unasked.
constexpr qint64 MaxRewriteBytes = 4LL * 1024 * 1024 * 1024;

qint64 pragma(const QString &name)
{
    return Database::value("PRAGMA "_L1 + name).toLongLong();
}

qint64 indexFileSize()
{
    return QFileInfo(Paths::databaseFile()).size() + QFileInfo(Paths::databaseFile() + "-wal"_L1).size();
}
}

void noteRemoval()
{
    Database::exec(u"INSERT INTO meta (key, value) VALUES ('removed', 1) ON CONFLICT(key) DO UPDATE SET value = value + 1"_s);
}

bool isDue()
{
    const qint64 pageSize = pragma(u"page_size"_s);
    const qint64 pages = pragma(u"page_count"_s);
    const qint64 freePages = pragma(u"freelist_count"_s);
    if (freePages * pageSize >= WorthwhileBytes && freePages * 4 >= pages) {
        return true;
    }
    const qint64 removed = Database::value(u"SELECT value FROM meta WHERE key = 'removed'"_s).toLongLong();
    const qint64 documents = Database::value(u"SELECT count(*) FROM documents"_s).toLongLong();
    return removed >= WorthwhileRemovals && removed * 4 >= documents;
}

qint64 tidy()
{
    const qint64 before = indexFileSize();

    // The full text index first: what it lets go of becomes free pages for the next step.
    if (Database::value(u"SELECT count(*) FROM documents"_s).toLongLong() == 0) {
        Database::exec(u"INSERT INTO doc_fts (doc_fts) VALUES ('delete-all')"_s);
    } else {
        // A negative amount asks for everything to end up in one segment; the calls after it
        // carry on with that until one of them finds nothing left to do.
        Database::exec(u"INSERT INTO doc_fts (doc_fts, rank) VALUES ('merge', ?)"_s, {-MergeStep});
        while (!Task::aborted()) {
            const qint64 changesBefore = Database::value(u"SELECT total_changes()"_s).toLongLong();
            Database::exec(u"INSERT INTO doc_fts (doc_fts, rank) VALUES ('merge', ?)"_s, {MergeStep});
            if (Database::value(u"SELECT total_changes()"_s).toLongLong() - changesBefore < 2) {
                break;
            }
        }
    }
    if (Task::aborted()) {
        return 0;
    }

    if (pragma(u"auto_vacuum"_s) == 2) {
        // Hands free pages back a few at a time.
        while (!Task::aborted() && pragma(u"freelist_count"_s) > 0) {
            QSqlQuery step = Database::exec(u"PRAGMA incremental_vacuum(%1)"_s.arg(VacuumStep));
            while (step.next()) { }
        }
    } else {
        // An index from before incremental vacuuming was switched on: writing it out anew
        // once both shrinks it and makes the cheap way available from then on.
        const qint64 content = (pragma(u"page_count"_s) - pragma(u"freelist_count"_s)) * pragma(u"page_size"_s);
        if (content <= MaxRewriteBytes) {
            Database::exec(u"PRAGMA auto_vacuum = INCREMENTAL"_s);
            Database::exec(u"VACUUM"_s);
        } else {
            qCInfo(INDEKS_CORE) << "The index is too large to be rewritten unasked; its free space stays for now";
        }
    }
    if (!Task::aborted()) {
        QSqlQuery checkpoint = Database::exec(u"PRAGMA wal_checkpoint(TRUNCATE)"_s);
        while (checkpoint.next()) { }
        Database::exec(u"INSERT OR REPLACE INTO meta (key, value) VALUES ('removed', 0)"_s);
    }
    return qMax<qint64>(0, before - indexFileSize());
}

}
