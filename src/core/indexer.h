// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "extractorpool.h"
#include "indexing.h"
#include "types.h"

#include <QObject>
#include <QSet>
#include <QThread>

#include <atomic>
#include <functional>

namespace Indeks
{

/** Does the actual indexing. Lives in the indexer thread; talk to it through Indexer. */
class IndexWorker : public QObject
{
    Q_OBJECT
public:
    explicit IndexWorker(std::atomic_bool *abort);

    enum Phase {
        Idle,
        Scanning,
        Reading,
        Recognizing,
        Tidying,
        Looking,
    };

public Q_SLOTS:
    void setOptions(const Indeks::IndexOptions &options);
    /** Brings the index in line with the files below @p directory of a local source. */
    void scanDirectory(qint64 sourceId, const QString &directory, bool recursive);
    void scanSource(qint64 sourceId);
    /** Indexes a download of a remote file and deletes the download afterwards. */
    void indexDownload(qint64 sourceId, const QUrl &url, const QString &downloadPath, qint64 size, qint64 mtime, bool inbox);
    void reindexDocument(qint64 id);
    void processOcrQueue();
    /**
     * Notes what the pictures and PDFs look like that the index has nothing of the kind for
     * (see Fingerprint): those from before Indeks did that when it read a file, and those an
     * older Indeks changed since. A portion at a time, with room for other work in between.
     */
    void lookAtDocuments();
    /** Gives back the space removed documents left in the index file (see Maintenance). */
    void tidy();
    /** First and last thing the indexer thread does. */
    void setup();
    void shutdown();

Q_SIGNALS:
    /**
     * @p phase is a Phase, @p item what is worked on. While Reading, @p done of @p total files
     * of the batch are finished; for Recognizing, @p total is the number of documents waiting.
     */
    void progress(int phase, const Indeks::QueueItem &item, int done, int total);
    /** The files a job is about to read one after the other; progress() counts through them. */
    void batchStarted(const QList<Indeks::QueueItem> &items);
    /** How much of the file at hand is done, where that can be told. */
    void fileProgress(double share);
    /** Documents waiting for text recognition behind the current one; @p more did not fit. */
    void ocrQueueChanged(const QList<Indeks::QueueItem> &waiting, int more);
    void jobStarted();
    void jobFinished();
    /** The index file shrank by @p bytes. */
    void tidied(qint64 bytes);
    /** Files that could not be read and went into the index by name only. */
    void unreadable(const QStringList &names);
    /** Files are gone from where they were, and their documents went into the trash. */
    void filesVanished(const QStringList &names);
    void changed();
    void documentsAdded(const QList<qint64> &ids);
    void sourceScanned(qint64 sourceId, bool available);
    void ocrActiveChanged(bool active);
    void lookActiveChanged(bool active);

private:
    void notifyChanged(bool force = false);
    void flushAdded();
    /**
     * Reads @p jobs side by side in helper processes and stores them in the index. @p items
     * are their lines for the progress display. @p stored hears of every file that went in.
     * Returns whether any of them is waiting for text recognition now.
     */
    bool indexJobs(const QList<Indexing::FileJob> &jobs,
                   const QList<QueueItem> &items,
                   bool adoptMoved,
                   const std::function<void(const Indexing::FileJob &, const Indexing::Result &)> &stored = {});
    /**
     * Brings the messages the index has of a mailbox in line with its file: finds where each
     * message stands, keeps those it knows, reads the new ones side by side like files, and
     * lets go of those that are gone. @p item is the mailbox's line for the progress display.
     */
    void indexMailbox(qint64 mailboxId, const QueueItem &item);
    void ocrStep();
    void recoverOcr();
    void lookStep();
    bool aborted() const
    {
        return m_abort->load();
    }

