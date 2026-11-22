// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "types.h"

#include <QSqlQuery>

namespace Indeks::Store
{
/** The string under which a URL is stored in the index. */
QString urlKey(const QUrl &url);

/** Column list matching readDocument(), for `SELECT ... FROM documents d`. */
QString documentColumns();
Document readDocument(const QSqlQuery &query);
Document document(qint64 id);
Document documentByUrl(const QUrl &url);
Source source(qint64 id);
QList<Source> sources();

/**
 * Local file to read a document's content from: the working copy if there is one,
 * else the original if it is a reachable local file, else a cached download.
 * Empty if the content is not available right now.
 */
QString contentPath(const Document &document);
/**
 * The same, but a part of another document (a message of a mailbox) that has not been taken
 * out of it yet is taken out now, into the cache. For when the content is about to be looked
 * at; contentPath() is for asking whether it is at hand.
 */
QString ensureContent(const Document &document);
/** The made-up address of the part of @p parentId that the index has as document @p id. */
QUrl partUrl(qint64 parentId, qint64 id);
/** The parts of a document, e.g. the messages of a mailbox. */
QList<qint64> parts(qint64 parentId);
int partCount(qint64 parentId);
/** Where a download of a remote document is cached. The file may not exist. */
QString cachePath(const Document &document);
/** Directory that holds the working copy of document @p id. */
QString copyDir(qint64 id);

/** Replaces name and content in the full text index, keeping the notes. */
void writeFullText(qint64 id, const QString &name, const QString &content);
void appendFullText(qint64 id, const QString &text);
void renameFullText(qint64 id, const QString &name);
/** Rebuilds the searchable notes of a document from its comment and note annotations. */
void refreshNotes(qint64 id);
QString fullText(qint64 id);

/**
 * Whether the document carries work of the user's that must not be thrown away: a comment,
 * notes, drawings, tags, groups, a flag, links to other documents, or a text edited in Indeks. A document whose file is
 * gone, or whose location is no longer indexed, is kept exactly as long as this holds.
 */
bool hasUserData(qint64 id);

/**
 * Removes a document from the index together with its working copy and cached download, and
 * with it the parts it has. The original file is never touched.
 */
void removeDocument(qint64 id);

/**
 * What becomes of a document whose file is no longer where it was: it goes into the trash,
 * with everything that is attached to it, where the user sees that it went and can tell a
 * file that was deleted by accident. Returns whether it was put there now. A document that
 * was in the trash already stays there as the user's; one that was emptied from the trash
 * is forgotten.
 */
bool fileVanished(qint64 id);
/** The file is there again: a document that was in the trash only for its file leaves it. */
void fileReturned(qint64 id);

/** "1,2,3" for use in `IN (...)`. */
QString idList(const QList<qint64> &ids);
}
