// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "indexer.h"

#include "database.h"
#include "extractor.h"
#include "indeks_core_debug.h"
#include "indexing.h"
#include "mailbox.h"
#include "maintenance.h"
#include "ocr.h"
#include "paths.h"
#include "store.h"
#include "task.h"

#include <KLocalizedString>

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QHash>
#include <QScopeGuard>
#include <QTimer>

using namespace Qt::StringLiterals;

namespace Indeks
{

namespace
{
constexpr int BatchSize = 50;
// How many messages of a mailbox are handed to the helper processes in one go.
constexpr qsizetype MailboxRound = 2000;
// How much of the progress of reading a mailbox goes to finding its messages.
constexpr double ScanShare = 0.2;
constexpr qint64 ChangeSignalIntervalMs = 1500;
constexpr int MaxListedOcrDocuments = 200;
// How long the index is held for writing at a stretch while files are stored.
constexpr qint64 MaxWriteHoldMs = 600;
// Pictures taken on per round for every process that reads them: enough to keep all of them
// busy, few enough for a round to be over in a few seconds.
constexpr int PicturesPerProcess = 4;
// How many documents are looked at for what they look like before other work gets its turn.
constexpr int LookPortion = 200;
constexpr int LookReportMs = 200;

struct Known {
    qint64 id = 0;
    qint64 size = 0;
    qint64 mtime = 0;
    bool missing = false;
};

/** Whether the place a document is indexed from can be looked at right now. */
bool locationReachable(const Document &doc)
{
    if (!doc.url.isLocalFile()) {
        return false;
    }
    if (doc.sourceId != 0) {
        const Source source = Store::source(doc.sourceId);
        return source.id != 0 && source.isLocal() && QFileInfo(source.url.toLocalFile()).isDir();
    }
    return QFileInfo(doc.url.toLocalFile()).dir().exists();
}

/**
 * If @p path holds the content of a document whose own file has disappeared, the file was moved
 * or renamed: point the document at the new place so that tags and annotations stay with it.
 */
bool adoptMovedDocument(const QString &path, qint64 sourceId, const QString &hash)
{
    if (hash.isEmpty()) {
        return false;
    }
    QList<Document> candidates;
    QSqlQuery query = Database::exec(u"SELECT %1 FROM documents d WHERE d.hash = ? AND d.owned = 0"_s.arg(Store::documentColumns()), {hash});
    while (query.next()) {
        candidates.append(Store::readDocument(query));
    }
    query.finish();

    for (const Document &candidate : std::as_const(candidates)) {
        if (!candidate.url.isLocalFile() || QFileInfo::exists(candidate.url.toLocalFile()) || !locationReachable(candidate)) {
            continue;
        }
        const QUrl url = QUrl::fromLocalFile(path);
        // Gone from where it was and in the trash for it, perhaps: here it is again.
        Store::fileReturned(candidate.id);
        Database::exec(u"UPDATE documents SET url = ?, name = ?, source_id = ?, missing = 0 WHERE id = ?"_s,
                       {Store::urlKey(url), url.fileName(), sourceId != 0 ? sourceId : candidate.sourceId, candidate.id});
        Store::renameFullText(candidate.id, url.fileName());
        return true;
    }
    return false;
}
}

IndexWorker::IndexWorker(std::atomic_bool *abort)
    : m_abort(abort)
{
}

void IndexWorker::setOptions(const IndexOptions &options)
{
    m_options = options;
}

void IndexWorker::notifyChanged(bool force)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (force || now - m_lastChangeSignal > ChangeSignalIntervalMs) {
        m_lastChangeSignal = now;
        Q_EMIT changed();
    }
}

void IndexWorker::flushAdded()
{
    if (!m_added.isEmpty()) {
        Q_EMIT documentsAdded(m_added);
        m_added.clear();
    }
}

void IndexWorker::scanSource(qint64 sourceId)
{
    const Source source = Store::source(sourceId);
    if (source.id == 0 || !source.isLocal()) {
        Q_EMIT jobFinished();
        return;
    }
    scanDirectory(sourceId, source.url.toLocalFile(), true);
}

