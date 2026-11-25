// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "sidebarmodel.h"

#include "library.h"
#include "searchquery.h"

#include <KLocalizedString>

#include <QDeadlineTimer>

using namespace Indeks;
using namespace Qt::StringLiterals;

namespace
{
// However long a round of counting took, the next one does not wait longer than this.
constexpr qint64 MaxCountPauseMs = 5000;
}

SidebarModel::SidebarModel(Library *library, QObject *parent)
    : QAbstractListModel(parent)
    , m_library(library)
{
    qRegisterMetaType<QHash<QString, int>>();

    m_thread.setObjectName(u"indeks-count"_s);
    m_worker = new CountWorker(&m_ticket);
    m_worker->moveToThread(&m_thread);
    // Runs in the counting thread itself, as its last act.
    connect(&m_thread, &QThread::finished, m_worker, &CountWorker::close, Qt::DirectConnection);
    connect(m_worker, &CountWorker::counted, this, [this](quint64 ticket, const QHash<QString, int> &counts, qint64 elapsed) {
        if (ticket == m_ticket) {
            applyCounts(counts, elapsed);
        }
    });
    m_thread.start();

    m_countTimer.setSingleShot(true);
    connect(&m_countTimer, &QTimer::timeout, this, &SidebarModel::startCounting);

    connect(library, &Library::changed, this, &SidebarModel::reload);
    connect(library, &Library::sourcesChanged, this, &SidebarModel::reload);
    connect(library, &Library::databaseAboutToChange, this, [this] {
        // What is being counted belongs to the database that is being left, and the counting
        // thread must let go of that database's file. The numbers go with it.
        ++m_ticket;
        m_counting = false;
        m_countAgain = false;
        m_countTimer.stop();
        m_countDuration = 0;
        m_counts.clear();
        if (m_thread.isRunning()) {
            QMetaObject::invokeMethod(m_worker, &CountWorker::close, Qt::BlockingQueuedConnection);
        }
        if (!m_opening) {
            m_opening = true;
            Q_EMIT openingChanged();
        }
    });
    reload();
}

SidebarModel::~SidebarModel()
{
    stopCounting();
    delete m_worker;
}

bool SidebarModel::stopCounting(int timeoutMs)
{
    ++m_ticket;
    m_counting = false;
    m_countAgain = false;
    m_countTimer.stop();
    m_thread.quit();
    return m_thread.wait(timeoutMs < 0 ? QDeadlineTimer(QDeadlineTimer::Forever) : QDeadlineTimer(timeoutMs));
}

int SidebarModel::countOf(const QString &key) const
{
    return m_counts.value(key, -1);
}

void SidebarModel::requestCounts()
{
    if (m_counting) {
        m_countAgain = true;
        return;
    }
    if (m_countTimer.isActive() || !m_thread.isRunning()) {
        return;
    }
    const qint64 pause = m_sinceCounted.isValid() ? qMin(m_countDuration, MaxCountPauseMs) - m_sinceCounted.elapsed() : 0;
    if (pause > 0) {
        m_countTimer.start(int(pause));
        return;
    }
    startCounting();
}

void SidebarModel::startCounting()
{
    if (m_counting || !m_thread.isRunning()) {
        return;
    }
    m_counting = true;
    m_countAgain = false;
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, ticket = m_ticket.load(), wanted = m_wanted] {
            worker->count(ticket, wanted);
        },
        Qt::QueuedConnection);
}

void SidebarModel::applyCounts(const QHash<QString, int> &counts, qint64 elapsed)
{
    m_counting = false;
    m_countDuration = elapsed;
    m_sinceCounted.start();
    m_counts = counts;

    // The files that are missing have an entry only while there are any: the one number
    // that changes what is listed. Everything else is a number in its place.
    const QString missingKey = u"smart:missing"_s;
    const bool missingListed = rowOfKey(missingKey) >= 0;
    const bool missingWanted = counts.contains(missingKey) && (counts.value(missingKey) > 0 || m_currentKey == missingKey);
    if (missingListed != missingWanted) {
        rebuild();
    } else {
        for (Item &item : m_items) {
            const auto count = counts.constFind(item.key);
            if (item.header) {
                continue;
            }
            if (count != counts.constEnd()) {
                item.count = *count;
            } else if (item.kind == "source"_L1) {
                item.count = 0; // a location without documents is not counted
            }
        }
        if (!m_items.isEmpty()) {
            Q_EMIT dataChanged(index(0), index(int(m_items.size()) - 1), {CountRole});
        }
    }
    Q_EMIT countsChanged();
    if (m_opening) {
        m_opening = false;
        Q_EMIT openingChanged();
    }
    if (m_countAgain) {
        m_countAgain = false;
        requestCounts();
    }
}

int SidebarModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_items.size());
}

QHash<int, QByteArray> SidebarModel::roleNames() const
{
    return {
        {KeyRole, "key"},
        {LabelRole, "label"},
        {IconRole, "iconName"},
        {CountRole, "count"},
        {QueryRole, "query"},
        {DepthRole, "depth"},
        {KindRole, "kind"},
        {ItemIdRole, "itemId"},
        {HeaderRole, "isHeader"},
        {CollapsedRole, "collapsed"},
        {OfflineRole, "offline"},
        {CurrentRole, "isCurrent"},
    };
}

QVariant SidebarModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_items.size()) {
        return {};
    }
    const Item &item = m_items.at(index.row());
    switch (role) {
    case KeyRole:
        return item.key;
    case Qt::DisplayRole:
    case LabelRole:
        return item.label;
    case IconRole:
        return item.icon;
    case CountRole:
        return item.count;
    case QueryRole:
        return item.query;
    case DepthRole:
        return item.depth;
    case KindRole:
        return item.kind;
    case ItemIdRole:
        return item.id;
    case HeaderRole:
        return item.header;
    case CollapsedRole:
        return m_collapsed.contains(item.section);
    case OfflineRole:
        return item.offline;
    case CurrentRole:
        return !item.header && item.key == m_currentKey;
    }
    return {};
}

void SidebarModel::addHeader(QList<Item> &items, const QString &section, const QString &label) const
{
    Item header;
    header.key = "header:"_L1 + section;
    header.label = label;
    header.section = section;
    header.header = true;
    items.append(header);
}

void SidebarModel::reload()
{
    rebuild();
    requestCounts();
}

