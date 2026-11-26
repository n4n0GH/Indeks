// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "database.h"

#include "databasefolder.h"
#include "indeks_core_debug.h"
#include "paths.h"

#include <QAtomicInt>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QThread>

using namespace Qt::StringLiterals;

namespace Indeks
{

static QString connectionName()
{
    return u"indeks-%1"_s.arg(reinterpret_cast<quintptr>(QThread::currentThreadId()), 0, 16);
}

static bool run(QSqlDatabase &db, const QString &sql)
{
    QSqlQuery query(db);
    if (!query.exec(sql)) {
        qCWarning(INDEKS_CORE) << "SQL failed:" << sql << query.lastError().text();
        return false;
    }
    return true;
}

/**
 * Up to version 6 a rule had a fixed set of things it could do, a column for each: tags to
 * add, one group, out of the inbox, flag. Version 7 keeps a list of actions instead, of any
 * number and kind. The rules there are get theirs from the old columns.
 */
static bool upgradeRules(QSqlDatabase &db)
{
    struct Rule {
        qint64 id;
        QString name;
        QString query;
        bool enabled;
        bool onImport;
        QJsonArray actions;
    };
    QList<Rule> rules;
    {
        QSqlQuery select(db);
        if (!select.exec(u"SELECT id, name, query, enabled, on_import, add_tags, COALESCE(folder_id, 0), clear_inbox, set_flag FROM rules"_s)) {
            qCWarning(INDEKS_CORE) << "Cannot read the rules to carry them over:" << select.lastError().text();
            return false;
        }
        while (select.next()) {
            Rule rule{select.value(0).toLongLong(), select.value(1).toString(), select.value(2).toString(), select.value(3).toBool(), select.value(4).toBool(), {}};
            const QStringList tags = select.value(5).toString().split(u',', Qt::SkipEmptyParts);
            for (const QString &tag : tags) {
                if (!tag.trimmed().isEmpty()) {
                    rule.actions.append(QJsonObject{{u"type"_s, u"tag"_s}, {u"name"_s, tag.trimmed()}});
                }
            }
            if (select.value(6).toLongLong() > 0) {
                rule.actions.append(QJsonObject{{u"type"_s, u"group"_s}, {u"id"_s, select.value(6).toLongLong()}});
            }
            if (select.value(7).toBool()) {
                rule.actions.append(QJsonObject{{u"type"_s, u"inbox"_s}});
            }
            if (select.value(8).toBool()) {
                rule.actions.append(QJsonObject{{u"type"_s, u"flag"_s}});
            }
            rules.append(rule);
        }
    }
    if (!run(db, u"DROP TABLE rules"_s)
        || !run(db,
                u"CREATE TABLE rules (id INTEGER PRIMARY KEY, name TEXT NOT NULL, query TEXT NOT NULL, enabled INTEGER NOT NULL DEFAULT 1, "
                "on_import INTEGER NOT NULL DEFAULT 1, filters TEXT NOT NULL DEFAULT '[]', actions TEXT NOT NULL DEFAULT '[]')"_s)) {
        return false;
    }
    for (const Rule &rule : std::as_const(rules)) {
        QSqlQuery insert(db);
        insert.prepare(u"INSERT INTO rules (id, name, query, enabled, on_import, actions) VALUES (?, ?, ?, ?, ?, ?)"_s);
        insert.addBindValue(rule.id);
        insert.addBindValue(rule.name);
        insert.addBindValue(rule.query);
        insert.addBindValue(rule.enabled);
        insert.addBindValue(rule.onImport);
        insert.addBindValue(QString::fromUtf8(QJsonDocument(rule.actions).toJson(QJsonDocument::Compact)));
        if (!insert.exec()) {
            qCWarning(INDEKS_CORE) << "Cannot carry a rule over:" << insert.lastError().text();
            return false;
        }
    }
    return true;
}

Database::FileState Database::classify(QSqlDatabase &db)
{
    FileState state;
    auto number = [&db](const QString &sql, int *result) {
        QSqlQuery query(db);
        if (!query.exec(sql) || !query.next()) {
            return false;
        }
        *result = query.value(0).toInt();
        return true;
    };
    int applicationId = 0;
    int tables = 0;
    if (!number(u"PRAGMA application_id"_s, &applicationId) || !number(u"PRAGMA user_version"_s, &state.version)
        || !number(u"SELECT COUNT(*) FROM sqlite_master"_s, &tables)) {
        state.kind = FileKind::Unreadable;
        return state;
    }
    if (applicationId == ApplicationId) {
        state.kind = FileKind::Own;
    } else if (applicationId != 0) {
        state.kind = FileKind::Foreign;
    } else if (tables == 0) {
        state.kind = FileKind::Blank;
    } else {
        // An index from before Indeks marked its files: known by the tables it always had.
        int own = 0;
        number(u"SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name IN ('documents', 'sources')"_s, &own);
        state.kind = own == 2 ? FileKind::Own : FileKind::Foreign;
    }
    return state;
}

Database::FileState Database::inspectFile(const QString &file)
{
    FileState state;
    QFile raw(file);
    if (!QFileInfo(file).isFile()) {
        return state;
    }
    if (raw.size() == 0) {
        state.kind = FileKind::Blank;
        return state;
    }
    // Every SQLite database begins with these words. SQLite itself is only asked once they
    // are there: it would take any file for a database until the first question fails.
    if (!raw.open(QIODevice::ReadOnly)) {
        state.kind = FileKind::Unreadable;
        return state;
    }
    const bool sqlite = raw.read(16) == QByteArray("SQLite format 3\0", 16);
    raw.close();
    if (!sqlite) {
        state.kind = FileKind::Foreign;
        return state;
    }

    static QAtomicInt counter;
    const QString name = u"indeks-probe-%1"_s.arg(counter.fetchAndAddRelaxed(1));
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(u"QSQLITE"_s, name);
        db.setDatabaseName(file);
        db.setConnectOptions(u"QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=3000"_s);
        if (db.open()) {
            state = classify(db);
            db.close();
        } else {
            state.kind = FileKind::Unreadable;
        }
    }
    QSqlDatabase::removeDatabase(name);
    return state;
}