void IndexWorker::scanDirectory(qint64 sourceId, const QString &directory, bool recursive)
{
    Q_EMIT jobStarted();
    bool ocrWanted = false;
    const auto finish = qScopeGuard([this, &ocrWanted] {
        flushAdded();
        notifyChanged(true);
        Q_EMIT progress(Idle, {}, 0, 0);
        Q_EMIT jobFinished();
        if (ocrWanted && !aborted()) {
            processOcrQueue();
        }
    });

    const Source source = Store::source(sourceId);
    if (source.id == 0 || !source.enabled || !source.isLocal()) {
        return;
    }
    const QString root = QDir::cleanPath(source.url.toLocalFile());
    const QString dir = QDir::cleanPath(directory);
    if (dir != root && !dir.startsWith(root + u'/')) {
        return;
    }
    if (!QFileInfo(root).isDir()) {
        // An unplugged drive or unmounted share. Its documents stay in the index untouched.
        Q_EMIT sourceScanned(sourceId, false);
        return;
    }
    const bool descend = recursive && source.recursive;
    const QString prefix = dir + u'/';

    // What the index knows below this directory.
    QHash<QString, Known> known;
    {
        QSqlQuery query = Database::exec(u"SELECT id, url, size, mtime, missing FROM documents WHERE source_id = ?"_s, {sourceId});
        while (query.next()) {
            const QString path = QUrl(query.value(1).toString()).toLocalFile();
            if (!path.startsWith(prefix)) {
                continue;
            }
            if (!descend && path.indexOf(u'/', prefix.size()) >= 0) {
                continue;
            }
            known.insert(path, {query.value(0).toLongLong(), query.value(2).toLongLong(), query.value(3).toLongLong(), query.value(4).toBool()});
        }
    }

    // What is on disk. Files that did not change drop out of `known` and need no work.
    QList<QueueItem> todo;
    if (QFileInfo(dir).isDir()) {
        QueueItem place;
        place.name = dir == root ? source.name : QDir(dir).dirName();
        place.location = dir;
        Q_EMIT progress(Scanning, place, 0, 0);
        QDir::Filters filters = QDir::Files | QDir::Readable;
        if (m_options.indexHidden) {
            filters |= QDir::Hidden;
        }
        const QString ownData = QDir::cleanPath(Paths::dataDir()) + u'/';
        QDirIterator it(dir, filters, descend ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);
        while (it.hasNext() && !aborted()) {
            const QFileInfo info = it.nextFileInfo();
            const QString path = info.filePath();
            if (Indexing::isIgnoredFileName(info.fileName()) || path.startsWith(ownData)) {
                continue;
            }
            const auto entry = known.constFind(path);
            if (entry != known.constEnd()) {
                const bool unchanged = !entry->missing && entry->size == info.size() && entry->mtime == info.lastModified().toSecsSinceEpoch();
                known.erase(entry);
                if (unchanged) {
                    continue;
                }
            }
            QueueItem item;
            item.name = info.fileName();
            item.location = info.path();
            item.size = info.size();
            todo.append(item);
        }
    }
    if (aborted()) {
        return;
    }

    // Index new and changed files.
    if (!todo.isEmpty()) {
        Q_EMIT batchStarted(todo);
    }
    QList<Indexing::FileJob> jobs;
    jobs.reserve(todo.size());
    for (const QueueItem &item : std::as_const(todo)) {
        Indexing::FileJob job;
        job.path = item.location + u'/' + item.name;
        job.url = QUrl::fromLocalFile(job.path);
        job.sourceId = sourceId;
        // Only counts for files the index has not seen: those that came after the location
        // was first gone through, a scan for example, wait in the inbox to be filed.
        job.inbox = source.newFilesToInbox();
        jobs.append(job);
    }
    ocrWanted = indexJobs(jobs, todo, true);

    // Documents whose file is gone. They are not dropped from the index: they go into the
    // trash, where a file that was deleted by accident is noticed, and return from it should
    // the file come back.
    QStringList vanished;
    Transaction transaction;
    for (auto it = known.constBegin(); it != known.constEnd() && !aborted(); ++it) {
        if (it->missing) {
            continue; // known to be gone, and dealt with when it went
        }
        const Document doc = Store::document(it->id);
        if (!doc.isValid() || doc.url.toLocalFile() != it.key() || QFileInfo::exists(it.key())) {
            continue; // adopted by a moved file in the meantime, or back again
        }
        if (Store::fileVanished(doc.id)) {
            vanished.append(doc.name);
        }
    }

    if (dir == root && !aborted()) {
        Database::exec(u"UPDATE sources SET last_scan = ?, settled = 1 WHERE id = ?"_s, {QDateTime::currentSecsSinceEpoch(), sourceId});
    }
    transaction.commit();
    if (!vanished.isEmpty()) {
        Q_EMIT filesVanished(vanished);
    }
    Q_EMIT sourceScanned(sourceId, true);
}

