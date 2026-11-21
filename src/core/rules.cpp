// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "rules.h"

#include "database.h"
#include "scripts.h"
#include "searchquery.h"
#include "store.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>

using namespace Qt::StringLiterals;

namespace Indeks::Rules
{

namespace
{
const QLatin1StringView ruleColumns("id, name, query, enabled, on_import, filters, actions");

QVariantList jsonList(const QString &json)
{
    return QJsonDocument::fromJson(json.toUtf8()).array().toVariantList();
}

QVariantMap readRule(const QSqlQuery &query)
{
    return {
        {u"id"_s, query.value(0)},
        {u"name"_s, query.value(1)},
        {u"query"_s, query.value(2)},
        {u"enabled"_s, query.value(3).toBool()},
        {u"onImport"_s, query.value(4).toBool()},
        {u"filters"_s, jsonList(query.value(5).toString())},
        {u"actions"_s, jsonList(query.value(6).toString())},
    };
}

QStringList pathOf(const QVariantMap &action)
{
    QStringList path;
    const QVariantList parts = action.value(u"path"_s).toList();
    for (const QVariant &part : parts) {
        const QString name = part.toString().simplified();
        if (!name.isEmpty()) {
            path.append(name);
        }
    }
    return path;
}
}

QVariantMap rule(qint64 id)
{
    QSqlQuery query = Database::exec(u"SELECT %1 FROM rules WHERE id = ?"_s.arg(ruleColumns), {id});
    return query.next() ? readRule(query) : QVariantMap();
}

QVariantList all()
{
    QVariantList result;
    QSqlQuery query = Database::exec(u"SELECT %1 FROM rules ORDER BY name COLLATE NOCASE"_s.arg(ruleColumns));
    while (query.next()) {
        result.append(readRule(query));
    }
    return result;
}

QJsonArray storable(const QVariantList &given)
{
    QJsonArray actions;
    for (const QVariant &entry : given) {
        const QVariantMap action = entry.toMap();
        const QString type = action.value(u"type"_s).toString();
        if (type == "tag"_L1 || type == "untag"_L1) {
            const QString tag = action.value(u"name"_s).toString().simplified();
            if (!tag.isEmpty()) {
                actions.append(QJsonObject{{u"type"_s, type}, {u"name"_s, tag}});
            }
        } else if (type == "group"_L1 || type == "ungroup"_L1) {
            const qint64 group = action.value(u"id"_s).toLongLong();
            if (group > 0) {
                actions.append(QJsonObject{{u"type"_s, type}, {u"id"_s, group}});
            }
        } else if (type == "flag"_L1 || type == "unflag"_L1 || type == "inbox"_L1) {
            actions.append(QJsonObject{{u"type"_s, type}});
        } else if (type == "script"_L1) {
            const QString script = action.value(u"name"_s).toString();
            if (Scripts::isFileName(script)) {
                actions.append(QJsonObject{{u"type"_s, type}, {u"name"_s, script}});
            }
        }
    }
    return actions;
}

bool runScripts(const QVariantList &actions)
{
    for (const QVariant &action : actions) {
        if (action.toMap().value(u"type"_s).toString() == "script"_L1) {
            return true;
        }
    }
    return false;
}

QList<qint64> matching(const QString &queryText, const QList<qint64> &restrictTo)
{
    const SearchQuery query(queryText);
    if (query.isEmpty()) {
        return {}; // an empty condition would match everything
    }
    QString sql = u"SELECT d.id FROM documents d WHERE (%1) AND d.trashed = 0"_s.arg(query.where());
    if (!query.reachesInside()) {
        sql += " AND d.parent_id = 0"_L1;
    }
    if (!restrictTo.isEmpty()) {
        sql += u" AND d.id IN (%1)"_s.arg(Store::idList(restrictTo));
    }
    QList<qint64> matches;
    QSqlQuery select = Database::exec(sql, query.binds());
    while (select.next()) {
        matches.append(select.value(0).toLongLong());
    }
    return matches;
}

qint64 tagId(const QString &rawName, bool create)
{
    QString name = rawName.simplified();
    name.remove(u'"');
    if (name.isEmpty()) {
        return 0;
    }
    const qint64 existing = Database::value(u"SELECT id FROM tags WHERE name = ? COLLATE NOCASE"_s, {name}).toLongLong();
    if (existing != 0 || !create) {
        return existing;
    }
    return Database::exec(u"INSERT INTO tags (name) VALUES (?)"_s, {name}).lastInsertId().toLongLong();
}

qint64 groupId(const QStringList &path, bool create)
{
    qint64 parent = 0;
    for (const QString &rawName : path) {
        const QString name = rawName.simplified();
        if (name.isEmpty()) {
            return 0;
        }
        qint64 id = Database::value(u"SELECT id FROM folders WHERE name = ? COLLATE NOCASE AND COALESCE(parent_id, 0) = ? ORDER BY id LIMIT 1"_s, {name, parent})
                        .toLongLong();
        if (id == 0) {
            if (!create) {
                return 0;
            }
            id = Database::exec(u"INSERT INTO folders (name, parent_id) VALUES (?, ?)"_s, {name, parent > 0 ? QVariant(parent) : QVariant()})
                     .lastInsertId()
                     .toLongLong();
        }
        parent = id;
    }
    return parent;
}

QHash<qint64, QStringList> groupPaths()
{
    QHash<qint64, std::pair<qint64, QString>> groups; // id to parent and name
    QSqlQuery query = Database::exec(u"SELECT id, COALESCE(parent_id, 0), name FROM folders"_s);
    while (query.next()) {
        groups.insert(query.value(0).toLongLong(), {query.value(1).toLongLong(), query.value(2).toString()});
    }
    QHash<qint64, QStringList> paths;
    for (auto it = groups.cbegin(); it != groups.cend(); ++it) {
        QStringList path;
        qint64 id = it.key();
        for (int depth = 0; id != 0 && groups.contains(id) && depth < 64; ++depth) {
            path.prepend(groups.value(id).second);
            id = groups.value(id).first;
        }
        paths.insert(it.key(), path);
    }
    return paths;
}

Outcome apply(const QList<qint64> &documents, const QVariantList &actions)
{
    Outcome outcome;
    if (documents.isEmpty() || actions.isEmpty()) {
        return outcome;
    }
    const QString idList = Store::idList(documents);
    bool notesChanged = false;

    Transaction transaction;
    for (const QVariant &entry : actions) {
        const QVariantMap action = entry.toMap();
        const QString type = action.value(u"type"_s).toString();
        if (type == "tag"_L1) {
            const QString name = action.value(u"name"_s).toString();
            qint64 tag = tagId(name, false);
            if (tag == 0) {
                tag = tagId(name, true);
                outcome.made = outcome.made || tag != 0;
            }
            if (tag != 0) {
                for (qint64 doc : documents) {
                    Database::exec(u"INSERT OR IGNORE INTO document_tags (doc_id, tag_id) VALUES (?, ?)"_s, {doc, tag});
                }
            }
        } else if (type == "untag"_L1) {
            const qint64 tag = tagId(action.value(u"name"_s).toString(), false);
            if (tag != 0) {
                Database::exec(u"DELETE FROM document_tags WHERE tag_id = ? AND doc_id IN (%1)"_s.arg(idList), {tag});
                outcome.took = true;
            }
        } else if (type == "group"_L1 || type == "ungroup"_L1) {
            const bool adding = type == "group"_L1;
            qint64 group = action.value(u"id"_s).toLongLong();
            if (group > 0) {
                // The group may have been deleted since the rule was written.
                if (!Database::value(u"SELECT 1 FROM folders WHERE id = ?"_s, {group}).toBool()) {
                    continue;
                }
            } else {
                const QStringList path = pathOf(action);
                group = groupId(path, false);
                if (group == 0 && adding) {
                    group = groupId(path, true);
                    outcome.made = outcome.made || group != 0;
                }
                if (group == 0) {
                    continue;
                }
            }
            if (!adding) {
                Database::exec(u"DELETE FROM document_folders WHERE folder_id = ? AND doc_id IN (%1)"_s.arg(idList), {group});
                outcome.took = true;
                continue;
            }
            for (qint64 doc : documents) {
                Database::exec(u"INSERT OR IGNORE INTO document_folders (doc_id, folder_id) VALUES (?, ?)"_s, {doc, group});
            }
            // Filed is filed.
            Database::exec(u"UPDATE documents SET inbox = 0 WHERE id IN (%1)"_s.arg(idList));
        } else if (type == "inbox"_L1 || type == "toinbox"_L1) {
            Database::exec(u"UPDATE documents SET inbox = ? WHERE id IN (%1)"_s.arg(idList), {type == "toinbox"_L1});
        } else if (type == "flag"_L1 || type == "unflag"_L1) {
            Database::exec(u"UPDATE documents SET flagged = ? WHERE id IN (%1)"_s.arg(idList), {type == "flag"_L1});
            outcome.took = outcome.took || type == "unflag"_L1;
        } else if (type == "comment"_L1) {
            const QString text = action.value(u"text"_s).toString();
            // No comment is an empty one, never none at all.
            Database::exec(u"UPDATE documents SET comment = ? WHERE id IN (%1)"_s.arg(idList), {text.isEmpty() ? u""_s : text});
            outcome.took = outcome.took || text.isEmpty();
            notesChanged = true;
        } else if (type == "note"_L1) {
            const QString text = action.value(u"text"_s).toString().trimmed();
            if (text.isEmpty()) {
                continue;
            }
            const qint64 now = QDateTime::currentSecsSinceEpoch();
            for (qint64 doc : documents) {
                // A rule or a script that comes by again does not say the same thing twice.
                if (!Database::value(u"SELECT 1 FROM annotations WHERE doc_id = ? AND kind = 'note' AND data = ?"_s, {doc, text}).toBool()) {
                    Database::exec(u"INSERT INTO annotations (doc_id, page, kind, data, created, modified) VALUES (?, -1, 'note', ?, ?, ?)"_s, {doc, text, now, now});
                    notesChanged = true;
                }
            }
        }
    }
    if (notesChanged) {
        for (qint64 doc : documents) {
            Store::refreshNotes(doc);
        }
    }
    transaction.commit();
    return outcome;
}

}
