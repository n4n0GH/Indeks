// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "documentmodel.h"

#include "database.h"
#include "library.h"
#include "searchquery.h"
#include "store.h"
#include "thumbnailprovider.h"

#include <KFormat>
#include <KLocalizedString>

#include <QLocale>
#include <QMimeDatabase>
#include <QTimer>

using namespace Indeks;
using namespace Qt::StringLiterals;

namespace
{
constexpr int MaxRows = 20000;
// How long the list waits for what it asked the search thread for before it lets the answer
// come by itself. Long enough for any list of a database of ordinary size, short enough not
// to be felt where it is not.
constexpr int QuickAnswerMs = 30;
// How many rows' passages are looked up in one go: about what is in view and a little more.
constexpr int ExcerptsAtOnce = 48;

struct MimeInfo {
    QString icon;
    QString comment;
};

const MimeInfo &mimeInfo(const QString &name)
{
    static QHash<QString, MimeInfo> cache;
    auto it = cache.find(name);
    if (it == cache.end()) {
        static const QMimeDatabase database;
        const QMimeType type = database.mimeTypeForName(name);
        it = cache.insert(name, type.isValid() ? MimeInfo{type.iconName(), type.comment()} : MimeInfo{u"unknown"_s, name});
    }
    return *it;
}

QString formatTime(qint64 secs)
{
    return secs > 0 ? QLocale().toString(QDateTime::fromSecsSinceEpoch(secs), QLocale::ShortFormat) : QString();
}
}

DocumentModel::DocumentModel(Library *library, QObject *parent)
    : QAbstractListModel(parent)
    , m_library(library)
{
    qRegisterMetaType<DocumentRow>();
    qRegisterMetaType<QList<DocumentRow>>();
    qRegisterMetaType<QList<qint64>>();
    qRegisterMetaType<QHash<qint64, QString>>();

    m_thread.setObjectName(u"indeks-search"_s);
    m_worker = new SearchWorker(&m_tickets, &m_handover);
    m_worker->moveToThread(&m_thread);
    // Runs in the search thread itself, as its last act.
    connect(&m_thread, &QThread::finished, m_worker, &SearchWorker::close, Qt::DirectConnection);
    connect(m_worker, &SearchWorker::found, this, [this](quint64 ticket, const QList<DocumentRow> &rows, bool truncated) {
        if (ticket != m_tickets.search) {
            return; // overtaken by a newer search or a change of place
        }
        apply(rows, truncated, m_pendingExcerptTerms);
        if (ticket == m_tickets.search) {
            // Unless that asked for the list once more, see apply().
            loaded();
        }
    });
    connect(m_worker, &SearchWorker::excerptsReady, this, [this](quint64 ticket, const QHash<qint64, QString> &excerpts, bool last) {
        if (ticket != m_tickets.excerpts) {
            return;
        }
        for (auto it = excerpts.constBegin(); it != excerpts.constEnd(); ++it) {
            m_excerpts.insert(it.key(), it.value());
            const auto row = m_rowOfId.constFind(it.key());
            if (row != m_rowOfId.constEnd() && !it.value().isEmpty()) {
                Q_EMIT dataChanged(index(*row), index(*row), {SnippetRole});
            }
        }
        if (last) {
            m_excerptsUnderWay = false;
            requestExcerpts();
        }
    });
    m_thread.start();

    connect(library, &Library::changed, this, [this] {
        if (m_loading) {
            // A list is on its way. Asking again with every change would only put it off,
            // for as long as documents keep coming in; it is read once more when it is there.
            m_refreshWanted = true;
            return;
        }
        load(Load::Refreshed, true);
    });
    connect(library, &Library::databaseAboutToChange, this, [this] {
        // What is on its way belongs to the database that is being left, and the search
        // thread must let go of that database's file. What is listed belongs to it too, and
        // the list of the other database takes a moment to come.
        clear();
        if (m_thread.isRunning()) {
            QMetaObject::invokeMethod(m_worker, &SearchWorker::close, Qt::BlockingQueuedConnection);
        }
        setOpening(m_active);
    });
}