bool IndexWorker::indexJobs(const QList<Indexing::FileJob> &jobs,
                            const QList<QueueItem> &items,
                            bool adoptMoved,
                            const std::function<void(const Indexing::FileJob &, const Indexing::Result &)> &stored)
{
    // What is to be read of each file depends on what the index knows of it.
    QList<ExtractorPool::Request> requests;
    requests.reserve(jobs.size());
    for (int i = 0; i < jobs.size(); ++i) {
        ExtractorPool::Request request;
        request.id = i;
        request.reading = Indexing::planReading(jobs.at(i), Store::documentByUrl(jobs.at(i).url));
        request.size = items.value(i).size;
        requests.append(request);
    }

    bool ocrWanted = false;
    int done = 0;
    int uncommitted = 0;
    QStringList unreadableNames;
    // Mailboxes among the files: their messages are read once the files themselves are in.
    QList<std::pair<qint64, QueueItem>> mailboxes;
    // The index is only held for writing while there is something to write, and not for long:
    // the user interface writes too.
    std::unique_ptr<Transaction> transaction;
    QElapsedTimer held;
    auto commit = [&] {
        if (transaction) {
            transaction->commit();
            transaction.reset();
            uncommitted = 0;
            flushAdded();
            notifyChanged();
        }
    };

    const auto started = [&](int id) {
        Q_EMIT progress(Reading, items.value(id), done, int(jobs.size()));
    };
    const auto finished = [&](const ExtractorPool::Outcome &outcome) {
        const Indexing::FileJob &job = jobs.at(outcome.id);
        if (!transaction) {
            transaction = std::make_unique<Transaction>();
            held.start();
        }
        Indexing::Prepared prepared = outcome.prepared;
        if (!outcome.ok) {
            // The helper died on this file or never came back from it. It goes in by name, and
            // stays that way until it changes or is read again on request.
            prepared = Indexing::unreadableFile(requests.at(outcome.id).reading);
            unreadableNames.append(requests.at(outcome.id).reading.name);
        }
        Document existing = Store::documentByUrl(job.url);
        bool adopted = false;
        if (adoptMoved && !existing.isValid() && job.url.isLocalFile() && adoptMovedDocument(job.path, job.sourceId, prepared.hash)) {
            existing = Store::documentByUrl(job.url);
            adopted = true;
        }
        const Indexing::Result result = Indexing::storeFile(job, existing, prepared, m_options);
        if (adopted && !existing.localCopy.isEmpty()) {
            // The moved document has a working copy, and that is what its text comes from.
            Indexing::reindexDocument(result.id, m_options);
        }
        if (result.created) {
            m_added.append(result.id);
        }
        if (result.id > 0 && isContainerMimeType(prepared.mimeType)) {
            // Until its messages are in, the mailbox counts as not read: should Indeks be
            // closed in between, the next scan takes it up again.
            Database::exec(u"UPDATE documents SET mtime = 0 WHERE id = ?"_s, {result.id});
            mailboxes.append({result.id, items.value(outcome.id)});
        }
        ocrWanted = ocrWanted || result.wantsOcr;
        if (stored) {
            stored(job, result);
        }
        ++done;
        if (++uncommitted >= BatchSize) {
            commit();
        }
    };
    const auto tick = [&] {
        if (transaction && held.elapsed() > MaxWriteHoldMs) {
            commit();
        }
    };

    const int processes = m_options.indexJobs > 0 ? m_options.indexJobs : ExtractorPool::defaultProcesses();
    m_pool.run(requests, m_options, processes, started, finished, tick);
    commit();
    if (!unreadableNames.isEmpty()) {
        Q_EMIT unreadable(unreadableNames);
    }
    for (const auto &[id, item] : std::as_const(mailboxes)) {
        if (aborted()) {
            break;
        }
        indexMailbox(id, item);
    }
    return ocrWanted;
}

void IndexWorker::indexMailbox(qint64 mailboxId, const QueueItem &item)
{
    const Document mailbox = Store::document(mailboxId);
    const QString path = mailbox.isValid() ? Store::contentPath(mailbox) : QString();
    if (path.isEmpty()) {
        return;
    }
    const QFileInfo info(path);
    // What the file was when it was looked at; written back at the end, which is what marks
    // the mailbox as read.
    const qint64 mtime = mailbox.url.isLocalFile() && mailbox.localCopy.isEmpty() ? info.lastModified().toSecsSinceEpoch() : Database::value(u"SELECT mtime FROM documents WHERE id = ?"_s, {mailboxId}).toLongLong();

    // One pass through the file to see where its messages are. This is the part that takes
    // as long as the file is large; it is given a fifth of the progress bar.
    Q_EMIT progress(Reading, item, 0, 1);
    const QList<Mailbox::Entry> entries = Mailbox::scan(path, [this](double share) {
        Q_EMIT fileProgress(share * ScanShare);
    });
    if (aborted()) {
        return;
    }

    // What the index has of this mailbox, by what each message is made of: a message that
    // moved within the file, because others before it were deleted, is still the same one.
    struct Known {
        qint64 id = 0;
        qint64 offset = 0;
        qint64 length = 0;
        bool missing = false;
    };
    QMultiHash<QString, Known> known;
    {
        QSqlQuery query = Database::exec(u"SELECT id, hash, part_offset, part_length, missing FROM documents WHERE parent_id = ?"_s, {mailboxId});
        while (query.next()) {
            known.insert(query.value(1).toString(), {query.value(0).toLongLong(), query.value(2).toLongLong(), query.value(3).toLongLong(), query.value(4).toBool()});
        }
    }

    QList<Mailbox::Entry> fresh;
    {
        Transaction transaction;
        for (const Mailbox::Entry &entry : entries) {
            const QString hash = QString::fromLatin1(entry.hash);
            const auto it = known.find(hash);
            if (it == known.end()) {
                fresh.append(entry);
                continue;
            }
            if (it->offset != entry.offset || it->length != entry.length || it->missing) {
                Database::exec(u"UPDATE documents SET part_offset = ?, part_length = ?, missing = 0 WHERE id = ?"_s, {entry.offset, entry.length, it->id});
            }
            known.erase(it);
        }
        // Messages that are no longer in the file. Those the user put work into are kept and
        // marked as missing, like files that went away.
        for (const Known &gone : std::as_const(known)) {
            if (Store::hasUserData(gone.id)) {
                Database::exec(u"UPDATE documents SET missing = 1 WHERE id = ?"_s, {gone.id});
            } else {
                Store::removeDocument(gone.id);
            }
        }
        transaction.commit();
    }

    // The new messages, read side by side in the helper processes like so many files. A round
    // at a time, so that neither the list of what is to be read nor what was read of it
    // grows with the mailbox.
    const int processes = m_options.indexJobs > 0 ? m_options.indexJobs : ExtractorPool::defaultProcesses();
    int stored = 0;
    for (qsizetype begin = 0; begin < fresh.size() && !aborted(); begin += MailboxRound) {
        const qsizetype end = qMin(fresh.size(), begin + MailboxRound);
        QList<ExtractorPool::Request> requests;
        requests.reserve(end - begin);
        for (qsizetype i = begin; i < end; ++i) {
            ExtractorPool::Request request;
            request.id = int(i - begin);
            request.reading.contentFile = path;
            request.reading.name = mailbox.name;
            request.reading.partOffset = fresh.at(i).offset;
            request.reading.partLength = fresh.at(i).length;
            request.size = fresh.at(i).length;
            requests.append(request);
        }

        std::unique_ptr<Transaction> transaction;
        QElapsedTimer held;
        int uncommitted = 0;
        auto commit = [&] {
            if (transaction) {
                transaction->commit();
                transaction.reset();
                uncommitted = 0;
                flushAdded();
                notifyChanged();
            }
        };
        const auto finished = [&](const ExtractorPool::Outcome &outcome) {
            if (!transaction) {
                transaction = std::make_unique<Transaction>();
                held.start();
            }
            const Mailbox::Entry &entry = fresh.at(begin + outcome.id);
            Indexing::Part part;
            part.offset = entry.offset;
            part.length = entry.length;
            part.hash = QString::fromLatin1(entry.hash);
            if (outcome.ok) {
                part.prepared = outcome.prepared;
            } else {
                // The helper died on this message. It goes in without its text.
                part.prepared.mimeType = u"message/rfc822"_s;
                part.prepared.kind = Kind::Email;
            }
            const qint64 id = Indexing::storePart(mailbox, part);
            if (id > 0) {
                m_added.append(id);
            }
            ++stored;
            if (++uncommitted >= BatchSize) {
                commit();
            }
            Q_EMIT fileProgress(ScanShare + (1 - ScanShare) * double(stored) / double(fresh.size()));
        };
        const auto tick = [&] {
            if (transaction && held.elapsed() > MaxWriteHoldMs) {
                commit();
            }
        };
        m_pool.run(requests, m_options, processes, {}, finished, tick);
        commit();
    }
    if (aborted()) {
        return; // the mailbox stays marked as not read; what is in already is found again by its content
    }

    Transaction transaction;
    Database::exec(u"UPDATE documents SET mtime = ? WHERE id = ?"_s, {mtime, mailboxId});
    Database::exec(u"INSERT OR REPLACE INTO metadata (doc_id, key, value) VALUES (?, 'emailCount', ?)"_s, {mailboxId, QString::number(entries.size())});
    transaction.commit();
    flushAdded();
    notifyChanged(true);
}

