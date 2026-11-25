// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "countworker.h"

#include "database.h"
#include "store.h"

#include <QElapsedTimer>

using namespace Indeks;

CountWorker::CountWorker(const std::atomic<quint64> *ticket)
    : m_ticket(ticket)
{
}

void CountWorker::count(quint64 ticket, const QList<CountRequest> &wanted)
{
    QElapsedTimer timer;
    timer.start();
    QHash<QString, int> counts;
    counts.reserve(wanted.size());
    for (const CountRequest &request : wanted) {
        if (ticket != *m_ticket) {
            return; // another database by now, or Indeks is leaving
        }
        counts.insert(request.key, Store::count(request.query));
    }
    // The locations, all in one go. One without documents is not among them.
    const QHash<qint64, int> sources = Store::sourceCounts();
    for (auto it = sources.constBegin(); it != sources.constEnd(); ++it) {
        counts.insert(sourceKeyPrefix() + QString::number(it.key()), it.value());
    }
    Q_EMIT counted(ticket, counts, timer.elapsed());
}

QString CountWorker::sourceKeyPrefix()
{
    return QStringLiteral("source:");
}

void CountWorker::close()
{
    Database::closeConnection();
}

#include "moc_countworker.cpp"