QSqlDatabase Database::connection()
{
    const QString name = connectionName();
    if (QSqlDatabase::contains(name)) {
        return QSqlDatabase::database(name);
    }

    QSqlDatabase db = QSqlDatabase::addDatabase(u"QSQLITE"_s, name);
    db.setDatabaseName(Paths::databaseFile());
    db.setConnectOptions(u"QSQLITE_BUSY_TIMEOUT=30000"_s);
    if (!db.open()) {
        qCCritical(INDEKS_CORE) << "Cannot open index database" << db.databaseName() << db.lastError().text();
        return db;
    }
    // A file that is called like the index but is not one is left exactly as it is. This
    // comes before everything below, all of which writes to the file.
    if (classify(db).kind == FileKind::Foreign) {
        qCCritical(INDEKS_CORE) << db.databaseName() << "is not an index of Indeks; it is not touched";
        db.close();
        return db;
    }
    // Lets free pages be handed back to the file system in small steps (see Maintenance). It
    // has to be asked for before anything is written to a new file. Only then: on a file that
    // has it already the request writes to the file's header, on every opening and in the
    // way of whoever is writing at that moment. Older files get it when they are tidied.
    {
        QSqlQuery pages(db);
        if (pages.exec(u"PRAGMA page_count"_s) && pages.next() && pages.value(0).toLongLong() == 0) {
            pages.finish();
            run(db, u"PRAGMA auto_vacuum = INCREMENTAL"_s);
        }
    }
    if (DatabaseFolder::isNetworkStorage(Paths::dataDir())) {
        // Write-ahead logging needs memory shared between everyone who has the file open,
        // which network shares do not provide. The classic journal works there.
        run(db, u"PRAGMA journal_mode = DELETE"_s);
        run(db, u"PRAGMA synchronous = FULL"_s);
    } else {
        run(db, u"PRAGMA journal_mode = WAL"_s);
        run(db, u"PRAGMA synchronous = NORMAL"_s);
    }
    run(db, u"PRAGMA foreign_keys = ON"_s);
    if (!migrate(db)) {
        qCCritical(INDEKS_CORE) << "Cannot set up the index database schema";
    }
    return db;
}

bool Database::isOpen()
{
    return connection().isOpen();
}

