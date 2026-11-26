// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "extractor.h"
#include "fingerprint.h"
#include "types.h"

namespace Indeks::Indexing
{

struct FileJob {
    /** Local file to read. For remote documents this is a temporary download. */
    QString path;
    /** Where the document really lives; the key under which it is stored. */
    QUrl url;
    qint64 sourceId = 0;
    /** Put a newly added document into the inbox. */
    bool inbox = false;
    /** The file was created by Indeks inside its own directory. */
    bool owned = false;
    /** Size and modification time of the original, if @p path is only a download of it. */
    qint64 originalSize = -1;
    qint64 originalMtime = -1;
};

struct Result {
    qint64 id = 0;
    bool created = false;
    bool wantsOcr = false;
};

/**
 * Indexing a file has two halves. Reading it means running format libraries on a file from
 * anywhere, which is slow and now and then goes wrong; it needs no access to the index and can
 * be done in another process (see ExtractorPool). Storing what was read is quick and belongs
 * to the thread that writes the index.
 */

/** What is to be read: the plan for one file, made from the job and what the index knows. */
struct Reading {
    /** The file to read: the working copy if the user has one, else the file itself. */
    QString contentFile;
    /** The name the document goes by, which decides its type together with the content. */
    QString name;
    /**
     * Hash of the content whose recognised text the index already holds. If the file still
     * has it, the text is kept and the file is not read again.
     */
    QString keepTextIfHash;
    /**
     * With a length: not the file but this stretch of it is read, as one e-mail message. That
     * is how the messages of a mailbox are read, side by side like files.
     */
    qint64 partOffset = 0;
    qint64 partLength = 0;
    /**
     * Nothing is wanted of the file but what it looks like (see Fingerprint): for documents
     * that came into the index before Indeks took note of that.
     */
    bool lookOnly = false;
};

/** What reading a file gave. */
struct Prepared {
    QString mimeType;
    Kind kind = Kind::Other;
    QString hash;
    Extracted extracted;
    /** The content is unchanged and the text in the index stays; `extracted` is empty. */
    bool textKept = false;
    /** The file could not be read, so it is known by name only. */
    bool unreadable = false;
    /** What the document looks like, if it was looked at for that (`looked`). */
    Fingerprint look;
    bool looked = false;
};

/** Plans the reading of @p job. @p existing is the document the index has for it, if any. */
Reading planReading(const FileJob &job, const Document &existing);

/** Reads the file. Does not touch the index; safe in any thread and any process. */
Prepared readFile(const Reading &reading, const IndexOptions &options);

/** What is known of a file that could not be read: its type by name, its hash, no text. */
Prepared unreadableFile(const Reading &reading);

/**
 * Writes what was read into the index, creating or updating the document. Works on the calling
 * thread's database connection.
 */
Result storeFile(const FileJob &job, const Document &existing, const Prepared &prepared, const IndexOptions &options);

/** One message of a mailbox, as storeParts() takes it. */
struct Part {
    qint64 offset = 0;
    qint64 length = 0;
    QString hash;
    Prepared prepared;
};

/**
 * Writes a message of the mailbox @p parent into the index as a document of its own that is
 * part of the mailbox. Returns its id, 0 if that did not work.
 */
qint64 storePart(const Document &parent, const Part &part);

/** Both halves in one go, in this process. */
Result indexFile(const FileJob &job, const IndexOptions &options);

/**
 * Runs OCR for a document whose state is OcrState::Pending and stores the recognised text.
 * Reads @p path if given, else wherever the document's content is found.
 * Returns false if the content is not reachable at the moment, leaving the state pending.
 */
bool runOcr(qint64 documentId, const IndexOptions &options, const QString &path = QString());

/**
 * The same for several pictures, read for text side by side, which is many times faster than
 * one after the other. Returns the documents whose content is not reachable at the moment.
 */
QList<qint64> runOcrBatch(const QList<qint64> &documentIds, const IndexOptions &options);

/**
 * For a scan whose text was recognised by an Indeks that did not yet keep where the words
 * stand: reads it again and stores that alone (Store::setRecognizedLayout()). The text in
 * the index is not touched. Returns whether something was stored.
 */
bool recognizeLayout(qint64 documentId, const IndexOptions &options);

/**
 * Reads a document again from wherever its content is at hand: the original, the working copy
 * or a cached download.
 */
Result reindexDocument(qint64 documentId, const IndexOptions &options);
/** The job reindexDocument() runs; `path` is empty if the content is not at hand. */
FileJob jobForDocument(qint64 documentId);

/**
 * Adds or updates a document that is known by name, size and date only, without reading it.
 * Used for remote files that are too large to download for indexing.
 */
Result indexNameOnly(const QUrl &url, qint64 sourceId, qint64 size, qint64 mtime, bool inbox);

/** File names that are never worth indexing: editor backups, partial downloads, lock files. */
bool isIgnoredFileName(const QString &name);

}
