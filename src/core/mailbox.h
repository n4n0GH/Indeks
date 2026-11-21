// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

#include <functional>

/**
 * Mailbox files (mbox): many messages in one file, each behind a line that begins with
 * "From ". Such files run to gigabytes, so nothing here ever holds one in memory. A mailbox is
 * gone through once, a buffer at a time, to note where each message stands; after that a
 * message is read by going straight to its place.
 */
namespace Indeks::Mailbox
{

/** Where one message stands in the file: behind its "From " line, up to the next one. */
struct Entry {
    qint64 offset = 0;
    qint64 length = 0;
    /** Of the message as it stands in the file; tells it from every other, wherever it moves. */
    QByteArray hash;
};

/**
 * Finds the messages of the mailbox at @p path. @p progress hears how far through the file
 * this is, from 0 to 1. Stops early, with what it has, if the calling thread's task is called
 * off (see Task). A file that does not begin like a mailbox is one message.
 */
QList<Entry> scan(const QString &path, const std::function<void(double)> &progress = {});

/**
 * The message at this place as it was before it went into the mailbox: lines that were marked
 * with ">" because they begin with "From " are as they were. Empty if it cannot be read.
 */
QByteArray readMessage(const QString &path, qint64 offset, qint64 length);

/** Writes that message to a file of its own, making the folder for it if need be. */
bool writeMessage(const QString &path, qint64 offset, qint64 length, const QString &target);

/** Removes messages that were taken out for looking at and not looked at for @p days. */
void pruneCache(int days);

}