void IndexWorker::indexDownload(qint64 sourceId, const QUrl &url, const QString &downloadPath, qint64 size, qint64 mtime, bool inbox)
{
    Q_EMIT jobStarted();
    const QueueItem item = queueItemForUrl(url, size);
    if (!aborted() && QFileInfo(downloadPath).isFile()) {
        Indexing::Result result;
        Indexing::FileJob job;
        job.path = downloadPath;
        job.url = url;
        job.sourceId = sourceId;
        job.inbox = inbox;
        job.originalSize = size;
        job.originalMtime = mtime;
        indexJobs({job}, {item}, false, [&result](const Indexing::FileJob &, const Indexing::Result &stored) {
            result = stored;
        });
        if (result.wantsOcr && !aborted()) {
            // The download is about to be deleted, so OCR cannot wait for the queue.
            Q_EMIT progress(Recognizing, item, 0, 1);
            Indexing::runOcr(result.id, m_options, downloadPath);
        }
    }
    QFile::remove(downloadPath);
    flushAdded();
    notifyChanged();
    Q_EMIT progress(Idle, {}, 0, 0);
    Q_EMIT jobFinished();
}

void IndexWorker::reindexDocument(qint64 id)
{
    Q_EMIT jobStarted();
    bool ocrWanted = false;
    const Document doc = Store::document(id);
    const Indexing::FileJob job = Indexing::jobForDocument(id);
    if (doc.isValid() && !job.path.isEmpty()) {
        QueueItem item = queueItemForUrl(doc.url, doc.size);
        item.name = doc.name;
        ocrWanted = indexJobs({job}, {item}, false);
    }
    notifyChanged(true);
    Q_EMIT progress(Idle, {}, 0, 0);
    Q_EMIT jobFinished();
    if (ocrWanted && !aborted()) {
        processOcrQueue();
    }
}

void IndexWorker::recoverOcr()
{
    // Reading pictures and PDFs means running image and PDF libraries on files from anywhere,
    // and a file one of them crashes on would crash Indeks again at every start, as soon as
    // its turn comes. So the documents being read are noted in the index while they are, and
    // a note still there at start tells what was going on when Indeks went down.
    const QStringList noted = Database::value(u"SELECT value FROM meta WHERE key = 'ocr_at'"_s).toString().split(u',', Qt::SkipEmptyParts);
    if (noted.size() == 1) {
        // It was being read alone: that is the one. It is found by name like any other.
        qCWarning(INDEKS_CORE) << "Text recognition is given up for document" << noted.constFirst() << "- Indeks went down while reading it";
        Database::exec(u"UPDATE documents SET ocr = ? WHERE id = ? AND ocr = ?"_s, {int(OcrState::Failed), noted.constFirst().toLongLong(), int(OcrState::Pending)});
    } else {
        for (const QString &id : noted) {
            m_ocrSuspects.insert(id.toLongLong());
        }
    }
    Database::exec(u"INSERT OR REPLACE INTO meta (key, value) VALUES ('ocr_at', '')"_s);
}