DocumentModel::~DocumentModel()
{
    stopSearching();
    delete m_worker;
}

bool DocumentModel::stopSearching(int timeoutMs)
{
    ++m_tickets.search;
    ++m_tickets.excerpts;
    m_thread.quit();
    return m_thread.wait(timeoutMs < 0 ? QDeadlineTimer(QDeadlineTimer::Forever) : QDeadlineTimer(timeoutMs));
}

int DocumentModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QHash<int, QByteArray> DocumentModel::roleNames() const
{
    return {
        {IdRole, "documentId"},
        {NameRole, "name"},
        {IconRole, "iconName"},
        {KindRole, "kind"},
        {TypeNameRole, "typeName"},
        {SizeTextRole, "sizeText"},
        {ModifiedTextRole, "modifiedText"},
        {AddedTextRole, "addedText"},
        {TagsRole, "tags"},
        {SnippetRole, "snippet"},
        {SelectedRole, "selected"},
        {MissingRole, "missing"},
        {LingeringRole, "lingering"},
        {ThumbnailRole, "thumbnail"},
        {OfflineRole, "offline"},
        {FlaggedRole, "flagged"},
        {AnnotatedRole, "annotated"},
        {InboxRole, "inbox"},
        {DepthRole, "depth"},
        {ExpandableRole, "expandable"},
        {ExpandedRole, "expanded"},
        {ParentNameRole, "parentName"},
        {GroupsRole, "groups"},
        {SenderRole, "sender"},
        {LocationRole, "location"},
        {PagesTextRole, "pagesText"},
        {CommentRole, "comment"},
    };
}

QVariant DocumentModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size()) {
        return {};
    }
    const Row &row = m_rows.at(index.row());
    switch (role) {
    case IdRole:
        return row.id;
    case Qt::DisplayRole:
    case NameRole:
        return row.name;
    case IconRole:
        // There is no icon for the type of a mailbox file; it holds mail like a folder does.
        if (row.kind == int(Kind::Website)) {
            // One icon for all of them, whatever kind of file holds the address, and not the
            // one of an HTML file, which is what icon themes give an internet shortcut.
            return u"globe"_s;
        }
        return isContainerMimeType(row.mime) ? u"folder-mail"_s : mimeInfo(row.mime).icon;
    case KindRole:
        return row.kind;
    case TypeNameRole:
        if (row.parts > 0) {
            // What it is, and how much is in it.
            return i18ncp("@info type of a document that holds others, e.g. 'Mailbox', and how many", "%2, %1 message", "%2, %1 messages", row.parts, mimeInfo(row.mime).comment);
        }
        if (row.kind == int(Kind::Website)) {
            return i18nc("@info type of document", "Website");
        }
        return row.kind == int(Kind::Canvas) ? i18nc("@info type of document", "Obsidian canvas") : mimeInfo(row.mime).comment;
    case SizeTextRole:
        return KFormat().formatByteSize(double(row.size));
    case ModifiedTextRole:
        return formatTime(row.mtime);
    case AddedTextRole:
        return formatTime(row.added);
    case TagsRole:
        return row.tags;
    case SnippetRole:
        return snippet(row.id);
    case SelectedRole:
        return m_selected.contains(row.id);
    case MissingRole:
        return row.missing;
    case LingeringRole:
        return m_lingering.contains(row.id);
    case OfflineRole:
        return row.sourceId != 0 && !m_library->isSourceAvailable(row.sourceId);
    case ThumbnailRole:
        // Only of what can be read right now.
        if (row.missing || (row.sourceId != 0 && !m_library->isSourceAvailable(row.sourceId))) {
            return QString();
        }
        return ThumbnailProvider::address(row.id, row.kind, row.path, row.mtime, row.size);
    case FlaggedRole:
        return row.flagged;
    case AnnotatedRole:
        return row.annotated;
    case InboxRole:
        return row.inbox;
    case DepthRole:
        return row.depth;
    case ExpandableRole:
        return canExpand(row);
    case ExpandedRole:
        return canExpand(row) && m_expanded.contains(row.id);
    case ParentNameRole:
        // Said with a part that is listed on its own, away from the document it is in.
        return row.depth == 0 ? row.parentName : QString();
    case GroupsRole:
        return row.groups;
    case SenderRole:
        return row.sender;
    case LocationRole:
        return row.location;
    case PagesTextRole:
        return row.pages > 0 ? QLocale().toString(row.pages) : QString();
    case CommentRole:
        return row.comment;
    }
    return {};
}

