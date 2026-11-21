// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "store.h"

#include "database.h"
#include "mailbox.h"
#include "maintenance.h"
#include "paths.h"

#include <QDir>
#include <QFileInfo>

using namespace Qt::StringLiterals;

namespace Indeks::Store
{

QString urlKey(const QUrl &url)
{
    return url.adjusted(QUrl::NormalizePathSegments).toString(QUrl::FullyEncoded);
}

QString documentColumns()
{
    return u"d.id, d.source_id, d.url, d.name, d.mime, d.kind, d.size, d.mtime, d.added, d.hash, d.local_copy, "
           "d.owned, d.inbox, d.trashed, d.missing, d.flagged, d.comment, d.pages, d.ocr, d.parent_id, d.part_offset, d.part_length"_s;
}

Document readDocument(const QSqlQuery &query)
{
    Document doc;
    doc.id = query.value(0).toLongLong();
    doc.sourceId = query.value(1).toLongLong();
    doc.url = QUrl(query.value(2).toString());
    doc.name = query.value(3).toString();
    doc.mimeType = query.value(4).toString();
    doc.kind = Kind(query.value(5).toInt());
    doc.size = query.value(6).toLongLong();
    doc.modified = QDateTime::fromSecsSinceEpoch(query.value(7).toLongLong());
    doc.added = QDateTime::fromSecsSinceEpoch(query.value(8).toLongLong());
    doc.hash = query.value(9).toString();
    doc.localCopy = query.value(10).toString();
    doc.owned = query.value(11).toBool();
    doc.inbox = query.value(12).toBool();
    doc.trashed = query.value(13).toBool();
    doc.missing = query.value(14).toBool();
    doc.flagged = query.value(15).toBool();
    doc.comment = query.value(16).toString();
    doc.pages = query.value(17).toInt();
    doc.ocr = OcrState(query.value(18).toInt());
    doc.parentId = query.value(19).toLongLong();
    doc.partOffset = query.value(20).toLongLong();
    doc.partLength = query.value(21).toLongLong();
    return doc;
}

Document document(qint64 id)
{
    QSqlQuery query = Database::exec(u"SELECT %1 FROM documents d WHERE d.id = ?"_s.arg(documentColumns()), {id});
    return query.next() ? readDocument(query) : Document();
}

Document documentByUrl(const QUrl &url)
{
    QSqlQuery query = Database::exec(u"SELECT %1 FROM documents d WHERE d.url = ?"_s.arg(documentColumns()), {urlKey(url)});
    return query.next() ? readDocument(query) : Document();
}

static Source readSource(const QSqlQuery &query)
{
    Source source;
    source.id = query.value(0).toLongLong();
    source.url = QUrl(query.value(1).toString());
    source.name = query.value(2).toString();
    source.recursive = query.value(3).toBool();
    source.enabled = query.value(4).toBool();
    const qint64 lastScan = query.value(5).toLongLong();
    if (lastScan > 0) {
        source.lastScan = QDateTime::fromSecsSinceEpoch(lastScan);
    }
    source.inbox = query.value(6).toBool();
    source.settled = query.value(7).toBool();
    return source;
}

Source source(qint64 id)
{
    QSqlQuery query = Database::exec(u"SELECT id, url, name, recursive, enabled, last_scan, inbox, settled FROM sources WHERE id = ?"_s, {id});
    return query.next() ? readSource(query) : Source();
}

QList<Source> sources()
{
    QList<Source> result;
    QSqlQuery query = Database::exec(u"SELECT id, url, name, recursive, enabled, last_scan, inbox, settled FROM sources ORDER BY name COLLATE NOCASE"_s);
    while (query.next()) {
        result.append(readSource(query));
    }
    return result;
}

QString copyDir(qint64 id)
{
    return Paths::copiesDir() + u'/' + QString::number(id);
}

QUrl partUrl(qint64 parentId, qint64 id)
{
    // Not the address of a file: nothing that goes by where files are takes it for one.
    return QUrl(u"indeks-part:%1/%2"_s.arg(parentId).arg(id));
}

QList<qint64> parts(qint64 parentId)
{
    QList<qint64> ids;
    QSqlQuery query = Database::exec(u"SELECT id FROM documents WHERE parent_id = ?"_s, {parentId});
    while (query.next()) {
        ids.append(query.value(0).toLongLong());
    }
    return ids;
}

int partCount(qint64 parentId)
{
    return Database::value(u"SELECT COUNT(*) FROM documents WHERE parent_id = ? AND trashed = 0"_s, {parentId}).toInt();
}

QString cachePath(const Document &document)
{
    if (document.isPart()) {
        // By number, whatever the message is called: a subject is no file name.
        return Paths::cacheDir() + "/parts/"_L1 + QString::number(document.id) + ".eml"_L1;
    }
    return Paths::cacheDir() + "/remote/"_L1 + QString::number(document.id) + u'/' + document.name;
}

QString ensureContent(const Document &document)
{
    const QString path = contentPath(document);
    if (!path.isEmpty() || !document.isPart()) {
        return path;
    }
    const QString mailbox = contentPath(Store::document(document.parentId));
    const QString target = cachePath(document);
    return !mailbox.isEmpty() && Mailbox::writeMessage(mailbox, document.partOffset, document.partLength, target) ? target : QString();
}

QString contentPath(const Document &document)
{
    if (!document.localCopy.isEmpty() && QFileInfo::exists(document.localCopy)) {
        return document.localCopy;
    }
    if (document.isPart()) {
        // Taken out of the file it is part of before, and that file has not changed since.
        const QFileInfo cached(cachePath(document));
        const Document parent = Store::document(document.parentId);
        return cached.isFile() && cached.lastModified() >= parent.modified ? cached.filePath() : QString();
    }
    if (document.url.isLocalFile()) {
        const QString path = document.url.toLocalFile();
        return QFileInfo(path).isFile() ? path : QString();
    }
    const QFileInfo cached(cachePath(document));
    // A cached download older than the indexed version is stale.
    if (cached.isFile() && cached.lastModified() >= document.modified) {
        return cached.filePath();
    }
    return {};
}

void writeFullText(qint64 id, const QString &name, const QString &content)
{
    const QString notes = Database::value(u"SELECT notes FROM doc_fts WHERE rowid = ?"_s, {id}).toString();
    Database::exec(u"DELETE FROM doc_fts WHERE rowid = ?"_s, {id});
    Database::exec(u"INSERT INTO doc_fts (rowid, name, content, notes) VALUES (?, ?, ?, ?)"_s, {id, name, content, notes});
}

void appendFullText(qint64 id, const QString &text)
{
    if (text.isEmpty()) {
        return;
    }
    QSqlQuery query = Database::exec(u"SELECT name, content, notes FROM doc_fts WHERE rowid = ?"_s, {id});
    if (!query.next()) {
        return;
    }
    const QString name = query.value(0).toString();
    QString content = query.value(1).toString();
    const QString notes = query.value(2).toString();
    query.finish();
    content = content.isEmpty() ? text : content + "\n\n"_L1 + text;
    Database::exec(u"DELETE FROM doc_fts WHERE rowid = ?"_s, {id});
    Database::exec(u"INSERT INTO doc_fts (rowid, name, content, notes) VALUES (?, ?, ?, ?)"_s, {id, name, content, notes});
}

void renameFullText(qint64 id, const QString &name)
{
    Database::exec(u"UPDATE doc_fts SET name = ? WHERE rowid = ?"_s, {name, id});
}

void refreshNotes(qint64 id)
{
    QStringList parts;
    const QString comment = Database::value(u"SELECT comment FROM documents WHERE id = ?"_s, {id}).toString();
    if (!comment.isEmpty()) {
        parts.append(comment);
    }
    QSqlQuery query = Database::exec(u"SELECT data FROM annotations WHERE doc_id = ? AND kind = 'note' ORDER BY id"_s, {id});
    while (query.next()) {
        parts.append(query.value(0).toString());
    }
    query.finish();
    Database::exec(u"UPDATE doc_fts SET notes = ? WHERE rowid = ?"_s, {parts.join(u'\n'), id});
}

QString fullText(qint64 id)
{
    return Database::value(u"SELECT content FROM doc_fts WHERE rowid = ?"_s, {id}).toString();
}

static bool hasOwnUserData(qint64 id);

bool hasUserData(qint64 id)
{
    if (hasOwnUserData(id)) {
        return true;
    }
    // What is attached to its parts is attached to the document. In one question: a mailbox
    // has a great many parts.
    return Database::value(u"SELECT EXISTS (SELECT 1 FROM documents c WHERE c.parent_id = ? AND (c.comment <> '' OR c.flagged = 1 "
                           "OR EXISTS (SELECT 1 FROM document_tags WHERE doc_id = c.id) "
                           "OR EXISTS (SELECT 1 FROM document_folders WHERE doc_id = c.id) "
                           "OR EXISTS (SELECT 1 FROM annotations WHERE doc_id = c.id) "
                           "OR EXISTS (SELECT 1 FROM todos WHERE doc_id = c.id AND done = 0)))"_s,
                           {id})
        .toBool();
}

static bool hasOwnUserData(qint64 id)
{
    // A working copy is the user's work only where Indeks can change it, which is text. The
    // copy of a PDF or a picture is made to draw on and is the same file as the original: once
    // the drawings are gone there is nothing of the user's in it.
    return Database::value(u"SELECT (d.local_copy <> '' AND (d.kind = 1 OR d.mime IN ('text/html', 'application/xhtml+xml'))) "
                           "OR d.comment <> '' OR d.flagged = 1 "
                           "OR EXISTS (SELECT 1 FROM document_tags WHERE doc_id = d.id) "
                           "OR EXISTS (SELECT 1 FROM document_folders WHERE doc_id = d.id) "
                           "OR EXISTS (SELECT 1 FROM annotations WHERE doc_id = d.id) "
                           // Something that is still to be done about it.
                           "OR EXISTS (SELECT 1 FROM todos WHERE doc_id = d.id AND done = 0) "
                           "FROM documents d WHERE d.id = ?"_s,
                           {id})
        .toBool();
}

void removeDocument(qint64 id)
{
    const Document doc = document(id);
    if (!doc.isValid()) {
        return;
    }
    // What was to be done about it stays on the list, without a document from here on.
    Database::exec(u"UPDATE todos SET doc_id = 0 WHERE doc_id = ? OR doc_id IN (SELECT id FROM documents WHERE parent_id = ?)"_s, {id, id});
    // A mailbox has many parts; they go in two statements, not two for each.
    const QList<qint64> children = parts(id);
    if (!children.isEmpty()) {
        Database::exec(u"DELETE FROM doc_fts WHERE rowid IN (SELECT id FROM documents WHERE parent_id = ?)"_s, {id});
        Database::exec(u"DELETE FROM documents WHERE parent_id = ?"_s, {id});
        for (qint64 child : children) {
            QFile::remove(Paths::cacheDir() + "/parts/"_L1 + QString::number(child) + ".eml"_L1);
        }
    }
    Database::exec(u"DELETE FROM doc_fts WHERE rowid = ?"_s, {id});
    Database::exec(u"DELETE FROM documents WHERE id = ?"_s, {id});
    Maintenance::noteRemoval();
    if (doc.isPart()) {
        QFile::remove(cachePath(doc));
    }

    // Both directories are named after the numeric id and live inside Indeks' own data and cache directories.
    QDir(copyDir(id)).removeRecursively();
    QDir(Paths::cacheDir() + "/remote/"_L1 + QString::number(id)).removeRecursively();
}

bool fileVanished(qint64 id)
{
    const int trashed = Database::value(u"SELECT trashed FROM documents WHERE id = ?"_s, {id}).toInt();
    if (trashed == 2) {
        // Emptied from the trash, and only remembered so that a scan would not bring it back.
        removeDocument(id);
        return false;
    }
    if (trashed != 0) {
        Database::exec(u"UPDATE documents SET missing = 1 WHERE id = ? AND missing = 0"_s, {id});
        return false;
    }
    // Missing in this way, 2, tells the trash it is in from the trash the user puts things in.
    Database::exec(u"UPDATE documents SET missing = 2, trashed = 1 WHERE id = ?"_s, {id});
    Database::exec(u"UPDATE documents SET trashed = 1 WHERE parent_id = ? AND trashed = 0"_s, {id});
    return true;
}

void fileReturned(qint64 id)
{
    if (Database::value(u"SELECT missing FROM documents WHERE id = ?"_s, {id}).toInt() == 2) {
        Database::exec(u"UPDATE documents SET trashed = 0 WHERE (id = ? OR parent_id = ?) AND trashed = 1"_s, {id, id});
    }
}

QString idList(const QList<qint64> &ids)
{
    QStringList parts;
    parts.reserve(ids.size());
    for (qint64 id : ids) {
        parts.append(QString::number(id));
    }
    return parts.join(u',');
}

}