void Database::closeConnection()
{
    const QString name = connectionName();
    if (QSqlDatabase::contains(name)) {
        {
            QSqlDatabase db = QSqlDatabase::database(name, false);
            db.close();
        }
        QSqlDatabase::removeDatabase(name);
    }
}

QSqlQuery Database::exec(const QString &sql, const QVariantList &binds)
{
    QSqlQuery query(connection());
    query.setForwardOnly(true);
    if (!query.prepare(sql)) {
        qCWarning(INDEKS_CORE) << "SQL prepare failed:" << sql << query.lastError().text();
        return query;
    }
    for (const QVariant &bind : binds) {
        query.addBindValue(bind);
    }
    if (!query.exec()) {
        qCWarning(INDEKS_CORE) << "SQL failed:" << sql << binds << query.lastError().text();
    }
    return query;
}

QVariant Database::value(const QString &sql, const QVariantList &binds)
{
    QSqlQuery query = exec(sql, binds);
    return query.next() ? query.value(0) : QVariant();
}

namespace
{
// What Indeks works out from the documents and can work out again is kept in tables that are
// no part of the schema's version: they are made where they are missing, in an index of any
// age, and an older Indeks that opens the index afterwards neither knows nor minds them. A
// new version number would shut such an Indeks out for the sake of something it can do
// without.
//
// fingerprints: what a document looks like (see Fingerprint), with the hash of the content
// it was made of, by which one that is out of date is told.
void ensureDerivedTables(QSqlDatabase &db)
{
    // Only what is missing is made: an index that has them all is not written to.
    const auto missing = [&db](const QString &table) {
        QSqlQuery there(db);
        there.prepare(u"SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ?"_s);
        there.addBindValue(table);
        return !(there.exec() && there.next());
    };
    if (missing(u"fingerprints"_s)) {
        run(db,
            u"CREATE TABLE IF NOT EXISTS fingerprints ("
            "doc_id INTEGER PRIMARY KEY REFERENCES documents(id) ON DELETE CASCADE, "
            "hash TEXT NOT NULL DEFAULT '', "
            "shape INTEGER NOT NULL DEFAULT 0, "
            "ink INTEGER NOT NULL DEFAULT 0, "
            "b0 INTEGER NOT NULL DEFAULT 0, b1 INTEGER NOT NULL DEFAULT 0, b2 INTEGER NOT NULL DEFAULT 0, b3 INTEGER NOT NULL DEFAULT 0, "
            "b4 INTEGER NOT NULL DEFAULT 0, b5 INTEGER NOT NULL DEFAULT 0, b6 INTEGER NOT NULL DEFAULT 0, b7 INTEGER NOT NULL DEFAULT 0)"_s);
    }
    if (missing(u"recognized_words"_s)) {
        // The words recognised on the pages of scans, and where they stand; see
        // Store::setRecognizedLayout().
        run(db,
            u"CREATE TABLE IF NOT EXISTS recognized_words ("
            "doc_id INTEGER NOT NULL REFERENCES documents(id) ON DELETE CASCADE, "
            "page INTEGER NOT NULL, "
            "words BLOB NOT NULL, "
            "PRIMARY KEY (doc_id, page)) WITHOUT ROWID"_s);
    }
}
}