bool DocumentModel::canExpand(const Row &row) const
{
    return row.parts > 0 && row.depth == 0 && !m_flat;
}

QString DocumentModel::rowsSql(const QString &where) const
{
    static const QHash<QString, QString> orderColumns = {
        {u"name"_s, u"d.name COLLATE NOCASE"_s},
        {u"kind"_s, u"d.mime"_s},
        {u"modified"_s, u"d.mtime"_s},
        {u"added"_s, u"d.added"_s},
        {u"size"_s, u"d.size"_s},
        {u"pages"_s, u"d.pages"_s},
        {u"comment"_s, u"d.comment COLLATE NOCASE"_s},
        {u"location"_s, u"d.url"_s},
        {u"sender"_s, u"(SELECT m.value FROM metadata m WHERE m.doc_id = d.id AND m.key = 'emailFrom') COLLATE NOCASE"_s},
    };
    const QString order = orderColumns.value(m_sortColumn, u"d.mtime"_s);
    return u"SELECT d.id, d.name, d.mime, d.kind, d.size, d.mtime, d.added, d.source_id, d.missing, d.flagged, d.inbox, "
           "(SELECT group_concat(t.name, char(31)) FROM document_tags dt JOIN tags t ON t.id = dt.tag_id WHERE dt.doc_id = d.id), "
           "(d.comment <> '' OR EXISTS (SELECT 1 FROM annotations a WHERE a.doc_id = d.id)), "
           "d.parent_id, "
           "CASE WHEN d.parent_id = 0 THEN '' ELSE (SELECT p.name FROM documents p WHERE p.id = d.parent_id) END, "
           "CASE WHEN d.parent_id = 0 THEN (SELECT COUNT(*) FROM documents c WHERE c.parent_id = d.id AND c.trashed = 0) ELSE 0 END, "
           "d.pages, d.comment, "
           "(SELECT group_concat(f.name, char(31)) FROM document_folders df JOIN folders f ON f.id = df.folder_id WHERE df.doc_id = d.id), "
           "(SELECT m.value FROM metadata m WHERE m.doc_id = d.id AND m.key = 'emailFrom'), "
           "d.url "
           "FROM documents d WHERE %1 ORDER BY %2 %3, d.id LIMIT %4"_s.arg(where, order, m_sortDescending ? u"DESC"_s : u"ASC"_s)
        .arg(MaxRows + 1);
}

QList<DocumentModel::Row> DocumentModel::partsOf(qint64 id, bool *truncated) const
{
    QList<Row> parts = SearchWorker::fetch(rowsSql(u"d.parent_id = ? AND d.trashed = 0"_s), {id}, MaxRows, truncated);
    for (Row &part : parts) {
        part.depth = 1;
    }
    return parts;
}

void DocumentModel::indexRows()
{
    m_rowOfId.clear();
    m_rowOfId.reserve(m_rows.size());
    for (int i = 0; i < m_rows.size(); ++i) {
        m_rowOfId.insert(m_rows.at(i).id, i);
    }
}

bool DocumentModel::isExpanded(qint64 id) const
{
    return m_expanded.contains(id);
}

