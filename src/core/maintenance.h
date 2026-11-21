// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QtTypes>

/**
 * Keeps the index file from only ever growing. Removing documents leaves two kinds of dead
 * weight behind: the full text index keeps the words of removed documents until its segments
 * are merged, and SQLite keeps the pages that became free instead of shrinking the file.
 * Neither goes away by itself.
 */
namespace Indeks::Maintenance
{

/** Counts a document that left the index, to know when tidying is worth it. */
void noteRemoval();

/** Whether enough dead weight has built up for tidy() to be worth running. */
bool isDue();

/**
 * Merges the full text index and gives free pages back to the file system, on the calling
 * thread's connection. Works in small steps, so that others can use the index meanwhile, and
 * stops between steps when the calling thread's task is aborted (see Task).
 * Returns by how many bytes the index file shrank.
 */
qint64 tidy();

}
