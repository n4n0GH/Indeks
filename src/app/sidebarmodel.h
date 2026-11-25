// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "countworker.h"

#include <QAbstractListModel>
#include <QElapsedTimer>
#include <QThread>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include <atomic>

namespace Indeks
{
class Library;
}

/**
 * Content of the database view of the panels: the inbox and other fixed places, smart groups, saved searches,
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
    /**
     * The sections that have a heading, in the order they are listed in: "smart",
     * "searches", "groups", "tags", "sources". The user's to say. The places without a
     * heading (inbox, all documents, …) come first whatever it is.
     */
    Q_PROPERTY(QStringList sectionOrder READ sectionOrder WRITE setSectionOrder NOTIFY sectionOrderChanged)
    /** The numbers of the database that was just opened are not there yet. */
    Q_PROPERTY(bool opening READ isOpening NOTIFY openingChanged)
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
        SectionRole,
        SectionEndRole,
        CollapsedRole,
        OfflineRole,
        CurrentRole,
    };

    explicit SidebarModel(Indeks::Library *library, QObject *parent = nullptr);
    ~SidebarModel() override;

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

    static QStringList defaultSectionOrder();
    QStringList sectionOrder() const
    {
        return m_sectionOrder;
    }
    void setSectionOrder(const QStringList &order);
    /** Where a section stands among those with a heading, from 0; -1 for none of that name. */
    Q_INVOKABLE int sectionIndex(const QString &section) const;
    /** Puts @p section before @p target, or behind it. */
    Q_INVOKABLE void moveSection(const QString &section, const QString &target, bool after);
    /** Whether @p section can go @p by places down (or up, if negative). */
    Q_INVOKABLE bool canShiftSection(const QString &section, int by) const;
    Q_INVOKABLE void shiftSection(const QString &section, int by);
    /** The section the selected entry is in; "library" for the places without a heading. */
    QString currentSection() const;
    Q_INVOKABLE void toggleSection(const QString &section);
    Q_INVOKABLE int rowOfKey(const QString &key) const;
    /** Moves the selection up or down by one entry, skipping headers. Returns the new row or -1. */
    Q_INVOKABLE int step(int delta);

    /**
     * Lists the entries anew, at once, and asks for the numbers that take a query each.
     * Those are worked out in the background (CountWorker); until they are there, an entry
     * shows the number it had, and none if it never had one.
     */
    void reload();

    bool isOpening() const
    {
        return m_opening;
    }
    /** The number last worked out for the entry @p key; -1 while there is none. */
    int countOf(const QString &key) const;
    /**
     * Ends the counting for good, before Indeks leaves. A count that is under way is waited
     * for, @p timeoutMs at most (for ever if negative). Returns whether the thread has ended.
     */
    bool stopCounting(int timeoutMs = -1);

Q_SIGNALS:
    void currentChanged();
    void collapsedSectionsChanged();
    void sectionOrderChanged();
    void openingChanged();
    /** Numbers came in from the background. */
    void countsChanged();

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
    /** Lists the entries with the numbers that are known. */
    void rebuild();
    void requestCounts();
    void startCounting();
    void applyCounts(const QHash<QString, int> &counts, qint64 elapsed);

    Indeks::Library *const m_library;
    QList<Item> m_items;

    // Counting in the background
    QThread m_thread;
    CountWorker *m_worker = nullptr;
    /** Goes up when what is being counted is no longer wanted: another database, or the end. */
    std::atomic<quint64> m_ticket = 0;
    /** The entries that are listed and take a query for their number. */
    QList<CountRequest> m_wanted;
    QHash<QString, int> m_counts;
    bool m_counting = false;
    /** The index changed while it was being counted. */
    bool m_countAgain = false;
    bool m_opening = true;
    // Counting takes its time in a large index, and the index may change many times a
    // second while documents come in. After a round, as long a pause as the round took.
    QTimer m_countTimer;
    QElapsedTimer m_sinceCounted;
    qint64 m_countDuration = 0;
    QString m_currentKey = QStringLiteral("place:all");
    QStringList m_collapsed;
    QStringList m_sectionOrder = defaultSectionOrder();
    // Query and label of the current entry, kept for when its section is collapsed.
    QString m_lastQuery;
    QString m_lastLabel;
};