int DocumentModel::parentRow(int row) const
{
    return row >= 0 && row < m_rows.size() && m_rows.at(row).depth > 0 ? m_rowOfId.value(m_rows.at(row).parentId, -1) : -1;
}

void DocumentModel::setExpanded(qint64 id, bool expanded)
{
    const int row = rowOf(id);
    if (row < 0 || !canExpand(m_rows.at(row)) || expanded == m_expanded.contains(id)) {
        return;
    }
    const int oldCount = count();
    if (expanded) {
        m_expanded.insert(id);
        bool truncated = false;
        const QList<Row> parts = partsOf(id, &truncated);
        if (!parts.isEmpty()) {
            beginInsertRows({}, row + 1, row + int(parts.size()));
            m_rows.insert(row + 1, parts.size(), Row());
            std::copy(parts.cbegin(), parts.cend(), m_rows.begin() + row + 1);
            indexRows();
            endInsertRows();
        }
        m_truncated = m_truncated || truncated;
    } else {
        m_expanded.remove(id);
        int last = row;
        while (last + 1 < m_rows.size() && m_rows.at(last + 1).depth > 0) {
            ++last;
        }
        if (last > row) {
            beginRemoveRows({}, row + 1, last);
            m_rows.remove(row + 1, last - row);
            indexRows();
            endRemoveRows();
        }
    }
    Q_EMIT dataChanged(index(row), index(row), {ExpandedRole});
    if (oldCount != count()) {
        Q_EMIT countChanged();
    }
    // What is no longer listed is no longer selected. Was it what is being looked at, its
    // document takes its place.
    QSet<qint64> selected;
    for (qint64 selectedId : std::as_const(m_selected)) {
        if (m_rowOfId.contains(selectedId)) {
            selected.insert(selectedId);
        }
    }
    if (m_rowOfId.contains(m_current)) {
        setSelection(selected, m_current);
    } else {
        selected.insert(id);
        setSelection(selected, id);
    }
}

QString DocumentModel::snippet(qint64 id) const
{
    if (m_excerptTerms.isEmpty()) {
        return {};
    }
    const auto cached = m_excerpts.constFind(id);
    if (cached != m_excerpts.constEnd()) {
        return *cached;
    }
    // Not known yet: the row is shown without for a moment, and told when it has arrived.
    if (!m_excerptsAsked.contains(id)) {
        m_excerptsAsked.insert(id);
        m_excerptsWanted.append(id);
        if (!m_excerptRequestScheduled) {
            // After the view has asked for all the rows it is about to show.
            m_excerptRequestScheduled = true;
            QTimer::singleShot(0, this, [this] {
                m_excerptRequestScheduled = false;
                const_cast<DocumentModel *>(this)->requestExcerpts();
            });
        }
    }
    return {};
}

void DocumentModel::requestExcerpts()
{
    if (m_excerptsUnderWay || m_excerptsWanted.isEmpty() || !m_thread.isRunning()) {
        return;
    }
    // The rows asked for last are the ones in view. Those before them have been scrolled
    // past; they are asked for again should they come back.
    const qsizetype skipped = qMax<qsizetype>(0, m_excerptsWanted.size() - ExcerptsAtOnce);
    for (qsizetype i = 0; i < skipped; ++i) {
        m_excerptsAsked.remove(m_excerptsWanted.at(i));
    }
    const QList<qint64> ids = m_excerptsWanted.mid(skipped);
    m_excerptsWanted.clear();
    m_excerptsUnderWay = true;
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, ticket = m_tickets.excerpts.load(), ids, terms = m_excerptTerms] {
            worker->excerpts(ticket, ids, terms);
        },
        Qt::QueuedConnection);
}

void DocumentModel::forgetExcerpts()
{
    ++m_tickets.excerpts; // whatever is being looked up is no longer waited for
    m_excerpts.clear();
    m_excerptsWanted.clear();
    m_excerptsAsked.clear();
    m_excerptsUnderWay = false;
}

