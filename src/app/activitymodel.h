// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "types.h"

#include <QAbstractListModel>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

namespace Indeks
{
class Library;
}

/**
 * What the indexer is doing, as a list: the file being worked on first, then everything that
 * is known to wait. Shown when the activity display in the sidebar is clicked.
 *
 * The indexer gets through thousands of small files a second, so the list follows it a few
 * times a second at most, and tells views which rows left instead of starting over, so that
 * someone scrolling through the queue is not thrown back to the top.
 */
class ActivityModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by App")
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    /** Waiting documents that are left out of the list to keep it short. */
    Q_PROPERTY(int overflow READ overflow NOTIFY countChanged)
public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        LocationRole,
        SizeTextRole,
        StateTextRole,
        IconNameRole,
        ActiveRole,
        ProgressRole,
    };

    explicit ActivityModel(Indeks::Library *library, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const
    {
        return int(m_items.size());
    }
    int overflow() const
    {
        return m_overflow;
    }

Q_SIGNALS:
    void countChanged();

private:
    void refresh();

    Indeks::Library *const m_library;
    QList<Indeks::QueueItem> m_items;
    int m_overflow = 0;
    QTimer m_refreshTimer;
};