void IndexWorker::processOcrQueue()
{
    if (!m_ocrRecovered) {
        m_ocrRecovered = true;
        recoverOcr();
    }
    if (m_ocrActive) {
        return; // the running chain picks up new documents by itself
    }
    m_ocrActive = true;
    Q_EMIT ocrActiveChanged(true);
    ocrStep();
}

void IndexWorker::ocrStep()
{
    // One portion per round, continued through the event loop, so that scans and other
    // requests do not have to wait behind a long OCR run. A portion is one PDF, whose pages
    // are read side by side, or a handful of pictures, read side by side.
    qint64 next = 0;
    int remaining = 0;
    QueueItem current;
    QList<QueueItem> waiting;
    QList<qint64> pictures;
    bool collecting = true;
    const int parallel = m_options.ocrJobs > 0 ? m_options.ocrJobs : Ocr::defaultParallelism();
    const int portion = qMax(8, parallel * PicturesPerProcess);
    if (!aborted() && m_options.ocrEnabled) {
        QSqlQuery query = Database::exec(u"SELECT id, name, url, size, kind FROM documents WHERE ocr = 1 AND trashed = 0 ORDER BY added DESC"_s);
        while (query.next()) {
            const qint64 id = query.value(0).toLongLong();
            if (m_ocrUnreachable.contains(id)) {
                continue;
            }
            ++remaining;
            // Pictures that follow each other in the queue go into the same portion.
            const bool picture = query.value(4).toInt() == int(Kind::Image);
            if (next == 0) {
                collecting = picture; // a portion of pictures only if the queue starts with one
            }
            if (collecting && picture && pictures.size() < portion) {
                pictures.append(id);
            } else {
                collecting = false;
            }
            if (next != 0 && waiting.size() >= MaxListedOcrDocuments) {
                continue;
            }
            QueueItem item = queueItemForUrl(QUrl(query.value(2).toString()), query.value(3).toLongLong());
            item.name = query.value(1).toString();
            if (next == 0) {
                next = id;
                current = item;
            } else {
                item.state = QueueItem::WaitingForOcr;
                waiting.append(item);
            }
        }
    }

    if (next == 0) {
        m_ocrUnreachable.clear();
        m_ocrActive = false;
        Q_EMIT ocrQueueChanged({}, 0);
        Q_EMIT progress(Idle, {}, 0, 0);
        Q_EMIT ocrActiveChanged(false);
        notifyChanged(true);
        return;
    }

    Q_EMIT ocrQueueChanged(waiting, remaining - 1 - int(waiting.size()));
    Q_EMIT progress(Recognizing, current, 0, remaining);
    // Anything that was being read when Indeks last went down is read by itself.
    if (!m_ocrSuspects.isEmpty() && (m_ocrSuspects.contains(next) || std::any_of(pictures.cbegin(), pictures.cend(), [this](qint64 id) {
                                         return m_ocrSuspects.contains(id);
                                     }))) {
        pictures.clear();
    }
    if (pictures.size() <= 1) {
        pictures = {next};
    }
    QStringList noted;
    for (const qint64 id : std::as_const(pictures)) {
        noted.append(QString::number(id));
    }
    Database::exec(u"INSERT OR REPLACE INTO meta (key, value) VALUES ('ocr_at', ?)"_s, {noted.join(u',')});
    const auto done = qScopeGuard([this, pictures] {
        Database::exec(u"INSERT OR REPLACE INTO meta (key, value) VALUES ('ocr_at', '')"_s);
        for (const qint64 id : pictures) {
            m_ocrSuspects.remove(id);
        }
    });
    if (pictures.size() > 1) {
        const QList<qint64> unreachable = Indexing::runOcrBatch(pictures, m_options);
        for (const qint64 id : unreachable) {
            m_ocrUnreachable.insert(id);
        }
    } else if (!Indexing::runOcr(next, m_options)) {
        m_ocrUnreachable.insert(next);
    }
    notifyChanged();
    QMetaObject::invokeMethod(this, &IndexWorker::ocrStep, Qt::QueuedConnection);
}

void IndexWorker::lookAtDocuments()
{
    if (m_lookActive) {
        return; // the round that is under way finds them
    }
    // Pictures and PDFs the index has no fingerprint for, or one of other content: an older
    // Indeks may have read the file anew since. Those that were looked at in vain have one
    // too, without a shape, and are left alone.
    m_lookQueue.clear();
    QSqlQuery query = Database::exec(u"SELECT d.id FROM documents d LEFT JOIN fingerprints f ON f.doc_id = d.id "
                                     "WHERE d.kind IN (?, ?) AND d.parent_id = 0 AND d.missing = 0 AND d.trashed = 0 AND (f.doc_id IS NULL OR f.hash <> d.hash) "
                                     "ORDER BY d.added DESC"_s,
                                     {int(Kind::Pdf), int(Kind::Image)});
    while (query.next()) {
        m_lookQueue.append(query.value(0).toLongLong());
    }
    if (m_lookQueue.isEmpty()) {
        return;
    }
    m_lookTotal = int(m_lookQueue.size());
    m_lookActive = true;
    m_lookStored = false;
    Q_EMIT lookActiveChanged(true);
    lookStep();
}