void DocumentModel::setLoading(bool loading)
{
    const bool was = isLoading();
    m_loading = loading;
    if (was != isLoading()) {
        Q_EMIT loadingChanged();
    }
}

void DocumentModel::setOpening(bool opening)
{
    if (m_opening != opening) {
        m_opening = opening;
        Q_EMIT openingChanged();
    }
}

void DocumentModel::loaded()
{
    setLoading(false);
    setOpening(false);
    if (m_wantedId != 0) {
        const qint64 wanted = m_wantedId;
        m_wantedId = 0;
        selectId(wanted);
    }
    if (m_refreshWanted) {
        m_refreshWanted = false;
        load(Load::Refreshed, true);
    }
}

void DocumentModel::setActive(bool active)
{
    if (m_active == active) {
        return;
    }
    m_active = active;
    Q_EMIT activeChanged();
    if (active) {
        load(Load::Navigated);
        return;
    }
    // Whatever is still being searched for is no longer wanted, and nothing is listed. A
    // list that shows nothing has nothing to open either.
    clear();
    m_refreshWanted = false;
    setOpening(false);
}

void DocumentModel::clear()
{
    ++m_tickets.search;
    m_wantedId = 0;
    forgetExcerpts();
    setLoading(false);
    m_lingering.clear();
    m_handled.clear();
    m_dismissed.clear();
    const bool changed = !m_rows.isEmpty() || m_truncated;
    beginResetModel();
    m_rows.clear();
    indexRows();
    endResetModel();
    m_truncated = false;
    if (changed) {
        Q_EMIT countChanged();
    }
    setSelection({}, 0);
}

void DocumentModel::setScope(const QString &scope)
{
    if (m_scope == scope) {
        return;
    }
    m_scope = scope;
    Q_EMIT scopeChanged();
    load(Load::Navigated);
}

void DocumentModel::setFilter(const QString &filter)
{
    if (m_filter == filter) {
        return;
    }
    m_filter = filter;
    load(Load::Navigated);
    Q_EMIT filterChanged();
}

void DocumentModel::setCriteria(const QString &criteria)
{
    if (m_criteria == criteria) {
        return;
    }
    m_criteria = criteria;
    load(Load::Navigated);
    Q_EMIT filterChanged();
}

QString DocumentModel::effectiveFilter() const
{
    // Two queries side by side are both required, also where either has an OR in it: OR only
    // joins the terms right next to it.
    const QString filter = m_filter.trimmed();
    const QString criteria = m_criteria.trimmed();
    return filter.isEmpty() ? criteria : criteria.isEmpty() ? filter : QString(filter + u' ' + criteria);
}

void DocumentModel::setSortColumn(const QString &column)
{
    if (m_sortColumn == column) {
        return;
    }
    m_sortColumn = column;
    Q_EMIT sortChanged();
    reload();
}

void DocumentModel::setSortDescending(bool descending)
{
    if (m_sortDescending == descending) {
        return;
    }
    m_sortDescending = descending;
    Q_EMIT sortChanged();
    reload();
}

void DocumentModel::reload()
{
    // The same documents, in another order perhaps.
    load(Load::Lingering);
}

void DocumentModel::letGo(const QList<qint64> &ids)
{
    for (qint64 id : ids) {
        m_dismissed.insert(id);
        m_lingering.remove(id);
    }
}

QSet<qint64> DocumentModel::leavers(const QList<Row> &found) const
{
    // Only what the user had a hand on: what the indexer or a rule takes out of a list in
    // the background is not held back. A list that is cut short tells nothing about what
    // left it.
    if (m_truncated || m_handled.isEmpty()) {
        return {};
    }
    QSet<qint64> still;
    still.reserve(found.size());
    for (const Row &row : found) {
        still.insert(row.id);
    }
    QSet<qint64> gone;
    for (qint64 id : m_handled) {
        const auto row = m_rowOfId.constFind(id);
        // Not what is listed inside a mailbox: that comes and goes with the mailbox.
        if (row != m_rowOfId.constEnd() && m_rows.at(*row).depth == 0 && !still.contains(id) && !m_dismissed.contains(id)) {
            gone.insert(id);
        }
    }
    return gone;
}