bool Database::migrate(QSqlDatabase &db)
{
    QSqlQuery versionQuery(db);
    if (!versionQuery.exec(u"PRAGMA user_version"_s) || !versionQuery.next()) {
        return false;
    }
    const int version = versionQuery.value(0).toInt();
    versionQuery.finish();
    if (version >= SchemaVersion) {
        // An index from before Indeks marked its files gets its mark now. Older versions
        // of Indeks do not look at it, so this takes nothing away from them.
        QSqlQuery marked(db);
        if (marked.exec(u"PRAGMA application_id"_s) && marked.next() && marked.value(0).toInt() != ApplicationId) {
            marked.finish();
            run(db, u"PRAGMA application_id = %1"_s.arg(ApplicationId));
        }
        if (version == SchemaVersion) {
            ensureDerivedTables(db);
        }
        return true;
    }

    static const QStringList schema = {
        u"CREATE TABLE sources ("
        "id INTEGER PRIMARY KEY, "
        "url TEXT NOT NULL UNIQUE, "
        "name TEXT NOT NULL, "
        "recursive INTEGER NOT NULL DEFAULT 1, "
        "enabled INTEGER NOT NULL DEFAULT 1, "
        "last_scan INTEGER NOT NULL DEFAULT 0, "
        "inbox INTEGER NOT NULL DEFAULT 1, "
        "settled INTEGER NOT NULL DEFAULT 0)"_s,

        u"CREATE TABLE documents ("
        "id INTEGER PRIMARY KEY, "
        "source_id INTEGER NOT NULL DEFAULT 0, "
        "url TEXT NOT NULL UNIQUE, "
        "name TEXT NOT NULL, "
        "mime TEXT NOT NULL DEFAULT '', "
        "kind INTEGER NOT NULL DEFAULT 0, "
        "size INTEGER NOT NULL DEFAULT 0, "
        "mtime INTEGER NOT NULL DEFAULT 0, "
        "added INTEGER NOT NULL DEFAULT 0, "
        "hash TEXT NOT NULL DEFAULT '', "
        "local_copy TEXT NOT NULL DEFAULT '', "
        "owned INTEGER NOT NULL DEFAULT 0, "
        "inbox INTEGER NOT NULL DEFAULT 0, "
        "trashed INTEGER NOT NULL DEFAULT 0, "
        "missing INTEGER NOT NULL DEFAULT 0, "
        "leftover INTEGER NOT NULL DEFAULT 0, " // stayed behind when its location was removed
        "flagged INTEGER NOT NULL DEFAULT 0, "
        "comment TEXT NOT NULL DEFAULT '', "
        "pages INTEGER NOT NULL DEFAULT 0, "
        "ocr INTEGER NOT NULL DEFAULT 0)"_s,
        u"CREATE INDEX documents_source ON documents(source_id)"_s,
        u"CREATE INDEX documents_hash ON documents(hash)"_s,
        u"CREATE INDEX documents_added ON documents(added)"_s,

        // name: file name, content: extracted text including OCR, notes: comment and note annotations
        u"CREATE VIRTUAL TABLE doc_fts USING fts5(name, content, notes, tokenize = \"unicode61 remove_diacritics 2\")"_s,

        u"CREATE TABLE metadata ("
        "doc_id INTEGER NOT NULL REFERENCES documents(id) ON DELETE CASCADE, "
        "key TEXT NOT NULL, "
        "value TEXT NOT NULL, "
        "PRIMARY KEY (doc_id, key)) WITHOUT ROWID"_s,

        u"CREATE TABLE tags ("
        "id INTEGER PRIMARY KEY, "
        "name TEXT NOT NULL UNIQUE COLLATE NOCASE)"_s,
        u"CREATE TABLE document_tags ("
        "doc_id INTEGER NOT NULL REFERENCES documents(id) ON DELETE CASCADE, "
        "tag_id INTEGER NOT NULL REFERENCES tags(id) ON DELETE CASCADE, "
        "PRIMARY KEY (doc_id, tag_id)) WITHOUT ROWID"_s,
        u"CREATE INDEX document_tags_tag ON document_tags(tag_id)"_s,

        // "Groups" in the user interface. GROUPS is an SQL keyword, hence the table name.
        u"CREATE TABLE folders ("
        "id INTEGER PRIMARY KEY, "
        "parent_id INTEGER REFERENCES folders(id) ON DELETE CASCADE, "
        "name TEXT NOT NULL)"_s,
        u"CREATE TABLE document_folders ("
        "doc_id INTEGER NOT NULL REFERENCES documents(id) ON DELETE CASCADE, "
        "folder_id INTEGER NOT NULL REFERENCES folders(id) ON DELETE CASCADE, "
        "PRIMARY KEY (doc_id, folder_id)) WITHOUT ROWID"_s,
        u"CREATE INDEX document_folders_folder ON document_folders(folder_id)"_s,

        u"CREATE TABLE searches ("
        "id INTEGER PRIMARY KEY, "
        "name TEXT NOT NULL, "
        "query TEXT NOT NULL)"_s,

        // filters: the condition as the user put it together, for the editor; query is what
        // it comes to. actions: what to do, in order. Both are JSON lists.
        u"CREATE TABLE rules ("
        "id INTEGER PRIMARY KEY, "
        "name TEXT NOT NULL, "
        "query TEXT NOT NULL, "
        "enabled INTEGER NOT NULL DEFAULT 1, "
        "on_import INTEGER NOT NULL DEFAULT 1, "
        "filters TEXT NOT NULL DEFAULT '[]', "
        "actions TEXT NOT NULL DEFAULT '[]')"_s,

        // page is -1 for annotations that belong to the whole document
        u"CREATE TABLE annotations ("
        "id INTEGER PRIMARY KEY, "
        "doc_id INTEGER NOT NULL REFERENCES documents(id) ON DELETE CASCADE, "
        "page INTEGER NOT NULL DEFAULT -1, "
        "kind TEXT NOT NULL, "
        "data TEXT NOT NULL, "
        "created INTEGER NOT NULL, "
        "modified INTEGER NOT NULL)"_s,
        u"CREATE INDEX annotations_doc ON annotations(doc_id, page)"_s,
    };

    if (!db.transaction()) {
        // Another thread or process is migrating right now; it will finish before our busy timeout.
        return false;
    }
    // Read the version again inside the transaction, another writer may have been faster.
    int current = 0;
    {
        QSqlQuery recheck(db);
        if (recheck.exec(u"PRAGMA user_version"_s) && recheck.next()) {
            current = recheck.value(0).toInt();
        }
    }

    QStringList steps;
    if (current < 1) {
        steps += schema;
    }
    if (current < 2) {
        // Version 2 knows Obsidian canvases as a kind of their own; they were plain text before.
        // Forgetting the modification time makes the next scan read them again, so that the
        // index holds the text on the cards instead of raw JSON.
        steps += u"UPDATE documents SET kind = 6, mtime = 0 WHERE name LIKE '%.canvas'"_s;
    }
    if (current < 3) {
        // Version 3 takes JSON Lines files for what their name says. Before, the content
        // decided, and files that quote HTML were indexed as web pages: those are read again.
        steps += u"UPDATE documents SET mtime = 0 WHERE (name LIKE '%.jsonl' OR name LIKE '%.ndjson') AND kind != 1"_s;
        steps += u"UPDATE documents SET mime = 'application/json', kind = 1 WHERE name LIKE '%.jsonl' OR name LIKE '%.ndjson'"_s;
    }
    if (current < 4) {
        // Version 4 notes where the database folder is, to notice when it has been moved.
        steps += u"CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, value TEXT NOT NULL)"_s;
    }
    if (current >= 1 && current < 5) {
        // Version 5 marks documents that stayed behind when their location was removed. A new
        // database has the column from the start.
        steps += u"ALTER TABLE documents ADD COLUMN leftover INTEGER NOT NULL DEFAULT 0"_s;
    }
    if (current >= 1 && current < 6) {
        // Before version 5, documents that stayed behind when their location was removed were
        // not marked, so they could not leave once nothing was attached to them any more.
        // They are recognised by what they are not: in no location, not made in Indeks, not
        // waiting in the inbox. Only those with nothing attached are removed later on.
        steps += u"UPDATE documents SET leftover = 1 WHERE source_id = 0 AND owned = 0 AND inbox = 0 AND trashed = 0"_s;
    }
    if (current < 8) {
        // Version 8 knows documents that are part of another one: the messages of a mailbox
        // file, each with its place in that file. Mailboxes indexed before were one document
        // each; forgetting when they were read has the next scan take them apart.
        steps += u"ALTER TABLE documents ADD COLUMN parent_id INTEGER NOT NULL DEFAULT 0"_s;
        steps += u"ALTER TABLE documents ADD COLUMN part_offset INTEGER NOT NULL DEFAULT 0"_s;
        steps += u"ALTER TABLE documents ADD COLUMN part_length INTEGER NOT NULL DEFAULT 0"_s;
        steps += u"CREATE INDEX documents_parent ON documents(parent_id)"_s;
        steps += u"UPDATE documents SET mtime = 0 WHERE mime = 'application/mbox'"_s;
    }
    if (current < 9) {
        // Version 9 lets a note stand at a place on a page, like a sticky note: where, as
        // shares of the page's width and height. Notes without a place have -1.
        steps += u"ALTER TABLE annotations ADD COLUMN x REAL NOT NULL DEFAULT -1"_s;
        steps += u"ALTER TABLE annotations ADD COLUMN y REAL NOT NULL DEFAULT -1"_s;
    }
    if (current >= 1 && current < 10) {
        // Version 10 puts files that turn up in an indexed location into the inbox, unless
        // that is switched off for the location. What is found on the first pass through a
        // location was there before and is not new: a location is settled once that is done.
        steps += u"ALTER TABLE sources ADD COLUMN inbox INTEGER NOT NULL DEFAULT 1"_s;
        steps += u"ALTER TABLE sources ADD COLUMN settled INTEGER NOT NULL DEFAULT 0"_s;
        steps += u"UPDATE sources SET settled = 1 WHERE last_scan > 0"_s;
    }
    if (current < 11) {
        // Version 11 keeps the user's list of things to do: a line of text each, a day it is
        // due if there is one (as 2026-11-30, or nothing), and the document it is about.
        steps += u"CREATE TABLE todos ("
                 "id INTEGER PRIMARY KEY, "
                 "text TEXT NOT NULL, "
                 "doc_id INTEGER NOT NULL DEFAULT 0, "
                 "due TEXT NOT NULL DEFAULT '', "
                 "done INTEGER NOT NULL DEFAULT 0, "
                 "created INTEGER NOT NULL, "
                 "finished INTEGER NOT NULL DEFAULT 0)"_s;
        steps += u"CREATE INDEX todos_doc ON todos(doc_id)"_s;
    }
    if (current < 12) {
        // Version 12 lets documents be linked to one another, for what belongs together
        // across files. A link has no direction: each pair is kept once, the lower id first,
        // which is what keeps a pair from being there twice.
        steps += u"CREATE TABLE document_links ("
                 "a_id INTEGER NOT NULL REFERENCES documents(id) ON DELETE CASCADE, "
                 "b_id INTEGER NOT NULL REFERENCES documents(id) ON DELETE CASCADE, "
                 "created INTEGER NOT NULL DEFAULT 0, "
                 "PRIMARY KEY (a_id, b_id), CHECK (a_id < b_id)) WITHOUT ROWID"_s;
        steps += u"CREATE INDEX document_links_b ON document_links(b_id)"_s;
    }
    for (const QString &statement : std::as_const(steps)) {
        if (!run(db, statement)) {
            db.rollback();
            return false;
        }
    }
    if (current >= 1 && current < 7 && !upgradeRules(db)) {
        db.rollback();
        return false;
    }
    run(db, u"PRAGMA user_version = %1"_s.arg(SchemaVersion));
    run(db, u"PRAGMA application_id = %1"_s.arg(ApplicationId));
    if (!db.commit()) {
        return false;
    }
    ensureDerivedTables(db);
    return true;
}