    std::atomic_bool *const m_abort;
    IndexOptions m_options;
    QList<qint64> m_added;
    qint64 m_lastChangeSignal = 0;
    bool m_ocrActive = false;
    QSet<qint64> m_ocrUnreachable;
    /**
     * Documents that were being read for text when Indeks last went down without finishing
     * them: one of them may be what brought it down. They are read one at a time, so that a
     * second failure names the culprit, which is then left alone (see recoverOcr()).
     */
    QSet<qint64> m_ocrSuspects;
    bool m_ocrRecovered = false;
    /** The documents that are still to be looked at, and how many there were. */
    QList<qint64> m_lookQueue;
    int m_lookTotal = 0;
    bool m_lookActive = false;
    bool m_lookStored = false;
    ExtractorPool m_pool;
};

/**
 * Front end of the indexer thread. All methods return immediately; results arrive as signals
 * in the thread this object lives in.
 */
class Indexer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString statusDetail READ statusDetail NOTIFY statusChanged)
    Q_PROPERTY(double progress READ progress NOTIFY statusChanged)
public:
    explicit Indexer(QObject *parent = nullptr);
    ~Indexer() override;

    bool isBusy() const
    {
        return !m_jobs.isEmpty() || m_ocrActive || m_lookActive;
    }
    QString status() const
    {
        return m_status;
    }
    /** What the status is about: the file or location being worked on. */
    QString statusDetail() const
    {
        return m_statusDetail;
    }
    /** Share of the current batch that is done, 0 to 1; negative while that cannot be told. */
    double progress() const
    {
        return m_progress;
    }

    void setOptions(const IndexOptions &options);
    /**
     * Has every location gone through as if for the first time: what the next scan finds is
     * not taken for new. Done in turn with the scans, after the one that is running.
     */
    void unsettleSources();
    void scanSource(qint64 sourceId);
    void scanDirectory(qint64 sourceId, const QString &directory, bool recursive);
    void indexDownload(qint64 sourceId, const QUrl &url, const QString &downloadPath, qint64 size, qint64 mtime, bool inbox);
    void reindexDocument(qint64 id);
    void processOcrQueue();
    /** Has the pictures and PDFs looked at that were not yet; see IndexWorker::lookAtDocuments(). */
    void lookAtDocuments();
    /** Queues giving space back to the file system, unless that is waiting already. */
    void tidy();

    /** What is being worked on, followed by everything that is known to wait. */
    QList<QueueItem> queue() const;
    /** How many waiting documents queue() leaves out to stay short. */
    int queueOverflow() const
    {
        return m_ocrMore;
    }

    /**
     * Calls off all work and ends the indexer thread. The file being read is given up, what is
     * indexed so far is kept and the rest is found again by the next scan.
     *
     * Returns false if the thread is still busy after @p timeoutMs (negative: wait for good),
     * which happens when a file format library does not come back from a file.
     */
    bool stop(int timeoutMs = -1);
    /** Starts the indexer thread again after stop(), with nothing left over from before. */
    void restart();

    /** Blocks until everything queued so far is done. Meant for tests. */
    void waitUntilIdle(int timeoutMs = 60000);

Q_SIGNALS:
    void busyChanged();
    void statusChanged();
    void queueChanged();
    void tidied(qint64 bytes);
    void unreadable(const QStringList &names);
    /** Files are gone from where they were, and their documents went into the trash. */
    void filesVanished(const QStringList &names);
    void changed();
    void documentsAdded(const QList<qint64> &ids);
    void sourceScanned(qint64 sourceId, bool available);

private:
    template<typename Functor>
    void post(const QueueItem &description, Functor &&functor);
    void setStatus(const QString &status, const QString &detail, double progress);

    QThread m_thread;
    IndexWorker *m_worker = nullptr;
    std::atomic_bool m_abort = false;
    QList<QueueItem> m_jobs; ///< what was asked for and is not done yet, oldest first
    bool m_jobRunning = false; ///< whether the worker has begun with m_jobs.first()
    bool m_ocrActive = false;
    bool m_lookActive = false;
    int m_phase = 0; ///< an IndexWorker::Phase
    QueueItem m_current;
    QList<QueueItem> m_batch;
    int m_batchDone = 0;
    double m_fileProgress = -1;
    QList<QueueItem> m_ocrWaiting;
    int m_ocrMore = 0;
    QString m_status;
    QString m_statusDetail;
    double m_progress = -1;
};

}