void DocumentModel::load(Load load, bool quiet)
{
    if (!m_active) {
        return;
    }
    if (m_loading && m_load == Load::Navigated) {
        // The list of the place that was just gone to is not there yet. Whatever is asked
        // for now is still that list, for the first time.
        load = Load::Navigated;
    }
    if (load == Load::Navigated) {
        // Somewhere else, or after something else: nothing is held back any longer.
        m_lingering.clear();
        m_dismissed.clear();
        m_handled = m_selected;
        m_wantedId = 0;
    }
    // Whatever changed in the index, this reads it.
    m_refreshWanted = false;
    if (m_quiet != quiet) {
        const bool was = isLoading();
        m_quiet = quiet;
        if (was != isLoading()) {
            Q_EMIT loadingChanged();
        }
    }
    m_load = load;
    const SearchQuery scopeQuery(m_scope);
    const SearchQuery filterQuery(effectiveFilter());
    const SearchQuery query = SearchQuery::combine(scopeQuery, filterQuery);

    QString where = query.where();
    // Places, tags, groups and filters on what is known about a document are answered from
    // the list of documents alone, at once. Words are looked up in the text of all documents.
    const bool plain = scopeQuery.ftsExpression().isEmpty() && filterQuery.ftsExpression().isEmpty();
    if (plain && !m_lingering.isEmpty()) {
        // A document that belongs into the list again is not held back: it is simply there.
        QSqlQuery back = Database::exec(u"SELECT d.id FROM documents d WHERE (%1) AND d.id IN (%2)"_s.arg(where, Store::idList(m_lingering.values())), query.binds());
        while (back.next()) {
            m_lingering.remove(back.value(0).toLongLong());
        }
    }
    if (!m_lingering.isEmpty()) {
        // Whatever is held back is listed as if it still belonged, in its place in the order.
        // Not what went into the trash or came out of it, though: that is meant to leave.
        const QString held = u"d.id IN (%1)"_s.arg(Store::idList(m_lingering.values()));
        where = query.includesTrash() ? u"((%1) OR (%2 AND d.trashed <> 0))"_s.arg(where, held) : u"((%1) OR %2)"_s.arg(where, held);
    }
    if (!query.includesTrash()) {
        where += " AND d.trashed = 0"_L1;
    }
    // Messages of mailboxes are documents too, but they are only listed among the others
    // where the search asks for something of theirs. Otherwise they are found under their
    // mailbox, which opens like a folder.
    m_flat = query.reachesInside();
    if (!m_flat) {
        where += " AND d.parent_id = 0"_L1;
    }
    const QString sql = rowsSql(where);

    m_terms = filterQuery.terms();
    // Whatever is still being searched for is overtaken by this.
    const quint64 ticket = ++m_tickets.search;

    // Every list is read in the search thread. Words are looked up in the text of all
    // documents, which can take seconds in a large index; and a list of the documents alone
    // (a place, a tag, a filter on what is known about a document) is a walk through all of
    // them, a second for the duplicates among some hundred thousand. The list keeps what it
    // shows until then.
    m_pendingExcerptTerms = filterQuery.ftsExpression().isEmpty() ? QStringList() : filterQuery.terms();
    const auto ask = [this, ticket, sql, binds = query.binds()] {
        QMetaObject::invokeMethod(
            m_worker,
            [worker = m_worker, ticket, sql, binds] {
                worker->search(ticket, sql, binds, MaxRows);
            },
            Qt::QueuedConnection);
    };
    if (!plain || m_opening || !m_thread.isRunning()) {
        // Nothing to wait for: a search takes its time, and of an index that was just
        // opened nothing is in memory, so that waiting would keep the window from coming up.
        setLoading(true);
        ask();
        return;
    }
    // Most such lists are there within milliseconds, and then they are shown here and now:
    // whoever asked finds the list read when this returns, and nothing flickers.
    QList<Row> rows;
    bool truncated = false;
    bool there = false;
    {
        QMutexLocker locker(&m_handover.mutex);
        m_handover.awaited = ticket;
        m_handover.there = false;
        ask();
        m_handover.arrived.wait(&m_handover.mutex, QDeadlineTimer(QuickAnswerMs));
        m_handover.awaited = 0;
        there = m_handover.there;
        if (there) {
            rows = std::move(m_handover.rows);
            truncated = m_handover.truncated;
            m_handover.rows.clear();
            m_handover.there = false;
        }
    }
    if (!there) {
        setLoading(true); // it comes with found()
        return;
    }
    apply(rows, truncated, {});
    if (ticket == m_tickets.search) {
        loaded();
    }
}

