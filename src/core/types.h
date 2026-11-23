// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDateTime>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QUrl>

namespace Indeks
{
Q_NAMESPACE

/** Broad class of a document; decides how it is previewed and annotated. */
enum class Kind {
    Other = 0,
    PlainText = 1,
    RichText = 2,
    Pdf = 3,
    Image = 4,
    Email = 5,
    Canvas = 6, ///< an Obsidian canvas (JSON Canvas): cards and connections on an infinite board
    Website = 7, ///< a file that holds the address of a page on the web and nothing else; see WebLink
};
Q_ENUM_NS(Kind)

/** State of optical character recognition for a document. */
enum class OcrState {
    NotNeeded = 0,
    Pending = 1,
    Done = 2,
    Failed = 3,
};
Q_ENUM_NS(OcrState)

struct Document {
    qint64 id = 0;
    qint64 sourceId = 0; ///< 0 for documents that were added one by one or created in Indeks
    QUrl url; ///< where the original lives; never written to
    QString name;
    QString mimeType;
    Kind kind = Kind::Other;
    qint64 size = 0;
    QDateTime modified;
    QDateTime added;
    QString hash;
    QString localCopy; ///< path of the working copy inside Indeks' data directory, if any
    bool owned = false; ///< created by Indeks and stored in its own directory
    bool inbox = false;
    bool trashed = false;
    bool missing = false;
    bool flagged = false;
    QString comment;
    int pages = 0;
    OcrState ocr = OcrState::NotNeeded;
    /**
     * For a document that is part of another one, like a message in a mailbox file: the
     * document it is in, and where in that document's file it stands. Such a part has no file
     * of its own; its address is made up (see Store::partUrl()).
     */
    qint64 parentId = 0;
    qint64 partOffset = 0;
    qint64 partLength = 0;

    bool isValid() const
    {
        return id > 0;
    }
    bool isPart() const
    {
        return parentId > 0;
    }
};

struct Source {
    qint64 id = 0;
    QUrl url;
    QString name;
    bool recursive = true;
    bool enabled = true;
    QDateTime lastScan;
    /** Files that turn up here later on are put into the inbox. */
    bool inbox = true;
    /** What was here when the location was added is in the index; what comes now is new. */
    bool settled = false;

    bool newFilesToInbox() const
    {
        return inbox && settled;
    }

    bool isLocal() const
    {
        return url.isLocalFile();
    }
};

/** Options that steer what the indexer extracts. Passed by value into the worker thread. */
struct IndexOptions {
    bool indexContent = true;
    bool indexHidden = false;
    bool ocrEnabled = true;
    QString ocrLanguages = QStringLiteral("eng");
    /** How many pages or pictures are read for text at once; 0 for most of the cores. */
    int ocrJobs = 0;
    /** Have LibreOffice read Office documents whose text cannot be got at otherwise. */
    bool officeConversion = true;
    /** The LibreOffice profile to use for that; whoever converts at the same time needs another. */
    QString officeProfile = QStringLiteral("index");
    /** How many files are read at once; 0 for half of the cores, eight at most. */
    int indexJobs = 0;
    int maxContentSizeMb = 100;
    int maxRemoteSizeMb = 50;
};

/** One line of the list of what the indexer is doing and has yet to do. */
struct QueueItem {
    enum State {
        Waiting, ///< found, to be read
        WaitingForScan, ///< a location that is to be looked through
        WaitingForOcr,
        WaitingForDownload,
        // Everything from here on is being worked on right now.
        Scanning,
        Reading,
        Recognizing,
        Downloading,
        Tidying, ///< the index itself: giving back space that removed documents left
        RunningScript, ///< a script of the user's, at one of the documents it is run on
    };

    QString name;
    QString location; ///< folder or address the file is in
    qint64 size = -1; ///< in bytes; negative if unknown or not a file
    State state = Waiting;
    double progress = -1; ///< 0 to 1 for the item being worked on, negative if that cannot be told

    bool isActive() const
    {
        return state >= Scanning;
    }
};

/** Whether a file of this type holds documents of its own: a mailbox with its messages. */
bool isContainerMimeType(const QString &mimeType);
Kind kindForMimeType(const QString &mimeType);
/** Like kindForMimeType(), but also recognises kinds that only the file name gives away. */
Kind kindForFile(const QString &fileName, const QString &mimeType);
QString kindIconName(Kind kind);
/** A queue line for the file at @p url. */
QueueItem queueItemForUrl(const QUrl &url, qint64 size = -1);

}

Q_DECLARE_METATYPE(Indeks::IndexOptions)
Q_DECLARE_METATYPE(Indeks::QueueItem)
