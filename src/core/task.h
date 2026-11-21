// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <functional>

/**
 * What a thread that reads documents needs to know and tell while it is deep inside one file:
 * whether to give up, and how far it is. Both are kept per thread, so that the code that reads
 * a file does not have to pass them through every function on the way.
 */
namespace Indeks::Task
{
/** Makes aborted() on the calling thread follow @p flag. */
void setAbortFlag(const std::atomic_bool *flag);

/** Whether the calling thread's work was called off. Anything that can take long asks this. */
bool aborted();

/** Where reportProgress() on the calling thread ends up. */
void setProgressHandler(std::function<void(double)> handler);

/** Tells how much of the file at hand is done, from 0 to 1. */
void reportProgress(double share);
}
