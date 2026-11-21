// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>

/**
 * A database is a folder: the index file, the documents created in Indeks and the working
 * copies. It can be anywhere the file system reaches, a mounted NAS share for instance. These
 * are the precautions that takes.
 */
namespace Indeks::DatabaseFolder
{

struct State {
    bool reachable = false; ///< the folder is there right now
    bool hasIndex = false; ///< it holds a database already
    bool writable = false;
    bool network = false; ///< on a share rather than a disk of this computer
    bool tooNew = false; ///< written by a newer Indeks than this one
    /** Name of the computer that has the database open, if another one has. */
    QString inUseBy;
};

/** Looks at @p dir without changing anything in it. */
State inspect(const QString &dir);

/** Whether @p path is on storage that is shared over the network. */
bool isNetworkStorage(const QString &path);

/**
 * Notes in the folder that this computer has the database open. SQLite's own locking cannot be
 * relied on across network shares, so two computers writing to one index would ruin it; the
 * note keeps the second one out. Returns false, and who holds it in @p holder, if the database
 * is in use elsewhere.
 */
bool acquire(const QString &dir, QString *holder = nullptr);
/** Renews the note; one that is not renewed for a few minutes is taken for left behind. */
void refresh(const QString &dir);
void release(const QString &dir);

/**
 * To be called once the database in Paths::dataDir() is open. If its folder is not where it
 * was the last time, because it was moved or is mounted under another path on this computer,
 * the places of Indeks' own documents and working copies recorded in the index are brought
 * up to date.
 */
void adoptLocation();

}
