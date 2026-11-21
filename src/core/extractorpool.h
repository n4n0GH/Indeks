// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "indexing.h"
#include "types.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QList>
#include <QString>

#include <functional>
#include <memory>
#include <vector>

class QProcess;

namespace Indeks
{

/**
 * Reads files for the index in helper processes (the program `indeks-extractor`), several at a
 * time.
 *
 * Reading a file means running format libraries on content from anywhere. That is slow, so it
 * is spread over the cores; and now and then a library crashes or never comes back from a
 * file, which in a process of its own costs that process and not Indeks. A file a helper dies
 * on, or does not finish in time, is reported as not read; the files that shared its helper
 * are read again by another one.
 *
 * Without the helper program everything is read in the calling thread instead.
 */
class ExtractorPool
{
public:
    struct Request {
        int id = 0; ///< the caller's handle for the file; also the order of reading
        Indexing::Reading reading;
        qint64 size = 0; ///< of the file, to put small files into one request
    };
    struct Outcome {
        int id = 0;
        bool ok = false; ///< false: the helper died on the file or took too long
        Indexing::Prepared prepared;
    };

    ExtractorPool();
    ~ExtractorPool();
    Q_DISABLE_COPY_MOVE(ExtractorPool)

    /** Where the helper program is; empty if it is not installed. */
    static QString helperPath();
    /** How many helpers to use unless told otherwise: half the cores, eight at most. */
    static int defaultProcesses();

    /**
     * Reads @p tasks. @p started hears when a file is begun, @p finished gets each result, in
     * the order they come in; @p tick is called regularly in between. All of them run in the
     * calling thread. Blocks until every task is finished or the calling thread's task is
     * aborted (see Task), in which case false is returned and the helpers are stopped.
     */
    bool run(const QList<Request> &tasks,
             const IndexOptions &options,
             int processes,
             const std::function<void(int)> &started,
             const std::function<void(const Outcome &)> &finished,
             const std::function<void()> &tick = {});

    /** Stops all helpers at once. */
    void shutdown();

private:
    struct Helper {
        std::unique_ptr<QProcess> process;
        QByteArray buffer;
        QList<int> inFlight; ///< positions in the task list that were sent and are not finished
        int current = -1; ///< the one the helper said it began
        QElapsedTimer timer; ///< since the helper was last heard from
        bool busy = false;
    };

    bool start(Helper &helper, int slot);
    void release(Helper &helper);

    std::vector<Helper> m_helpers;
    bool m_unavailable = false;
};

/** What pool and helper say to each other; see extractorpool.cpp. */
namespace ExtractorProtocol
{
enum Message : quint8 {
    Batch = 1, ///< to the helper: options and files to read
    Started = 2, ///< from the helper: began reading the file with this number
    Result = 3, ///< from the helper: what reading it gave
    BatchDone = 4, ///< from the helper: ready for more
};

QByteArray frame(const QByteArray &payload);
/** Takes the first complete message off @p buffer; false if there is none yet. */
bool takeFrame(QByteArray &buffer, QByteArray &payload);

QByteArray batchMessage(const IndexOptions &options, const QList<ExtractorPool::Request> &tasks, const QList<int> &positions);
bool readBatch(const QByteArray &payload, IndexOptions &options, QList<ExtractorPool::Request> &tasks);
QByteArray startedMessage(int id);
QByteArray resultMessage(int id, const Indexing::Prepared &prepared);
QByteArray batchDoneMessage();
/** Reads a message from the helper: its kind, and for Started and Result the file's number. */
Message readReply(const QByteArray &payload, int &id, Indexing::Prepared &prepared);
}

}
