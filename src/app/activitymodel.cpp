// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "activitymodel.h"

#include "library.h"

#include <KFormat>
#include <KLocalizedString>

#include <QMimeDatabase>

using namespace Qt::StringLiterals;
using Indeks::QueueItem;

namespace
{
constexpr int RefreshIntervalMs = 150;

/** Whether two lines stand for the same file or location, whatever state they are in. */
bool sameSubject(const QueueItem &a, const QueueItem &b)
{
    return a.size == b.size && a.name == b.name && a.location == b.location;
}

bool isLocation(const QueueItem &item)
{
    return item.state == QueueItem::Scanning || item.state == QueueItem::WaitingForScan || item.state == QueueItem::Tidying;
}

QString stateText(QueueItem::State state)
{
    switch (state) {
    case QueueItem::Waiting:
        return i18nc("@info state of a file in the indexing queue", "Waiting");
    case QueueItem::WaitingForScan:
        return i18nc("@info state of a folder in the indexing queue", "Waiting to be looked through");
    case QueueItem::WaitingForOcr:
        return i18nc("@info state of a file in the indexing queue", "Waiting for text recognition");
    case QueueItem::WaitingForDownload:
        return i18nc("@info state of a file in the indexing queue", "Waiting to be fetched");
    case QueueItem::Scanning:
        return i18nc("@info state of a folder in the indexing queue", "Looking for new and changed files");
    case QueueItem::Reading:
        return i18nc("@info state of a file in the indexing queue", "Indexing");
    case QueueItem::Recognizing:
        return i18nc("@info state of a file in the indexing queue", "Recognizing text");
    case QueueItem::Downloading:
        return i18nc("@info state of a file in the indexing queue", "Fetching");
    case QueueItem::Tidying:
        return i18nc("@info state of the index in the indexing queue", "Giving back the space that removed documents left");
    case QueueItem::RunningScript:
        return i18nc("@info state of a script in the list of activity", "Running on documents");
    }
    return {};
}
}

ActivityModel::ActivityModel(Indeks::Library *library, QObject *parent)
    : QAbstractListModel(parent)
    , m_library(library)
{
    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(RefreshIntervalMs);
    connect(&m_refreshTimer, &QTimer::timeout, this, &ActivityModel::refresh);
    connect(library, &Indeks::Library::activityChanged, this, [this] {
        if (!m_refreshTimer.isActive()) {
            m_refreshTimer.start();
        }
    });
    refresh();
}

int ActivityModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_items.size());
}

QVariant ActivityModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return {};
    }
    const QueueItem &item = m_items.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return item.name;
    case LocationRole:
        return item.location;
    case SizeTextRole:
        return item.size >= 0 && !isLocation(item) ? KFormat().formatByteSize(double(item.size)) : QString();
    case StateTextRole:
        return stateText(item.state);
    case IconNameRole: {
        if (item.state == QueueItem::Tidying) {
            return u"server-database"_s;
        }
        if (isLocation(item)) {
            return u"folder"_s;
        }
        static const QMimeDatabase mimeDatabase;
        return mimeDatabase.mimeTypeForFile(item.name, QMimeDatabase::MatchExtension).iconName();
    }
    case ActiveRole:
        return item.isActive();
    case ProgressRole:
        return item.progress;
    }
    return {};
}

QHash<int, QByteArray> ActivityModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {LocationRole, "location"},
        {SizeTextRole, "sizeText"},
        {StateTextRole, "stateText"},
        {IconNameRole, "iconName"},
        {ActiveRole, "active"},
        {ProgressRole, "progress"},
    };
}

void ActivityModel::refresh()
{
    const QList<QueueItem> fresh = m_library->activity();
    const int overflow = m_library->activityOverflow();
    const qsizetype oldCount = m_items.size();

    // The usual change is that finished rows leave at the top and the next one becomes the
    // current one. Find out how many left: where does the new top row sit in the old list?
    // If the new top row is not a file that was waiting (a folder being looked through, say),
    // the row below it tells.
    qsizetype gone = 0;
    if (!fresh.isEmpty()) {
        auto position = [this](const QueueItem &item, qsizetype from) {
            for (qsizetype i = from; i < m_items.size(); ++i) {
                if (sameSubject(m_items.at(i), item)) {
                    return i;
                }
            }
            return qsizetype(-1);
        };
        if (const qsizetype top = position(fresh.constFirst(), 0); top >= 0) {
            gone = top;
        } else if (fresh.size() > 1) {
            if (const qsizetype second = position(fresh.at(1), 1); second >= 1) {
                gone = second - 1;
            }
        }
    }
    if (gone > 0) {
        beginRemoveRows(QModelIndex(), 0, int(gone) - 1);
        m_items.remove(0, gone);
        endRemoveRows();
    }

    // From the top down the lists now agree as far as `same`; behind that the old rows go and
    // the new ones come. The top row counts as agreeing: it is updated in place.
    const qsizetype common = qMin(m_items.size(), fresh.size());
    qsizetype same = qMin<qsizetype>(1, common);
    while (same < common && sameSubject(m_items.at(same), fresh.at(same))) {
        ++same;
    }
    if (m_items.size() > same) {
        beginRemoveRows(QModelIndex(), int(same), int(m_items.size()) - 1);
        m_items.remove(same, m_items.size() - same);
        endRemoveRows();
    }
    if (fresh.size() > same) {
        beginInsertRows(QModelIndex(), int(same), int(fresh.size()) - 1);
        m_items.append(fresh.mid(same));
        endInsertRows();
    }
    for (qsizetype row = 0; row < same; ++row) {
        const QueueItem &before = m_items.at(row);
        const QueueItem &now = fresh.at(row);
        if (before.state != now.state || before.progress != now.progress || !sameSubject(before, now)) {
            m_items[row] = now;
            Q_EMIT dataChanged(index(int(row)), index(int(row)));
        }
    }

    if (oldCount != m_items.size() || overflow != m_overflow) {
        m_overflow = overflow;
        Q_EMIT countChanged();
    }
}

#include "moc_activitymodel.cpp"