void DocumentModel::apply(const QList<Row> &found, bool foundTruncated, const QStringList &excerptTerms)
{
    if (m_load == Load::Refreshed && !foundTruncated) {
        const QSet<qint64> gone = leavers(found);
        m_dismissed.clear();
        if (!gone.isEmpty()) {
            // Documents the user is working with would drop out of the list. It is read once
            // more with them in it, and only that is shown.
            m_lingering.unite(gone);
            load(Load::Lingering, m_quiet);
            return;
        }
    }
    if (!m_lingering.isEmpty()) {
        // What has gone for good, or belongs again, is not held back any more ...
        QSet<qint64> listed;
        for (const Row &row : found) {
            if (m_lingering.contains(row.id)) {
                listed.insert(row.id);
            }
        }
        m_lingering = listed;
    }
    // ... and from here on, what is selected is what the user may be working with.
    m_handled = m_selected;
    if (m_rowOfId.contains(m_current)) {
        m_handled.insert(m_current);
    }

    // Below every document that is open, what is in it.
    QList<Row> rows;
    bool truncated = foundTruncated;
    bool tree = false;
    if (m_flat) {
        rows = found;
    } else {
        rows.reserve(found.size());
        for (const Row &row : found) {
            rows.append(row);
            tree = tree || row.parts > 0;
            if (row.parts > 0 && m_expanded.contains(row.id)) {
                bool partsTruncated = false;
                rows.append(partsOf(row.id, &partsTruncated));
                truncated = truncated || partsTruncated;
            }
        }
    }
    if (tree != m_tree) {
        m_tree = tree;
        Q_EMIT treeChanged();
    }

    const int oldCount = count();
    const bool sameOrder = rows.size() == m_rows.size() && std::equal(rows.cbegin(), rows.cend(), m_rows.cbegin(), [](const Row &a, const Row &b) {
        return a.id == b.id;
    });

    if (m_excerptTerms != excerptTerms) {
        // Passages found for the same words stay: the list is read anew whenever something
        // in the index changes, and they would flicker with every file that is indexed.
        forgetExcerpts();
        m_excerptTerms = excerptTerms;
    }

    if (sameOrder) {
        // Keep the view where it is; only the contents of the rows may have changed.
        m_rows = rows;
        if (!m_rows.isEmpty()) {
            Q_EMIT dataChanged(index(0), index(int(m_rows.size()) - 1));
        }
    } else {
        beginResetModel();
        m_rows = rows;
        indexRows();
        endResetModel();
    }
    if (m_truncated != truncated || oldCount != count()) {
        m_truncated = truncated;
        Q_EMIT countChanged();
    }

    // Documents that left the list also leave the selection.
    QSet<qint64> selected;
    for (qint64 id : std::as_const(m_selected)) {
        if (m_rowOfId.contains(id)) {
            selected.insert(id);
        }
    }
    qint64 current = m_rowOfId.contains(m_current) ? m_current : 0;
    if (current == 0 && !selected.isEmpty()) {
        current = *selected.constBegin();
    }
    setSelection(selected, current);
}

