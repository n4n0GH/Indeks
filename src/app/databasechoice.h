// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>
#include <QVariantMap>

/**
 * What the user may choose as a database, and what is said about a choice before it is
 * taken. Shared by the dialogs of the running application (through App) and by the window
 * a new installation starts with (Welcome), which is there before there is an App.
 */
namespace DatabaseChoice
{
/** A local path for what a file or folder dialog or the user's typing gave. */
QString localPath(const QString &input);
/**
 * What adding the database whose index is @p file would do: `path` (the folder of the
 * database), `valid` (there is something to say about it), `network`, `known` (already in
 * the list) and `problem`.
 */
QVariantMap inspectIndexFile(const QString &file);
/**
 * What starting a new database in @p folder would do: `path`, `valid`, `empty` (nothing is
 * in the folder), `standard` (it is the folder of the local database), `network`, `known`
 * and `problem`. A folder that holds a database already is a problem: that one is added.
 *
 * The folder of the local database is taken already once Indeks runs. Where a new
 * installation is asked, @p standardIsFree says that it is still to be had.
 */
QVariantMap inspectNewFolder(const QString &folder, bool standardIsFree = false);
/** Puts the database in @p path on the list, as the one to open, and saves that. */
void list(const QString &name, const QString &path);
}
