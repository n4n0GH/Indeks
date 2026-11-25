// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "searchworker.h"

#include <QAbstractListModel>
#include <QSet>
#include <QThread>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

namespace Indeks
{
class Library;
}

/**
 * The documents shown in the file browser: those of the place selected in the sidebar
 * (the scope), narrowed down by the search field (the filter). Also keeps the selection,
 * by document id, so that it survives reloads.
 */
class DocumentModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by App")
    Q_PROPERTY(QString scope READ scope WRITE setScope NOTIFY scopeChanged)
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
    /**
     * Further conditions in the query language, stacked on top of the search field's by the
     * filter bar. The list shows what matches the scope, the filter and these.
     */
    Q_PROPERTY(QString criteria READ criteria WRITE setCriteria NOTIFY filterChanged)
    /** Filter and criteria as one query: everything the user is searching for. */
    Q_PROPERTY(QString effectiveFilter READ effectiveFilter NOTIFY filterChanged)
    Q_PROPERTY(QString sortColumn READ sortColumn WRITE setSortColumn NOTIFY sortChanged)
    Q_PROPERTY(bool sortDescending READ sortDescending WRITE setSortDescending NOTIFY sortChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool truncated READ isTruncated NOTIFY countChanged)
    Q_PROPERTY(QVariantList selectedIds READ selectedIds NOTIFY selectionChanged)
    Q_PROPERTY(int selectionCount READ selectionCount NOTIFY selectionChanged)
    Q_PROPERTY(qint64 currentId READ currentId NOTIFY currentChanged)
    Q_PROPERTY(int currentRow READ currentRow NOTIFY currentChanged)
    Q_PROPERTY(QStringList searchTerms READ searchTerms NOTIFY filterChanged)
    /**
     * A search through the text of the documents is under way and the list still shows what
     * was found before. Such searches run in the background; see SearchWorker.
     */
    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)
    /** The first list of the database that was just opened is not there yet. */
    Q_PROPERTY(bool opening READ isOpening NOTIFY openingChanged)
    /**
     * Whether the list has rows that open like folders: mailboxes, whose messages are listed
     * below them when they are opened. Rows then keep room for the arrow that opens them.
     */
    Q_PROPERTY(bool tree READ isTree NOTIFY treeChanged)
    /**
     * Whether the list is kept at all. One that nobody looks at, such as the list a dialog
     * picks documents from while that dialog is closed, is switched off: it holds nothing
     * and reads nothing, whatever changes in the index.
     */
    Q_PROPERTY(bool active READ isActive WRITE setActive NOTIFY activeChanged)
public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        IconRole,
        KindRole,
        TypeNameRole,
        SizeTextRole,
        ModifiedTextRole,
        AddedTextRole,
        TagsRole,
        SnippetRole,
        SelectedRole,
        MissingRole,
        LingeringRole,
        OfflineRole,
        FlaggedRole,
        AnnotatedRole,
        InboxRole,
        DepthRole,
        ExpandableRole,
        ExpandedRole,
        ParentNameRole,
        GroupsRole,
        SenderRole,
        LocationRole,
        PagesTextRole,
        CommentRole,
        ThumbnailRole, ///< the address of a small picture of the document, empty if there is none; see ThumbnailProvider
    };
    enum SelectionMode {
        Single,
        Toggle,
        Range,
    };
    Q_ENUM(SelectionMode)

    explicit DocumentModel(Indeks::Library *library, QObject *parent = nullptr);
    ~DocumentModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString scope() const
    {
        return m_scope;
    }
    void setScope(const QString &scope);
    QString filter() const
    {
        return m_filter;
    }
    void setFilter(const QString &filter);
    QString criteria() const
    {
        return m_criteria;
    }
    void setCriteria(const QString &criteria);
    QString effectiveFilter() const;
    QString sortColumn() const
    {
        return m_sortColumn;
    }
    void setSortColumn(const QString &column);
    bool sortDescending() const
    {
        return m_sortDescending;
    }
    void setSortDescending(bool descending);
    int count() const
    {
        return int(m_rows.size());
    }
    bool isTruncated() const
    {
        return m_truncated;
    }
    QStringList searchTerms() const
    {
        return m_terms;
    }
    /**
     * A list the user asked for is on its way. Not while the list is only read anew because
     * the index changed: that happens behind what is shown, many times over while documents
     * come in.
     */
    bool isLoading() const
    {
        return m_loading && !m_quiet;
    }
    bool isOpening() const
    {
        return m_opening;
    }
    bool isTree() const
    {
        return m_tree;
    }
    bool isActive() const
    {
        return m_active;
    }
    void setActive(bool active);

    QVariantList selectedIds() const;
    QList<qint64> selection() const;
    int selectionCount() const
    {
        return int(m_selected.size());
    }
    qint64 currentId() const
    {
        return m_current;
    }
    int currentRow() const;

    Q_INVOKABLE void select(int row, DocumentModel::SelectionMode mode = Single);
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void selectId(qint64 id);
    Q_INVOKABLE bool isSelected(int row) const;
    Q_INVOKABLE int rowOf(qint64 id) const;
    Q_INVOKABLE qint64 idAt(int row) const;
    /**
     * Opens or closes a document that holds others: lists what is in it below it, or takes
     * that away again. Stays that way for as long as Indeks runs.
     */
    Q_INVOKABLE void setExpanded(qint64 id, bool expanded);
    Q_INVOKABLE bool isExpanded(qint64 id) const;
    /** The row of the document a part is listed under; -1 if it is not listed under one. */
    Q_INVOKABLE int parentRow(int row) const;
    /** Locations of the selected documents, for dragging them into other applications. */
    Q_INVOKABLE QList<QUrl> selectedUrls() const;

    void reload();
    /**
     * Has documents leave the list at once when they no longer belong into it: for commands
     * that are there to take them out of it, such as removing them from the inbox.
     */
    void letGo(const QList<qint64> &ids);

    /**
     * Ends the thread that searches. Returns false if a search did not come back within
     * @p timeoutMs; the thread is still running then.
     */
    bool stopSearching(int timeoutMs = -1);

