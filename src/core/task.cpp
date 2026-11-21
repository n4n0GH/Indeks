// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "task.h"

namespace Indeks::Task
{

namespace
{
thread_local const std::atomic_bool *t_abort = nullptr;
thread_local std::function<void(double)> t_progress;
}

void setAbortFlag(const std::atomic_bool *flag)
{
    t_abort = flag;
}

bool aborted()
{
    return t_abort && t_abort->load(std::memory_order_relaxed);
}

void setProgressHandler(std::function<void(double)> handler)
{
    t_progress = std::move(handler);
}

void reportProgress(double share)
{
    if (t_progress) {
        t_progress(share);
    }
}

}