void SidebarModel::rebuild()
{
    QList<Item> items;
    QList<CountRequest> wanted;
    auto add = [&](const QString &section, const QString &kind, const QString &key, const QString &label, const QString &icon, const QString &query) -> Item * {
        if (m_collapsed.contains(section)) {
            return nullptr;
        }
        Item item;
        item.section = section;
        item.kind = kind;
        item.key = key;
        item.label = label;
        item.icon = icon;
        item.query = query;
        items.append(item);
        return &items.last();
    };
    auto addCounted = [&](const QString &section, const QString &kind, const QString &key, const QString &label, const QString &icon, const QString &query) {
        if (Item *item = add(section, kind, key, label, icon, query)) {
            // Counted in the background; the number that is known stands in until then.
            item->count = m_counts.value(key, -1);
            wanted.append({key, query});
        }
    };

    // Fixed places
    const QString library = u"library"_s;
    addCounted(library, u"place"_s, u"place:inbox"_s, i18nc("@item sidebar", "Inbox"), u"mail-folder-inbox"_s, u"is:inbox"_s);
    addCounted(library, u"place"_s, u"place:all"_s, i18nc("@item sidebar", "All Documents"), u"folder-documents"_s, QString());
    addCounted(library, u"place"_s, u"place:flagged"_s, i18nc("@item sidebar", "Pinned"), u"pin"_s, u"is:pinned"_s);
    addCounted(library, u"place"_s, u"place:trash"_s, i18nc("@item sidebar", "Trash"), u"user-trash"_s, u"is:trashed"_s);

    // Smart groups: built-in searches
    const QString smart = u"smart"_s;
    addHeader(items, smart, i18nc("@title:group sidebar", "Smart Groups"));
    addCounted(smart, u"smart"_s, u"smart:today"_s, i18nc("@item sidebar", "Added Today"), u"view-calendar-day"_s, u"added:today"_s);
    addCounted(smart, u"smart"_s, u"smart:week"_s, i18nc("@item sidebar", "Added This Week"), u"view-calendar-week"_s, u"added:7d"_s);
    addCounted(smart, u"smart"_s, u"smart:unfiled"_s, i18nc("@item sidebar", "Not in a Group"), u"folder-open"_s, u"is:unfiled"_s);
    addCounted(smart, u"smart"_s, u"smart:untagged"_s, i18nc("@item sidebar", "Untagged"), u"tag"_s, u"is:untagged"_s);
    addCounted(smart, u"smart"_s, u"smart:annotated"_s, i18nc("@item sidebar", "Annotated"), u"draw-freehand"_s, u"is:annotated"_s);
    addCounted(smart, u"smart"_s, u"smart:duplicates"_s, i18nc("@item sidebar", "Duplicates"), u"edit-copy"_s, u"is:duplicate"_s);
    if (!m_collapsed.contains(smart)) {
        wanted.append({u"smart:missing"_s, u"is:missing"_s});
        const int missing = m_counts.value(u"smart:missing"_s, 0);
        if (missing > 0 || m_currentKey == "smart:missing"_L1) {
            if (Item *item = add(smart, u"smart"_s, u"smart:missing"_s, i18nc("@item sidebar", "Missing Files"), u"dialog-warning"_s, u"is:missing"_s)) {
                item->count = missing;
            }
        }
    }

    // Saved searches
    const QString searches = u"searches"_s;
    addHeader(items, searches, i18nc("@title:group sidebar", "Saved Searches"));
    const QVariantList savedSearches = m_library->savedSearches();
    for (const QVariant &entry : savedSearches) {
        const QVariantMap search = entry.toMap();
        const QString query = search.value(u"query"_s).toString();
        const QString key = "search:"_L1 + search.value(u"id"_s).toString();
        if (Item *item = add(searches, u"search"_s, key, search.value(u"name"_s).toString(), u"search"_s, query)) {
            item->id = search.value(u"id"_s).toLongLong();
            item->count = m_counts.value(key, -1);
            wanted.append({key, query});
        }
    }

    // Groups
    const QString groups = u"groups"_s;
    addHeader(items, groups, i18nc("@title:group sidebar", "Groups"));
    const QVariantList groupList = m_library->groups();
    for (const QVariant &entry : groupList) {
        const QVariantMap group = entry.toMap();
        const QString id = group.value(u"id"_s).toString();
        if (Item *item = add(groups, u"group"_s, "group:"_L1 + id, group.value(u"name"_s).toString(), u"folder"_s, "groupid:"_L1 + id)) {
            item->id = id.toLongLong();
            item->count = group.value(u"count"_s).toInt();
            item->depth = group.value(u"depth"_s).toInt();
        }
    }

    // Tags
    const QString tags = u"tags"_s;
    addHeader(items, tags, i18nc("@title:group sidebar", "Tags"));
    const QVariantList tagList = m_library->tags();
    for (const QVariant &entry : tagList) {
        const QVariantMap tag = entry.toMap();
        const QString id = tag.value(u"id"_s).toString();
        if (Item *item = add(tags, u"tag"_s, "tag:"_L1 + id, tag.value(u"name"_s).toString(), u"tag"_s, "tagid:"_L1 + id)) {
            item->id = id.toLongLong();
            item->count = tag.value(u"count"_s).toInt();
        }
    }

    // Where the documents are
    const QString sources = u"sources"_s;
    addHeader(items, sources, i18nc("@title:group sidebar", "Locations"));
    addCounted(sources, u"place"_s, u"place:local"_s, i18nc("@item sidebar", "Stored in Indeks"), u"folder-database"_s, u"is:local"_s);
    // Their numbers come with the others, from the background.
    const QVariantList sourceList = m_library->sources(false);
    for (const QVariant &entry : sourceList) {
        const QVariantMap source = entry.toMap();
        const QString id = source.value(u"id"_s).toString();
        const bool local = source.value(u"local"_s).toBool();
        const QString key = CountWorker::sourceKeyPrefix() + id;
        if (Item *item = add(sources, u"source"_s, key, source.value(u"name"_s).toString(), local ? u"folder"_s : u"folder-remote"_s, "sourceid:"_L1 + id)) {
            item->id = id.toLongLong();
            item->count = m_counts.value(key, 0);
            item->offline = !source.value(u"available"_s).toBool();
        }
    }

    m_wanted = wanted;

    const bool sameShape = items.size() == m_items.size() && std::equal(items.cbegin(), items.cend(), m_items.cbegin(), [](const Item &a, const Item &b) {
        return a.key == b.key;
    });
    const QString oldQuery = currentQuery();
    const QString oldLabel = currentLabel();
    if (sameShape) {
        m_items = items;
        Q_EMIT dataChanged(index(0), index(int(m_items.size()) - 1));
    } else {
        beginResetModel();
        m_items = items;
        endResetModel();
    }

    // The current entry may be gone, e.g. a deleted tag. Collapsed sections keep their selection.
    const bool currentHidden = !m_currentKey.startsWith("place:"_L1) && !current();
    bool currentCollapsed = false;
    if (currentHidden) {
        const QString prefix = m_currentKey.section(u':', 0, 0);
        static const QHash<QString, QString> sectionOfKind = {
            {u"smart"_s, u"smart"_s}, {u"search"_s, u"searches"_s}, {u"group"_s, u"groups"_s},
            {u"tag"_s, u"tags"_s},     {u"source"_s, u"sources"_s},
        };
        currentCollapsed = m_collapsed.contains(sectionOfKind.value(prefix));
    }
    rememberCurrent();
    if (currentHidden && !currentCollapsed) {
        setCurrentKey(u"place:all"_s);
    } else if (oldQuery != currentQuery() || oldLabel != currentLabel()) {
        Q_EMIT currentChanged();
    }
}