void IndexWorker::lookStep()
{
    // A portion per round, continued through the event loop, like text recognition: scans
    // and whatever else is asked for do not wait behind some ten thousand documents.
    struct Subject {
        qint64 id = 0;
        QString hash;
        QueueItem item;
    };
    QList<Subject> subjects;
    QList<ExtractorPool::Request> requests;
    while (!aborted() && !m_lookQueue.isEmpty() && requests.size() < LookPortion) {
        const Document doc = Store::document(m_lookQueue.takeFirst());
        // What is not at hand now, a file on a drive that is away or on a server, waits for
        // another start.
        const QString path = doc.isValid() && !doc.isPart() ? Store::contentPath(doc) : QString();
        if (path.isEmpty()) {
            continue;
        }
        Subject subject;
        subject.id = doc.id;
        subject.hash = doc.hash;
        subject.item = queueItemForUrl(doc.url, doc.size);
        subject.item.name = doc.name;
        ExtractorPool::Request request;
        request.id = int(subjects.size());
        request.reading.contentFile = path;
        request.reading.name = doc.name;
        request.reading.lookOnly = true;
        request.size = doc.size;
        subjects.append(subject);
        requests.append(request);
    }
    if (!requests.isEmpty()) {
        const int left = int(m_lookQueue.size() + requests.size());
        QList<std::pair<int, Fingerprint>> seen;
        // Hundreds of pictures are looked at in a second. Saying so for each would keep the
        // window busier with the telling than the helpers are with the looking.
        QElapsedTimer told;
        const auto started = [&](int id) {
            if (!told.isValid() || told.elapsed() >= LookReportMs) {
                told.start();
                Q_EMIT progress(Looking, subjects.at(id).item, m_lookTotal - left + int(seen.size()), m_lookTotal);
            }
        };
        const auto finished = [&](const ExtractorPool::Outcome &outcome) {
            // A file the helper died on has nothing to go by, and is not tried again.
            seen.append({outcome.id, outcome.ok ? outcome.prepared.look : Fingerprint()});
        };
        const int processes = m_options.indexJobs > 0 ? m_options.indexJobs : ExtractorPool::defaultProcesses();
        const bool complete = m_pool.run(requests, m_options, processes, started, finished);
        if (!seen.isEmpty()) {
            Transaction transaction;
            for (const auto &[id, look] : std::as_const(seen)) {
                // Only for a document that is still there and still holds what was looked at.
                Database::exec(u"INSERT OR REPLACE INTO fingerprints (doc_id, hash, shape, ink, b0, b1, b2, b3, b4, b5, b6, b7) "
                               "SELECT id, hash, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? FROM documents WHERE id = ? AND hash = ?"_s,
                               {int(look.shape), look.ink, qint64(look.bits[0]), qint64(look.bits[1]), qint64(look.bits[2]), qint64(look.bits[3]), qint64(look.bits[4]),
                                qint64(look.bits[5]), qint64(look.bits[6]), qint64(look.bits[7]), subjects.at(id).id, subjects.at(id).hash});
            }
            transaction.commit();
            m_lookStored = true;
        }
        Q_UNUSED(complete)
    }
    if (!aborted() && !m_lookQueue.isEmpty()) {
        QMetaObject::invokeMethod(this, &IndexWorker::lookStep, Qt::QueuedConnection);
        return;
    }
    m_lookQueue.clear();
    m_lookActive = false;
    Q_EMIT progress(Idle, {}, 0, 0);
    Q_EMIT lookActiveChanged(false);
    if (m_lookStored) {
        // A list of similar documents that is open gets those that were found since.
        notifyChanged(true);
    }
}

void IndexWorker::tidy()
{
    Q_EMIT jobStarted();
    Q_EMIT progress(Tidying, {}, 0, 0);
    const qint64 freed = Maintenance::tidy();
    Q_EMIT progress(Idle, {}, 0, 0);
    Q_EMIT jobFinished();
    if (freed > 0) {
        Q_EMIT tidied(freed);
    }
}

void IndexWorker::setup()
{
    m_added.clear();
    m_ocrActive = false;
    m_ocrUnreachable.clear();
    m_ocrSuspects.clear();
    m_ocrRecovered = false;
    m_lookQueue.clear();
    m_lookActive = false;
    m_lastChangeSignal = 0;
    Task::setAbortFlag(m_abort);
    Task::setProgressHandler([this](double share) {
        Q_EMIT fileProgress(share);
    });
}

void IndexWorker::shutdown()
{
    m_pool.shutdown();
    Task::setProgressHandler({});
    Database::closeConnection();
}