void DocumentModel::setSelection(const QSet<qint64> &selected, qint64 current)
{
    const bool selectionDiffers = selected != m_selected;
    m_handled.unite(selected);
    if (current != 0) {
        m_handled.insert(current);
    }
    if (selectionDiffers) {
        const QSet<qint64> touched = (m_selected - selected) + (selected - m_selected);
        m_selected = selected;
        for (qint64 id : touched) {
            const auto row = m_rowOfId.constFind(id);
            if (row != m_rowOfId.constEnd()) {
                Q_EMIT dataChanged(index(*row), index(*row), {SelectedRole});
            }
        }
        Q_EMIT selectionChanged();
    }
    if (current != m_current) {
        m_current = current;
        Q_EMIT currentChanged();
    }
}

QVariantList DocumentModel::selectedIds() const
{
    QVariantList result;
    const QList<qint64> ids = selection();
    for (qint64 id : ids) {
        result.append(id);
    }
    return result;
}

QList<qint64> DocumentModel::selection() const
{
    // In list order, which is what the user sees.
    QList<qint64> result;
    result.reserve(m_selected.size());
    for (const Row &row : m_rows) {
        if (m_selected.contains(row.id)) {
            result.append(row.id);
        }
    }
    return result;
}

int DocumentModel::currentRow() const
{
    return m_rowOfId.value(m_current, -1);
}

void DocumentModel::select(int row, SelectionMode mode)
{
    if (row < 0 || row >= m_rows.size()) {
        return;
    }
    const qint64 id = m_rows.at(row).id;
    QSet<qint64> selected = m_selected;
    switch (mode) {
    case Single:
        selected = {id};
        m_anchor = id;
        break;
    case Toggle:
        if (!selected.remove(id)) {
            selected.insert(id);
        }
        m_anchor = id;
        break;
    case Range: {
        const int anchorRow = m_rowOfId.value(m_anchor, row);
        selected.clear();
        for (int i = qMin(anchorRow, row); i <= qMax(anchorRow, row); ++i) {
            selected.insert(m_rows.at(i).id);
        }
        break;
    }
    }
    setSelection(selected, id);
}

void DocumentModel::selectAll()
{
    QSet<qint64> selected;
    selected.reserve(m_rows.size());
    for (const Row &row : std::as_const(m_rows)) {
        selected.insert(row.id);
    }
    setSelection(selected, m_current != 0 || m_rows.isEmpty() ? m_current : m_rows.first().id);
}

void DocumentModel::clearSelection()
{
    setSelection({}, 0);
}

void DocumentModel::selectId(qint64 id)
{
    if (m_rowOfId.contains(id)) {
        m_anchor = id;
        setSelection({id}, id);
    } else if (m_loading) {
        // The list it may be in is on its way.
        m_wantedId = id;
    }
}

bool DocumentModel::isSelected(int row) const
{
    return row >= 0 && row < m_rows.size() && m_selected.contains(m_rows.at(row).id);
}

int DocumentModel::rowOf(qint64 id) const
{
    return m_rowOfId.value(id, -1);
}

qint64 DocumentModel::idAt(int row) const
{
    return row >= 0 && row < m_rows.size() ? m_rows.at(row).id : 0;
}

QList<QUrl> DocumentModel::selectedUrls() const
{
    QList<QUrl> urls;
    const QList<qint64> ids = selection();
    for (qint64 id : ids) {
        const Document doc = Store::document(id);
        // A message of a mailbox is taken out of it, to be dragged as a file of its own.
        const QString path = Store::ensureContent(doc);
        if (path.isEmpty() && doc.isPart()) {
            continue;
        }
        urls.append(path.isEmpty() ? doc.url : QUrl::fromLocalFile(path));
    }
    return urls;
}

#include "moc_documentmodel.cpp"
