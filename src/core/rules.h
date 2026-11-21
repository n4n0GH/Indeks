// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QHash>
#include <QJsonArray>
#include <QList>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

/**
 * What rules and scripts are made of: actions, and doing them to documents.
 *
 * An action is a map with a `type` and what that type needs:
 *   tag, untag        `name` of the tag; adding creates the tag if there is none
 *   group, ungroup    the group by its `id`, or by its `path`, the names from the outermost
 *                     group in; adding by path creates the groups that are missing
 *   flag, unflag      pin the document, take the pin off (the names are from when pinning
 *                     was called flagging)
 *   inbox, toinbox    out of the inbox, into it
 *   comment           `text` that becomes the comment
 *   note              `text` of a note to attach, unless the document has that very note
 *   script            `name` of a script file to run on the document (rules only)
 *
 * Everything here works on the calling thread's connection to the index, so that rules can be
 * applied from the thread that runs scripts as well as from the user interface.
 */
namespace Indeks::Rules
{
/** A rule as a map: id, name, query, enabled, onImport, filters, actions. Empty if there is none. */
QVariantMap rule(qint64 id);
QVariantList all();

/** Keeps of @p given what a rule may hold, each action with what it needs and nothing else. */
QJsonArray storable(const QVariantList &given);

/** Whether one of @p actions runs a script. */
bool runScripts(const QVariantList &actions);

/** The documents outside the trash that @p query finds, among @p restrictTo if that is not empty. */
QList<qint64> matching(const QString &query, const QList<qint64> &restrictTo);

struct Outcome {
    bool took = false; ///< something was taken off a document, which may leave it abandoned
    bool made = false; ///< a tag or a group was created
};

/**
 * Does @p actions to @p documents, in the order given: a later action may undo an earlier
 * one, and that is then what was asked for. All of it or nothing. Actions that run scripts
 * are left out; see ScriptHost for those.
 */
Outcome apply(const QList<qint64> &documents, const QVariantList &actions);

/** The id of the tag called @p name, which is created with @p create if there is none; else 0. */
qint64 tagId(const QString &name, bool create);
/** The id of the group at @p path, made level by level with @p create if it is missing; else 0. */
qint64 groupId(const QStringList &path, bool create);
/** Every group with the names that lead to it. */
QHash<qint64, QStringList> groupPaths();
}