Indexer::Indexer(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<Indeks::IndexOptions>();
    qRegisterMetaType<Indeks::QueueItem>();
    qRegisterMetaType<QList<Indeks::QueueItem>>();
    m_thread.setObjectName(u"indeks-indexer"_s);
    m_worker = new IndexWorker(&m_abort);
    m_worker->moveToThread(&m_thread);

    connect(m_worker, &IndexWorker::changed, this, &Indexer::changed);
    connect(m_worker, &IndexWorker::documentsAdded, this, &Indexer::documentsAdded);
    connect(m_worker, &IndexWorker::sourceScanned, this, &Indexer::sourceScanned);
    // The worker reports what it does in plain data; the texts are made here because
    // translations must first be used in the main thread.
    // Both run in the indexer thread itself, as its first and last act.
    connect(&m_thread, &QThread::started, m_worker, &IndexWorker::setup, Qt::DirectConnection);
    connect(&m_thread, &QThread::finished, m_worker, &IndexWorker::shutdown, Qt::DirectConnection);

    connect(m_worker, &IndexWorker::progress, this, [this](int phase, const QueueItem &item, int done, int total) {
        const QString name = item.name;
        m_phase = phase;
        m_current = item;
        m_batchDone = done;
        m_fileProgress = -1;
        if (phase == IndexWorker::Idle) {
            m_batch.clear();
        }
        QString text;
        double share = -1;
        switch (IndexWorker::Phase(phase)) {
        case IndexWorker::Idle:
            break;
        case IndexWorker::Scanning:
            text = i18nc("@info:status", "Scanning…");
            break;
        case IndexWorker::Reading:
            text = total > 1 ? i18nc("@info:status number of the document, number of documents", "Indexing (%1 of %2)…", done + 1, total)
                             : i18nc("@info:status", "Indexing…");
            if (total > 1) {
                share = double(done) / total;
            }
            break;
        case IndexWorker::Tidying:
            text = i18nc("@info:status", "Tidying up the index…");
            m_current.name = i18nc("@item what is being worked on: the index of all documents", "The index");
            break;
        case IndexWorker::Looking:
            text = i18ncp("@info:status noting what documents look like, for finding similar ones", "Looking at documents (%1 left)…", "Looking at documents (%1 left)…", qMax(1, total - done));
            if (total > 1) {
                share = double(done) / total;
            }
            break;
        case IndexWorker::Recognizing:
            text = total > 1 ? i18ncp("@info:status", "Recognizing text (%1 document left)…", "Recognizing text (%1 documents left)…", total)
                             : i18nc("@info:status", "Recognizing text…");
            break;
        }
        setStatus(text, text.isEmpty() || phase == IndexWorker::Tidying ? QString() : name, share);
        Q_EMIT queueChanged();
    });
    connect(m_worker, &IndexWorker::tidied, this, &Indexer::tidied);
    connect(m_worker, &IndexWorker::unreadable, this, &Indexer::unreadable);
    connect(m_worker, &IndexWorker::filesVanished, this, &Indexer::filesVanished);
    connect(m_worker, &IndexWorker::batchStarted, this, [this](const QList<QueueItem> &items) {
        m_batch = items;
        m_batchDone = 0;
        Q_EMIT queueChanged();
    });
    connect(m_worker, &IndexWorker::fileProgress, this, [this](double share) {
        m_fileProgress = share;
        Q_EMIT queueChanged();
    });
    connect(m_worker, &IndexWorker::ocrQueueChanged, this, [this](const QList<QueueItem> &waiting, int more) {
        m_ocrWaiting = waiting;
        m_ocrMore = qMax(0, more);
        Q_EMIT queueChanged();
    });
    connect(m_worker, &IndexWorker::jobStarted, this, [this] {
        m_jobRunning = true;
        Q_EMIT queueChanged();
    });
    connect(m_worker, &IndexWorker::jobFinished, this, [this] {
        const bool wasBusy = isBusy();
        if (!m_jobs.isEmpty()) {
            m_jobs.removeFirst();
        }
        m_jobRunning = false;
        if (wasBusy != isBusy()) {
            Q_EMIT busyChanged();
        }
        Q_EMIT queueChanged();
    });
    connect(m_worker, &IndexWorker::ocrActiveChanged, this, [this](bool active) {
        const bool wasBusy = isBusy();
        m_ocrActive = active;
        if (wasBusy != isBusy()) {
            Q_EMIT busyChanged();
        }
    });
    connect(m_worker, &IndexWorker::lookActiveChanged, this, [this](bool active) {
        const bool wasBusy = isBusy();
        m_lookActive = active;
        if (wasBusy != isBusy()) {
            Q_EMIT busyChanged();
        }
    });
    m_thread.start(QThread::LowPriority);
}

Indexer::~Indexer()
{
    stop();
    delete m_worker;
}

bool Indexer::stop(int timeoutMs)
{
    // The worker looks at the flag between files and inside everything that can take long.
    // Jobs that have not begun are dropped with the thread's event queue.
    m_abort = true;
    m_thread.quit();
    if (!m_thread.wait(timeoutMs < 0 ? QDeadlineTimer(QDeadlineTimer::Forever) : QDeadlineTimer(timeoutMs))) {
        return false;
    }
    // What was asked of the worker and never begun must not run if the thread starts again.
    QCoreApplication::removePostedEvents(m_worker);
    return true;
}

