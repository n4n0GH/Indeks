// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>

namespace Indeks
{
class Library;
}

/**
 * Content of the left sidebar: the inbox and other fixed places, smart groups, saved searches,
 * groups, tags and indexed locations. Every entry stands for a search query; selecting
 * one shows the matching documents in the file browser.
 */
class SidebarModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by App")
    Q_PROPERTY(QString currentKey READ currentKey WRITE setCurrentKey NOTIFY currentChanged)
    Q_PROPERTY(QString currentQuery READ currentQuery NOTIFY currentChanged)
    Q_PROPERTY(QString currentLabel READ currentLabel NOTIFY currentChanged)
    /** The icon the current entry has in the sidebar; for what stands for it elsewhere. */
    Q_PROPERTY(QString currentIcon READ currentIcon NOTIFY currentChanged)
    Q_PROPERTY(QString currentKind READ currentKind NOTIFY currentChanged)
    Q_PROPERTY(qint64 currentId READ currentId NOTIFY currentChanged)
    Q_PROPERTY(QStringList collapsedSections READ collapsedSections WRITE setCollapsedSections NOTIFY collapsedSectionsChanged)
public:
    enum Roles {
        KeyRole = Qt::UserRole + 1,
        LabelRole,
        IconRole,
        CountRole,
        QueryRole,
        DepthRole,
        KindRole,
        ItemIdRole,
        HeaderRole,
        CollapsedRole,
        OfflineRole,
        CurrentRole,
    };

    explicit SidebarModel(Indeks::Library *library, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString currentKey() const
    {
        return m_currentKey;
    }
    void setCurrentKey(const QString &key);
    QString currentQuery() const;
    QString currentLabel() const;
    QString currentIcon() const;
    /** "place", "smart", "search", "group", "tag" or "source". */
    QString currentKind() const;
    /** Database id of the current group, tag, search or location; 0 otherwise. */
    qint64 currentId() const;

    QStringList collapsedSections() const
    {
        return m_collapsed;
    }
    void setCollapsedSections(const QStringList &sections);
    Q_INVOKABLE void toggleSection(const QString &section);
    Q_INVOKABLE int rowOfKey(const QString &key) const;
    /** Moves the selection up or down by one entry, skipping headers. Returns the new row or -1. */
    Q_INVOKABLE int step(int delta);

    void reload();

Q_SIGNALS:
    void currentChanged();
    void collapsedSectionsChanged();

private:
    struct Item {
        QString key;
        QString label;
        QString icon;
        QString query;
        QString kind;
        QString section;
        qint64 id = 0;
        int count = -1;
        int depth = 0;
        bool header = false;
        bool offline = false;
    };
    const Item *current() const;
    void rememberCurrent();
    void addHeader(QList<Item> &items, const QString &section, const QString &label) const;

    Indeks::Library *const m_library;
    QList<Item> m_items;
    QString m_currentKey = QStringLiteral("place:all");
    QStringList m_collapsed;
    // Query and label of the current entry, kept for when its section is collapsed.
    QString m_lastQuery;
    QString m_lastLabel;
};
