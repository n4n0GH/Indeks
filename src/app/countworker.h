// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

#include <atomic>

/** An entry of the sidebar whose number takes a query of its own: its key, and what it lists. */
struct CountRequest {
    QString key;
    QString query;
};

/**
 * Counts the documents behind entries of the sidebar away from the user interface.
 *
 * Each of these numbers is a walk through the list of documents, and some look further than
 * that (the duplicates). In an index of some hundred thousand documents all of them together
 * take a second or more, and several when the index has just been opened and nothing of it
 * is in memory yet. They are asked for with every change to the index. So they are worked
 * out here, in a thread of their own with its own connection to the index, and the sidebar
 * shows them when they are there.
 *
 * A count that has begun cannot be called off; between two of them, a request that is no
 * longer wanted is given up.
 */
class CountWorker : public QObject
{
    Q_OBJECT
public:
    /** @p ticket says which request is still wanted. */
    explicit CountWorker(const std::atomic<quint64> *ticket);

    /**
     * The indexed locations are counted with every request, without being asked for, and
     * come back under this followed by their id. One without documents does not come back.
     */
    static QString sourceKeyPrefix();

public Q_SLOTS:
    void count(quint64 ticket, const QList<CountRequest> &wanted);
    /** Closes this thread's connection to the index. */
    void close();

Q_SIGNALS:
    /** The numbers by key, and how long they took in milliseconds. */
    void counted(quint64 ticket, const QHash<QString, int> &counts, qint64 elapsed);

private:
    const std::atomic<quint64> *const m_ticket;
};