void SidebarModel::rememberCurrent()
{
    if (const Item *item = current()) {
        m_lastQuery = item->query;
        m_lastLabel = item->label;
    }
}

const SidebarModel::Item *SidebarModel::current() const
{
    for (const Item &item : m_items) {
        if (!item.header && item.key == m_currentKey) {
            return &item;
        }
    }
    return nullptr;
}

void SidebarModel::setCurrentKey(const QString &key)
{
    if (m_currentKey == key) {
        return;
    }
    if (rowOfKey(key) < 0) {
        reload(); // the entry may be brand new, e.g. a group that was just created
    }
    const int oldRow = rowOfKey(m_currentKey);
    m_currentKey = key;
    rememberCurrent();
    const int newRow = rowOfKey(key);
    if (oldRow >= 0) {
        Q_EMIT dataChanged(index(oldRow), index(oldRow), {CurrentRole});
    }
    if (newRow >= 0) {
        Q_EMIT dataChanged(index(newRow), index(newRow), {CurrentRole});
    }
    Q_EMIT currentChanged();
}

QString SidebarModel::currentQuery() const
{
    const Item *item = current();
    return item ? item->query : m_lastQuery;
}

QString SidebarModel::currentLabel() const
{
    const Item *item = current();
    return item ? item->label : m_lastLabel;
}

QString SidebarModel::currentIcon() const
{
    const Item *item = current();
    return item && !item->icon.isEmpty() ? item->icon : u"folder-documents"_s;
}

QString SidebarModel::currentKind() const
{
    return m_currentKey.section(u':', 0, 0);
}

qint64 SidebarModel::currentId() const
{
    const QString kind = currentKind();
    return kind == "place"_L1 || kind == "smart"_L1 ? 0 : m_currentKey.section(u':', 1).toLongLong();
}

void SidebarModel::setCollapsedSections(const QStringList &sections)
{
    if (m_collapsed == sections) {
        return;
    }
    m_collapsed = sections;
    Q_EMIT collapsedSectionsChanged();
    reload();
}

void SidebarModel::toggleSection(const QString &section)
{
    QStringList sections = m_collapsed;
    if (!sections.removeOne(section)) {
        sections.append(section);
    }
    setCollapsedSections(sections);
}

int SidebarModel::rowOfKey(const QString &key) const
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (!m_items.at(i).header && m_items.at(i).key == key) {
            return i;
        }
    }
    return -1;
}

int SidebarModel::step(int delta)
{
    if (delta == 0) {
        return -1;
    }
    for (int row = rowOfKey(m_currentKey) + delta; row >= 0 && row < m_items.size(); row += delta) {
        if (!m_items.at(row).header) {
            setCurrentKey(m_items.at(row).key);
            return row;
        }
    }
    return -1;
}

#include "moc_sidebarmodel.cpp"