Q_SIGNALS:
    void scopeChanged();
    void filterChanged();
    void sortChanged();
    void countChanged();
    void selectionChanged();
    void currentChanged();
    void loadingChanged();
    void openingChanged();
    void treeChanged();
    void activeChanged();

private:
    using Row = DocumentRow;
    enum class Load {
        Navigated, ///< the list shows something else now
        Refreshed, ///< the same list, read anew because something in the index changed
        Lingering, ///< the same list once more, with the documents that are kept in it
    };
    /** @p quiet for a list nobody asked for: it is read behind what is shown. */
    void load(Load load, bool quiet = false);
    void apply(const QList<Row> &rows, bool truncated, const QStringList &excerptTerms);
    /** The documents that have just dropped out of the list and are to be kept in it. */
    QSet<qint64> leavers(const QList<Row> &found) const;
    void setLoading(bool loading);
    void setOpening(bool opening);
    /** The list that was asked for last is there. */
    void loaded();
    /** Nothing is listed any more, and nothing selected. */
    void clear();
    /** The list's query for the documents that @p where finds, in the order the list is in. */
    QString rowsSql(const QString &where) const;
    QList<Row> partsOf(qint64 id, bool *truncated) const;
    bool canExpand(const Row &row) const;
    void indexRows();
    void setSelection(const QSet<qint64> &selected, qint64 current);
    QString snippet(qint64 id) const;
    void requestExcerpts();
    void forgetExcerpts();

    Indeks::Library *const m_library;
    QList<Row> m_rows;
    QHash<qint64, int> m_rowOfId;
    QString m_scope;
    QString m_filter;
    QString m_criteria;
    QString m_sortColumn = QStringLiteral("modified");
    bool m_sortDescending = true;
    bool m_truncated = false;
    /** Documents that are open, showing what is in them. */
    QSet<qint64> m_expanded;
    /** The search itself finds parts of documents; they are listed among the rest then. */
    bool m_flat = false;
    bool m_tree = false;
    bool m_active = true;
    QStringList m_terms;
    bool m_loading = false;
    bool m_quiet = false;
    // The first list of a database is read in the background whatever it is, see load():
    // nothing of an index that was just opened is in memory yet.
    bool m_opening = true;
    /** The index changed while a list was on its way. */
    bool m_refreshWanted = false;
    /** The document to select as soon as the list that is on its way has it. */
    qint64 m_wantedId = 0;

    // Searching in the background
    QThread m_thread;
    SearchWorker *m_worker = nullptr;
    SearchTickets m_tickets;
    SearchHandover m_handover;
    /** What the passages under the names are looked up with, for the search under way. */
    QStringList m_pendingExcerptTerms;

    // The passages shown under the names of documents found by their text. They are looked
    // up for the rows that come into view, a handful at a time.
    QStringList m_excerptTerms;
    mutable QHash<qint64, QString> m_excerpts;
    mutable QList<qint64> m_excerptsWanted;
    mutable QSet<qint64> m_excerptsAsked;
    mutable bool m_excerptRequestScheduled = false;
    bool m_excerptsUnderWay = false;
    QSet<qint64> m_selected;
    qint64 m_current = 0;

    // A document the user works with stays in the list it is in, also when that work means
    // it no longer belongs there: tagged in the list of the untagged, taken up in the inbox.
    // It would vanish under the user's hands otherwise. It leaves when the list is left.
    QSet<qint64> m_lingering;
    /** What was selected since the list was last read: what the user may have worked with. */
    QSet<qint64> m_handled;
    /** Documents that are to leave when they no longer belong, see letGo(). */
    QSet<qint64> m_dismissed;
    /** How the list was last asked for, for when the answer comes from the background. */
    Load m_load = Load::Navigated;
    qint64 m_anchor = 0;
};
