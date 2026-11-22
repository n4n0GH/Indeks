// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "remotescanner.h"

#include "database.h"
#include "indeks_core_debug.h"
#include "indexer.h"
#include "indexing.h"
#include "paths.h"
#include "store.h"

#include <KIO/FileCopyJob>
#include <KIO/ListJob>
#include <KIO/StatJob>
#include <KLocalizedString>

#include <QDir>

using namespace Qt::StringLiterals;

namespace Indeks
{

RemoteScanner::RemoteScanner(Indexer *indexer, QObject *parent)
    : QObject(parent)
    , m_indexer(indexer)
{
    // Downloads left behind by a crash.
    QDir(Paths::cacheDir() + "/downloads"_L1).removeRecursively();
}

RemoteScanner::~RemoteScanner() = default;

void RemoteScanner::setOptions(const IndexOptions &options)
{
    m_options = options;
}

void RemoteScanner::updateStatus()
{
    QString status;
    if (m_downloading || !m_queue.isEmpty()) {
        status = i18ncp("@info:status", "Fetching %1 remote file…", "Fetching %1 remote files…", m_queue.size() + (m_downloading ? 1 : 0));
    } else if (m_listing > 0) {
        status = i18nc("@info:status", "Listing remote location…");
    }
    if (status != m_status) {
        m_status = status;
        Q_EMIT statusChanged();
    }
    if (m_wasBusy != isBusy()) {
        m_wasBusy = isBusy();
        Q_EMIT busyChanged();
    }
    Q_EMIT queueChanged();
}

QList<QueueItem> RemoteScanner::queue() const
{
    QList<QueueItem> list;
    if (m_downloading) {
        QueueItem item = queueItemForUrl(m_current.url, m_current.size);
        item.state = QueueItem::Downloading;
        item.progress = m_currentProgress;
        list.append(item);
    }
    for (const RemoteFile &file : m_queue) {
        QueueItem item = queueItemForUrl(file.url, file.size);
        item.state = QueueItem::WaitingForDownload;
        list.append(item);
    }
    return list;
}

void RemoteScanner::resume()
{
    m_stopped = false;
    m_scanning.clear();
    m_listing = 0;
    m_downloading = false;
    updateStatus();
}

void RemoteScanner::track(KJob *job)
{
    m_jobs.removeAll(nullptr);
    m_jobs.append(job);
}

void RemoteScanner::stop()
{
    m_stopped = true;
    m_queue.clear();
    // A first pass that is cut short is not done: the next scan goes on with it.
    m_firstPass.clear();
    m_listings.clear();
    for (const QPointer<KJob> &job : std::as_const(m_jobs)) {
        if (job) {
            job->kill(KJob::Quietly); // no result signal, so nothing of ours runs any more
        }
    }
    m_jobs.clear();
}

void RemoteScanner::scanSource(qint64 sourceId)
{
    const Source source = Store::source(sourceId);
    if (m_stopped || source.id == 0 || !source.enabled || source.isLocal() || m_scanning.contains(sourceId)) {
        return;
    }
    m_scanning.insert(sourceId);
    m_listings.insert(sourceId, Listing());
    ++m_listing;

    QUrl base = source.url;
    if (!base.path().endsWith(u'/')) {
        base.setPath(base.path() + u'/');
    }
    const KIO::ListJob::ListFlags listFlags = m_options.indexHidden ? KIO::ListJob::ListFlags(KIO::ListJob::ListFlag::IncludeHidden) : KIO::ListJob::ListFlags();
    KIO::ListJob *job =
        source.recursive ? KIO::listRecursive(source.url, KIO::HideProgressInfo, listFlags) : KIO::listDir(source.url, KIO::HideProgressInfo, listFlags);

    connect(job, &KIO::ListJob::entries, this, [this, sourceId, base](KIO::Job *, const KIO::UDSEntryList &entries) {
        auto listing = m_listings.find(sourceId);
        if (listing == m_listings.end()) {
            return;
        }
        for (const KIO::UDSEntry &entry : entries) {
            const QString name = entry.stringValue(KIO::UDSEntry::UDS_NAME); // relative path when listing recursively
            const QString baseName = name.section(u'/', -1);
            if (entry.isDir() || baseName.isEmpty() || baseName == "."_L1 || baseName == ".."_L1 || Indexing::isIgnoredFileName(baseName)) {
                continue;
            }
            RemoteFile file;
            file.url = base;
            file.url.setPath(base.path() + name);
            file.sourceId = sourceId;
            file.size = entry.numberValue(KIO::UDSEntry::UDS_SIZE, 0);
            file.mtime = entry.numberValue(KIO::UDSEntry::UDS_MODIFICATION_TIME, 0);
            listing->files.append(file);
        }
    });
    connect(job, &KJob::result, this, [this, sourceId](KJob *finished) {
        finishListing(sourceId, finished);
    });
    track(job);
    updateStatus();
}

void RemoteScanner::finishListing(qint64 sourceId, KJob *job)
{
    const Listing listing = m_listings.take(sourceId);
    m_scanning.remove(sourceId);
    --m_listing;

    if (job->error()) {
        // Server unreachable or location gone. Keep what is indexed; it stays searchable offline.
        qCInfo(INDEKS_CORE) << "Cannot list remote source" << sourceId << job->errorString();
        Q_EMIT sourceScanned(sourceId, false);
        updateStatus();
        return;
    }

    struct Known {
        qint64 id;
        qint64 size;
        qint64 mtime;
        bool missing;
    };
    QHash<QString, Known> known;
    {
        QSqlQuery query = Database::exec(u"SELECT id, url, size, mtime, missing FROM documents WHERE source_id = ? AND parent_id = 0"_s, {sourceId});
        while (query.next()) {
            known.insert(query.value(1).toString(), {query.value(0).toLongLong(), query.value(2).toLongLong(), query.value(3).toLongLong(), query.value(4).toBool()});
        }
    }

    // What is here when a location is gone through for the first time was there before. What
    // turns up later is new, and waits in the inbox to be filed.
    const Source source = Store::source(sourceId);
    for (RemoteFile file : listing.files) {
        const QString key = Store::urlKey(file.url);
        const auto entry = known.constFind(key);
        if (entry != known.constEnd()) {
            const bool unchanged = !entry->missing && entry->size == file.size && entry->mtime == file.mtime;
            known.erase(entry);
            if (unchanged) {
                continue;
            }
        } else {
            file.inbox = source.newFilesToInbox();
        }
        file.firstPass = !source.settled;
        enqueue(file);
    }

    QStringList vanished;
    {
        Transaction transaction;
        // Gone from the server. Into the trash with them, as with files that are gone from
        // a folder: see IndexWorker::scanDirectory().
        for (auto it = known.constBegin(); it != known.constEnd(); ++it) {
            if (it->missing) {
                continue;
            }
            const QString name = Store::document(it->id).name;
            if (Store::fileVanished(it->id)) {
                vanished.append(name);
            }
        }
        Database::exec(u"UPDATE sources SET last_scan = ? WHERE id = ?"_s, {QDateTime::currentSecsSinceEpoch(), sourceId});
        if (!source.settled && m_firstPass.value(sourceId) == 0) {
            // Nothing of it has to be fetched; otherwise that is waited for, see fetched().
            Database::exec(u"UPDATE sources SET settled = 1 WHERE id = ?"_s, {sourceId});
        }
        transaction.commit();
    }

    if (!vanished.isEmpty()) {
        Q_EMIT filesVanished(vanished);
    }
    Q_EMIT sourceScanned(sourceId, true);
    Q_EMIT changed();
    startNextDownload();
    updateStatus();
}

void RemoteScanner::checkFolder(const QUrl &url)
{
    if (m_stopped) {
        return;
    }
    ++m_listing;
    KIO::StatJob *job = KIO::stat(url, KIO::StatJob::SourceSide, KIO::StatBasic, KIO::HideProgressInfo);
    track(job);
    connect(job, &KJob::result, this, [this, url, job] {
        --m_listing;
        if (job->error()) {
            Q_EMIT failed(i18nc("@info", "Cannot add %1: %2", url.toDisplayString(), job->errorString()));
        } else if (job->statResult().isDir()) {
            Q_EMIT folderFound(url);
        } else {
            Q_EMIT failed(i18nc("@info %1 is the address of a file",
                                "%1 is a file. Indeks takes folders, not single files: add the folder it is in as an indexed location.",
                                url.toDisplayString()));
        }
        updateStatus();
    });
    updateStatus();
}

void RemoteScanner::enqueue(const RemoteFile &file)
{
    if (!m_options.indexContent || file.size > qint64(m_options.maxRemoteSizeMb) * 1024 * 1024) {
        // Not worth or not wanted to download: searchable by name only.
        Indexing::indexNameOnly(file.url, file.sourceId, file.size, file.mtime, file.inbox);
        return;
    }
    if (file.firstPass) {
        ++m_firstPass[file.sourceId];
    }
    m_queue.enqueue(file);
}

void RemoteScanner::fetched(const RemoteFile &file)
{
    if (!file.firstPass) {
        return;
    }
    const auto left = m_firstPass.find(file.sourceId);
    if (left != m_firstPass.end() && --left.value() <= 0) {
        // The last file that was there from the start: from here on, what is found is new.
        // Had Indeks been closed before, the rest would have been taken for new the next time.
        m_firstPass.erase(left);
        Database::exec(u"UPDATE sources SET settled = 1 WHERE id = ?"_s, {file.sourceId});
    }
}

void RemoteScanner::startNextDownload()
{
    if (m_stopped || m_downloading || m_queue.isEmpty()) {
        updateStatus();
        return;
    }
    const RemoteFile file = m_queue.dequeue();
    m_downloading = true;
    m_current = file;
    m_currentProgress = -1;

    const QString dir = Paths::cacheDir() + "/downloads/"_L1 + QString::number(++m_downloadCounter);
    QDir().mkpath(dir);
    // Keep the file name: the type detection looks at the extension.
    const QString target = dir + u'/' + file.url.fileName();

    KIO::FileCopyJob *job = KIO::file_copy(file.url, QUrl::fromLocalFile(target), 0600, KIO::HideProgressInfo | KIO::Overwrite);
    track(job);
    connect(job, &KJob::percentChanged, this, [this](KJob *, unsigned long percent) {
        m_currentProgress = percent / 100.0;
        Q_EMIT queueChanged();
    });
    connect(job, &KJob::result, this, [this, file, target, dir, job] {
        m_downloading = false;
        fetched(file);
        if (job->error()) {
            qCInfo(INDEKS_CORE) << "Cannot fetch" << file.url << job->errorString();
            QDir(dir).removeRecursively();
            Indexing::indexNameOnly(file.url, file.sourceId, file.size, file.mtime, file.inbox);
            Q_EMIT changed();
        } else {
            m_indexer->indexDownload(file.sourceId, file.url, target, file.size, file.mtime, file.inbox);
        }
        startNextDownload();
    });
    updateStatus();
}

}
