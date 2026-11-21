// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>

namespace Indeks::Paths
{
/**
 * Folder of the database in use. It holds the index, the documents created in Indeks and the
 * working copies, so that a database is complete wherever it is put, a NAS for instance.
 * This is defaultDataDir() unless setDatabaseDir() chose another one.
 */
QString dataDir();
/** The database every installation has: ~/.local/share/indeks. */
QString defaultDataDir();
/**
 * Makes @p dir the database folder; an empty string goes back to the default. Only to be called
 * while nothing reads or writes the index (see Library::switchDatabase()).
 *
 * A folder chosen this way is never created on demand: if it is on a share that is not mounted,
 * making it would quietly start an empty database on the local disk in its place.
 */
void setDatabaseDir(const QString &dir);
/** Documents created inside Indeks. */
QString filesDir();
/** Working copies of external documents that were annotated or edited. */
QString copiesDir();
/** Downloads of remote documents; safe to delete at any time. */
QString cacheDir();
QString databaseFile();

/** Returns a path inside @p dir for @p fileName that does not exist yet. */
QString uniqueFilePath(const QString &dir, const QString &fileName);
}
