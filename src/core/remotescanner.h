// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "types.h"

#include <QObject>
#include <QPointer>
#include <QQueue>
#include <QSet>

class KJob;

namespace Indeks
{
class Indexer;

/**
 * Indexes files that are not reachable through the file system: anything KIO can talk to,
 * such as sftp://, smb:// or webdav:// locations.
 *
 * Remote files are listed with KIO, downloaded one at a time into the cache and handed to the
 * indexer, which deletes the download when it is done. Nothing is ever written to the server.
 */
class RemoteScanner : public QObject
{
    Q_OBJECT
public:
    explicit RemoteScanner(Indexer *indexer, QObject *parent = nullptr);
    ~RemoteScanner() override;

    void setOptions(const IndexOptions &options);
    void scanSource(qint64 sourceId);
    /** Adds a single remote file that does not belong to an indexed location. */
    void addFile(const QUrl &url, bool inbox);

    bool isBusy() const
    {
        return m_listing > 0 || m_downloading || !m_queue.isEmpty();
    }
    QString status() const
    {
        return m_status;
    }

    /** The file being fetched, followed by the ones that wait for their turn. */
    QList<QueueItem> queue() const;

    /** Cancels all transfers and forgets what was waiting. The next scan finds it again. */
    void stop();
    /** Makes the scanner usable again after stop(). */
    void resume();

Q_SIGNALS:
    void busyChanged();
    void statusChanged();
    void queueChanged();
    void changed();
    void sourceScanned(qint64 sourceId, bool available);
    void failed(const QString &message);
    void filesVanished(const QStringList &names);

private:
    struct RemoteFile {
        QUrl url;
        qint64 sourceId = 0;
        qint64 size = 0;
        qint64 mtime = 0;
        bool inbox = false;
        /** Found when its location was gone through for the first time. */
        bool firstPass = false;
    };
    struct Listing {
        QList<RemoteFile> files;
    };

    void finishListing(qint64 sourceId, KJob *job);
    void enqueue(const RemoteFile &file);
    void fetched(const RemoteFile &file);
    void startNextDownload();
    void updateStatus();
    void track(KJob *job);

    Indexer *const m_indexer;
    IndexOptions m_options;
    QHash<qint64, Listing> m_listings;
    QSet<qint64> m_scanning;
    QQueue<RemoteFile> m_queue;
    /// Per location, how many files of the first pass through it are still to be fetched.
    QHash<qint64, int> m_firstPass;
    int m_listing = 0;
    bool m_downloading = false;
    bool m_stopped = false;
    RemoteFile m_current; ///< valid while m_downloading
    double m_currentProgress = -1;
    QList<QPointer<KJob>> m_jobs;
    bool m_wasBusy = false;
    quint64 m_downloadCounter = 0;
    QString m_status;
};

}