void Indexer::restart()
{
    if (m_thread.isRunning()) {
        return;
    }
    // Neither must what the worker reported last and has not arrived here yet.
    QCoreApplication::removePostedEvents(this, QEvent::MetaCall);
    const bool wasBusy = isBusy();
    m_jobs.clear();
    m_jobRunning = false;
    m_ocrActive = false;
    m_lookActive = false;
    m_phase = IndexWorker::Idle;
    m_current = {};
    m_batch.clear();
    m_batchDone = 0;
    m_fileProgress = -1;
    m_ocrWaiting.clear();
    m_ocrMore = 0;
    setStatus({}, {}, -1);
    m_abort = false;
    m_thread.start(QThread::LowPriority);
    if (wasBusy) {
        Q_EMIT busyChanged();
    }
    Q_EMIT queueChanged();
}

template<typename Functor>
void Indexer::post(const QueueItem &description, Functor &&functor)
{
    const bool wasBusy = isBusy();
    m_jobs.append(description);
    QMetaObject::invokeMethod(m_worker, std::forward<Functor>(functor), Qt::QueuedConnection);
    if (!wasBusy) {
        Q_EMIT busyChanged();
    }
    Q_EMIT queueChanged();
}

QList<QueueItem> Indexer::queue() const
{
    QList<QueueItem> list;
    if (m_phase != IndexWorker::Idle) {
        QueueItem current = m_current;
        current.state = m_phase == IndexWorker::Scanning      ? QueueItem::Scanning
            : m_phase == IndexWorker::Recognizing ? QueueItem::Recognizing
            : m_phase == IndexWorker::Tidying     ? QueueItem::Tidying
            : m_phase == IndexWorker::Looking     ? QueueItem::Looking
                                                  : QueueItem::Reading;
        current.progress = m_fileProgress;
        list.append(current);
    }
    if (m_phase == IndexWorker::Reading && m_batchDone + 1 < m_batch.size()) {
        list.append(m_batch.mid(m_batchDone + 1));
    }
    // The first job is the one the lines above came from, once the worker has taken it up.
    QList<QueueItem> waiting = m_jobs.mid(m_jobRunning ? 1 : 0);
    for (QueueItem &job : waiting) {
        if (job.state == QueueItem::Tidying) {
            job.state = QueueItem::Waiting; // not at it yet
        }
    }
    list.append(waiting);
    list.append(m_ocrWaiting);
    return list;
}

void Indexer::setStatus(const QString &status, const QString &detail, double progress)
{
    if (m_status != status || m_statusDetail != detail || m_progress != progress) {
        m_status = status;
        m_statusDetail = detail;
        m_progress = progress;
        Q_EMIT statusChanged();
    }
}

void Indexer::setOptions(const IndexOptions &options)
{
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, options] {
            worker->setOptions(options);
        },
        Qt::QueuedConnection);
}

void Indexer::unsettleSources()
{
    QMetaObject::invokeMethod(
        m_worker,
        [] {
            Database::exec(u"UPDATE sources SET settled = 0"_s);
        },
        Qt::QueuedConnection);
}

void Indexer::scanSource(qint64 sourceId)
{
    const Source source = Store::source(sourceId);
    QueueItem description;
    description.name = source.name;
    description.location = source.url.toDisplayString(QUrl::PreferLocalFile | QUrl::RemoveUserInfo);
    description.state = QueueItem::WaitingForScan;
    post(description, [worker = m_worker, sourceId] {
        worker->scanSource(sourceId);
    });
}

void Indexer::scanDirectory(qint64 sourceId, const QString &directory, bool recursive)
{
    QueueItem description;
    description.name = QDir(directory).dirName();
    description.location = directory;
    description.state = QueueItem::WaitingForScan;
    post(description, [worker = m_worker, sourceId, directory, recursive] {
        worker->scanDirectory(sourceId, directory, recursive);
    });
}

void Indexer::indexDownload(qint64 sourceId, const QUrl &url, const QString &downloadPath, qint64 size, qint64 mtime, bool inbox)
{
    post(queueItemForUrl(url, size), [worker = m_worker, sourceId, url, downloadPath, size, mtime, inbox] {
        worker->indexDownload(sourceId, url, downloadPath, size, mtime, inbox);
    });
}

void Indexer::reindexDocument(qint64 id)
{
    const Document doc = Store::document(id);
    QueueItem description = queueItemForUrl(doc.url, doc.size);
    description.name = doc.name;
    post(description, [worker = m_worker, id] {
        worker->reindexDocument(id);
    });
}

void Indexer::tidy()
{
    const bool waiting = std::any_of(m_jobs.cbegin(), m_jobs.cend(), [](const QueueItem &job) {
        return job.state == QueueItem::Tidying;
    });
    if (waiting) {
        return;
    }
    QueueItem description;
    description.name = i18nc("@item what is being worked on: the index of all documents", "The index");
    description.state = QueueItem::Tidying;
    post(description, [worker = m_worker] {
        worker->tidy();
    });
}

void Indexer::processOcrQueue()
{
    QMetaObject::invokeMethod(m_worker, &IndexWorker::processOcrQueue, Qt::QueuedConnection);
}

void Indexer::lookAtDocuments()
{
    QMetaObject::invokeMethod(m_worker, &IndexWorker::lookAtDocuments, Qt::QueuedConnection);
}

void Indexer::waitUntilIdle(int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    // Give queued signals a chance to arrive before trusting the busy state.
    do {
        QEventLoop loop;
        QTimer::singleShot(20, &loop, &QEventLoop::quit);
        loop.exec();
    } while (isBusy() && timer.elapsed() < timeoutMs);
}

}