namespace
{
// Whether the calling thread is inside a Transaction already; one inside another is part of it.
thread_local bool t_inTransaction = false;
}

Transaction::Transaction()
{
    if (t_inTransaction) {
        return;
    }
    // Taking the write lock at once, and waiting for it if need be. A transaction that starts
    // out reading cannot turn to writing after another thread wrote in between; SQLite then
    // refuses on the spot, and indexer and user interface both write all the time.
    QSqlQuery begin(Database::connection());
    m_active = begin.exec(u"BEGIN IMMEDIATE"_s);
    if (m_active) {
        t_inTransaction = true;
    } else {
        qCWarning(INDEKS_CORE) << "Cannot begin a transaction:" << begin.lastError().text();
    }
}

Transaction::~Transaction()
{
    if (m_active) {
        t_inTransaction = false;
        QSqlQuery rollback(Database::connection());
        rollback.exec(u"ROLLBACK"_s);
    }
}

bool Transaction::commit()
{
    if (!m_active) {
        return false;
    }
    m_active = false;
    t_inTransaction = false;
    QSqlQuery commit(Database::connection());
    if (!commit.exec(u"COMMIT"_s)) {
        qCWarning(INDEKS_CORE) << "Cannot commit:" << commit.lastError().text();
        return false;
    }
    return true;
}

}
