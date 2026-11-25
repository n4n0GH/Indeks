// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "indexing.h"

#include "database.h"
#include "extractor.h"
#include "indeks_core_debug.h"
#include "jsonformat.h"
#include "mailbox.h"
#include "ocr.h"
#include "store.h"
#include "task.h"

#include <KLocalizedString>

#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>

using namespace Qt::StringLiterals;

namespace Indeks::Indexing
{

bool isIgnoredFileName(const QString &name)
{
    return name.endsWith(u'~') || name.endsWith(".part"_L1) || name.endsWith(".crdownload"_L1) || name.endsWith(".tmp"_L1) || name.endsWith(".swp"_L1)
        || name.startsWith(".~lock."_L1) || name.startsWith("~$"_L1) || name == "Thumbs.db"_L1 || name == ".DS_Store"_L1 || name == ".directory"_L1;
}

Reading planReading(const FileJob &job, const Document &existing)
{
    Reading reading;
    // The user works on the copy once there is one, so that is what gets indexed.
    const bool useCopy = existing.isValid() && !existing.localCopy.isEmpty() && QFileInfo::exists(existing.localCopy);
    reading.contentFile = useCopy ? existing.localCopy : job.path;
    reading.name = job.url.fileName().isEmpty() ? QFileInfo(job.path).fileName() : job.url.fileName();
    // Unchanged content that already went through OCR keeps its text; redoing OCR is expensive.
    if (existing.isValid() && existing.ocr == OcrState::Done) {
        reading.keepTextIfHash = existing.hash;
    }
    return reading;
}

namespace
{
QString mimeTypeByName(const QString &name)
{
    static const QMimeDatabase mimeDatabase;
    return isJsonLinesFile(name) ? u"application/json"_s : mimeDatabase.mimeTypeForFile(name, QMimeDatabase::MatchExtension).name();
}
}

Prepared readFile(const Reading &reading, const IndexOptions &options)
{
    Prepared prepared;
    static const QMimeDatabase mimeDatabase;
    if (reading.partLength > 0) {
        // One message out of a mailbox.
        prepared.mimeType = u"message/rfc822"_s;
        prepared.kind = Kind::Email;
        const bool tooLarge = reading.partLength > qint64(options.maxContentSizeMb) * 1024 * 1024;
        if (options.indexContent && !tooLarge) {
            prepared.extracted = extractMessage(Mailbox::readMessage(reading.contentFile, reading.partOffset, reading.partLength));
        }
        return prepared;
    }
    {
        QFile file(reading.contentFile);
        if (file.open(QIODevice::ReadOnly)) {
            prepared.mimeType = mimeDatabase.mimeTypeForFileNameAndData(reading.name, &file).name();
        } else {
            prepared.mimeType = mimeDatabase.mimeTypeForFile(reading.name, QMimeDatabase::MatchExtension).name();
        }
    }
    if (isJsonLinesFile(reading.name)) {
        // No MIME type is registered for the extension, so the content decides, and that is
        // read as HTML as soon as a record quotes some. It is JSON, a record per line.
        prepared.mimeType = u"application/json"_s;
    }
    prepared.kind = kindForFile(reading.name, prepared.mimeType);
    if (reading.lookOnly) {
        prepared.look = Fingerprint::ofFile(reading.contentFile, prepared.kind);
        prepared.looked = true;
        return prepared;
    }
    prepared.hash = hashFile(reading.contentFile);
    prepared.textKept = !prepared.hash.isEmpty() && prepared.hash == reading.keepTextIfHash;
    if (!prepared.textKept) {
        prepared.extracted = extractContent(reading.contentFile, prepared.mimeType, prepared.kind, options);
        // What it looks like goes with its content: unchanged content looks as it did.
        if (Fingerprint::applies(prepared.kind)) {
            prepared.look = Fingerprint::ofFile(reading.contentFile, prepared.kind);
            prepared.looked = true;
        }
    }
    return prepared;
}

Prepared unreadableFile(const Reading &reading)
{
    Prepared prepared;
    prepared.mimeType = mimeTypeByName(reading.name);
    prepared.kind = kindForFile(reading.name, prepared.mimeType);
    prepared.hash = hashFile(reading.contentFile);
    prepared.unreadable = true;
    return prepared;
}

Result storeFile(const FileJob &job, const Document &existing, const Prepared &prepared, const IndexOptions &options)
{
    Result result;
    const QFileInfo info(job.path);
    const QString key = Store::urlKey(job.url);
    const QString name = job.url.fileName().isEmpty() ? info.fileName() : job.url.fileName();
    const qint64 size = job.originalSize >= 0 ? job.originalSize : info.size();
    const qint64 mtime = job.originalMtime >= 0 ? job.originalMtime : info.lastModified().toSecsSinceEpoch();
    const QString &hash = prepared.hash;
    const Extracted &extracted = prepared.extracted;

    // Decided here rather than taken from the reading: the document may have been matched to
    // this file only after it was read (a file that was moved).
    const bool keepText = existing.isValid() && existing.ocr == OcrState::Done && !hash.isEmpty() && existing.hash == hash;
    const bool ocrPossible = options.ocrEnabled && options.indexContent && Ocr::isAvailable();
    const OcrState ocr = keepText ? OcrState::Done : (extracted.wantsOcr && ocrPossible) ? OcrState::Pending : OcrState::NotNeeded;

    if (existing.isValid()) {
        result.id = existing.id;
        // A document that was added on its own is adopted by the indexed location that contains it.
        const qint64 sourceId = job.sourceId != 0 ? job.sourceId : existing.sourceId;
        Store::fileReturned(existing.id);
        Database::exec(
            u"UPDATE documents SET source_id = ?, name = ?, mime = ?, kind = ?, size = ?, mtime = ?, hash = ?, missing = 0, ocr = ?, pages = ? WHERE id = ?"_s,
            {sourceId, name, prepared.mimeType, int(prepared.kind), size, mtime, hash, int(ocr), keepText ? existing.pages : extracted.pages, existing.id});
        if (job.sourceId != 0 || job.inbox) {
            // Found in an indexed location, or added by hand: it is here in its own right
            // again, not only for what is attached to it.
            Database::exec(u"UPDATE documents SET leftover = 0 WHERE id = ? AND leftover = 1"_s, {existing.id});
        }
    } else {
        QSqlQuery insert = Database::exec(
            u"INSERT INTO documents (source_id, url, name, mime, kind, size, mtime, added, hash, owned, inbox, ocr, pages) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"_s,
            {job.sourceId, key, name, prepared.mimeType, int(prepared.kind), size, mtime, QDateTime::currentSecsSinceEpoch(), hash, job.owned, job.inbox, int(ocr),
             extracted.pages});
        result.id = insert.lastInsertId().toLongLong();
        result.created = true;
        if (result.id <= 0) {
            qCWarning(INDEKS_CORE) << "Could not add" << job.url << "to the index";
            return Result();
        }
    }

    if (keepText) {
        Store::renameFullText(result.id, name);
    } else {
        Store::writeFullText(result.id, name, extracted.text);
        Database::exec(u"DELETE FROM metadata WHERE doc_id = ?"_s, {result.id});
        for (auto it = extracted.metadata.constBegin(); it != extracted.metadata.constEnd(); ++it) {
            Database::exec(u"INSERT OR REPLACE INTO metadata (doc_id, key, value) VALUES (?, ?, ?)"_s, {result.id, it.key(), it.value()});
        }
    }
    if (prepared.unreadable) {
        // Shown with the document's details, so that it is no riddle why its text is not found.
        Database::exec(u"INSERT OR REPLACE INTO metadata (doc_id, key, value) VALUES (?, 'unreadable', '1')"_s, {result.id});
    }
    if (prepared.looked) {
        Store::setFingerprint(result.id, hash, prepared.look);
    } else if (existing.isValid() && !Fingerprint::applies(prepared.kind)) {
        // It was a picture or a PDF once, under this name, and is something else now.
        Database::exec(u"DELETE FROM fingerprints WHERE doc_id = ?"_s, {result.id});
    }
    result.wantsOcr = ocr == OcrState::Pending;
    return result;
}

qint64 storePart(const Document &parent, const Part &part)
{
    QMap<QString, QString> metadata = part.prepared.extracted.metadata;
    QString name = metadata.value(u"subject"_s).simplified();
    if (name.isEmpty()) {
        name = i18nc("name of an e-mail message that has no subject", "(No subject)");
    }
    // A message is as old as it says; one that does not say is as old as its mailbox.
    bool dated = false;
    qint64 sent = metadata.take(u"emailDateSecs"_s).toLongLong(&dated);
    if (!dated || sent <= 0) {
        sent = parent.modified.toSecsSinceEpoch();
    }
    // Its address holds its number in the index, which is only known once it is in.
    QSqlQuery insert = Database::exec(
        u"INSERT INTO documents (source_id, url, name, mime, kind, size, mtime, added, hash, parent_id, part_offset, part_length) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"_s,
        {parent.sourceId, u"indeks-part:%1/new-%2"_s.arg(parent.id).arg(part.offset), name, part.prepared.mimeType, int(part.prepared.kind), part.length, sent,
         QDateTime::currentSecsSinceEpoch(), part.hash, parent.id, part.offset, part.length});
    const qint64 id = insert.lastInsertId().toLongLong();
    if (id <= 0) {
        return 0;
    }
    Database::exec(u"UPDATE documents SET url = ? WHERE id = ?"_s, {Store::urlKey(Store::partUrl(parent.id, id)), id});
    Store::writeFullText(id, name, part.prepared.extracted.text);
    metadata.remove(u"subject"_s); // that is its name
    for (auto it = metadata.constBegin(); it != metadata.constEnd(); ++it) {
        Database::exec(u"INSERT OR REPLACE INTO metadata (doc_id, key, value) VALUES (?, ?, ?)"_s, {id, it.key(), it.value()});
    }
    return id;
}

Result indexFile(const FileJob &job, const IndexOptions &options)
{
    if (!QFileInfo(job.path).isFile()) {
        return Result();
    }
    const Document existing = Store::documentByUrl(job.url);
    const Prepared prepared = readFile(planReading(job, existing), options);
    if (Task::aborted()) {
        // Whatever was read may be cut short. Leave the index as it is; the next scan sees
        // that this file still has to be read.
        return Result();
    }
    return storeFile(job, existing, prepared, options);
}

Result reindexDocument(qint64 documentId, const IndexOptions &options)
{
    const FileJob job = jobForDocument(documentId);
    return job.path.isEmpty() ? Result() : indexFile(job, options);
}

FileJob jobForDocument(qint64 documentId)
{
    const Document doc = Store::document(documentId);
    const QString path = doc.isValid() ? Store::contentPath(doc) : QString();
    if (path.isEmpty()) {
        return FileJob();
    }
    FileJob job;
    job.url = doc.url;
    job.sourceId = doc.sourceId;
    job.owned = doc.owned;
    if (doc.url.isLocalFile() && QFileInfo(doc.url.toLocalFile()).isFile()) {
        job.path = doc.url.toLocalFile();
    } else {
        // Only a copy or download is at hand; keep what is known about the original.
        job.path = path;
        job.originalSize = doc.size;
        job.originalMtime = doc.modified.toSecsSinceEpoch();
    }
    return job;
}

Result indexNameOnly(const QUrl &url, qint64 sourceId, qint64 size, qint64 mtime, bool inbox)
{
    Result result;
    static const QMimeDatabase mimeDatabase;
    const QString name = url.fileName();
    const QString mimeType = mimeTypeByName(name);
    const Kind kind = kindForFile(name, mimeType);

    const Document existing = Store::documentByUrl(url);
    if (existing.isValid()) {
        result.id = existing.id;
        Store::fileReturned(existing.id);
        Database::exec(u"UPDATE documents SET name = ?, mime = ?, kind = ?, size = ?, mtime = ?, missing = 0 WHERE id = ?"_s,
                       {name, mimeType, int(kind), size, mtime, existing.id});
        Store::renameFullText(existing.id, name);
        return result;
    }
    QSqlQuery insert = Database::exec(u"INSERT INTO documents (source_id, url, name, mime, kind, size, mtime, added, inbox) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"_s,
                                      {sourceId, Store::urlKey(url), name, mimeType, int(kind), size, mtime, QDateTime::currentSecsSinceEpoch(), inbox});
    result.id = insert.lastInsertId().toLongLong();
    if (result.id <= 0) {
        return Result();
    }
    result.created = true;
    Store::writeFullText(result.id, name, QString());
    return result;
}

bool runOcr(qint64 documentId, const IndexOptions &options, const QString &pathOverride)
{
    const Document doc = Store::document(documentId);
    if (!doc.isValid() || doc.ocr != OcrState::Pending) {
        return true;
    }
    const QString path = pathOverride.isEmpty() ? Store::contentPath(doc) : pathOverride;
    if (path.isEmpty()) {
        return false;
    }

    bool ok = false;
    QString text;
    if (doc.kind == Kind::Pdf) {
        text = Ocr::recognizePdfFile(path, options.ocrLanguages, 200, options.ocrJobs, &ok);
    } else {
        text = Ocr::recognizeImageFile(path, options.ocrLanguages, &ok);
    }

    if (Task::aborted()) {
        return false; // stays pending and is picked up again
    }
    // The document may have been removed while tesseract was running.
    if (!Store::document(documentId).isValid()) {
        return true;
    }
    Transaction transaction;
    Store::appendFullText(documentId, text);
    Database::exec(u"UPDATE documents SET ocr = ? WHERE id = ?"_s, {int(ok ? OcrState::Done : OcrState::Failed), documentId});
    transaction.commit();
    return true;
}

QList<qint64> runOcrBatch(const QList<qint64> &documentIds, const IndexOptions &options)
{
    struct Item {
        qint64 id = 0;
        QString path;
    };
    QList<Item> items;
    QList<qint64> unreachable;
    for (const qint64 id : documentIds) {
        const Document doc = Store::document(id);
        if (!doc.isValid() || doc.ocr != OcrState::Pending) {
            continue;
        }
        const QString path = Store::contentPath(doc);
        if (path.isEmpty()) {
            unreachable.append(id);
        } else {
            items.append({id, path});
        }
    }

    const QList<Ocr::Recognized> recognized = Ocr::recognizeAll(
        int(items.size()),
        [&items](int index) {
            return Ocr::readImageFile(items.at(index).path);
        },
        options.ocrLanguages,
        options.ocrJobs);
    if (Task::aborted()) {
        return unreachable; // everything stays pending and is picked up again
    }

    Transaction transaction;
    for (qsizetype i = 0; i < items.size(); ++i) {
        const qint64 id = items.at(i).id;
        // A document may have been removed while tesseract was running.
        if (!Store::document(id).isValid()) {
            continue;
        }
        Store::appendFullText(id, recognized.at(i).text);
        Database::exec(u"UPDATE documents SET ocr = ? WHERE id = ?"_s, {int(recognized.at(i).ok ? OcrState::Done : OcrState::Failed), id});
    }
    transaction.commit();
    return unreachable;
}

}
