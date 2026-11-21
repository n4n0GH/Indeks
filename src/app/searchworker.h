// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include <atomic>

/** What the document list knows about one of its rows. */
struct DocumentRow {
    qint64 id = 0;
    QString name;
    QString mime;
    int kind = 0;
    qint64 size = 0;
    qint64 mtime = 0;
    qint64 added = 0;
    qint64 sourceId = 0;
    QStringList tags;
    bool missing = false;
    bool flagged = false;
    bool inbox = false;
    bool annotated = false;
    /** For a document that is part of another, like a message of a mailbox: which, and its name. */
    qint64 parentId = 0;
    QString parentName;
    /** How many documents this one holds. */
    int parts = 0;
    /** How far in the row stands: 1 for a part shown under its document. */
    int depth = 0;
    // For columns the user may add to the list.
    int pages = 0;
    QString comment;
    QStringList groups;
    QString sender; ///< of an e-mail
    QString location; ///< the folder the file is in, or the mailbox a message is in
};

/** Which requests are still wanted; the worker drops the ones that were overtaken. */
struct SearchTickets {
    std::atomic<quint64> search = 0;
    std::atomic<quint64> excerpts = 0;
};

/**
 * Runs searches through the text of all documents away from the user interface.
 *
 * Looking a word up in a large index can take seconds, the shorter the word the longer, and
 * so can reading the text of large documents for the passages shown with the results. Both
 * happen here, in a thread of their own with its own connection to the index, so that the
 * window stays usable and typing on is never held up.
 *
 * A search that has begun cannot be called off; one that still waits and has been overtaken
 * by a newer one is skipped.
 */
class SearchWorker : public QObject
{
    Q_OBJECT
public:
    explicit SearchWorker(const SearchTickets *tickets);

    /** Runs the list's query in the calling thread. At most @p maxRows rows are returned. */
    static QList<DocumentRow> fetch(const QString &sql, const QVariantList &binds, int maxRows, bool *truncated);

public Q_SLOTS:
    void search(quint64 ticket, const QString &sql, const QVariantList &binds, int maxRows);
    /** Reads the text of the documents @p ids and looks for the passages with @p terms. */
    void excerpts(quint64 ticket, const QList<qint64> &ids, const QStringList &terms);
    /** Closes this thread's connection to the index. */
    void close();

Q_SIGNALS:
    void found(quint64 ticket, const QList<DocumentRow> &rows, bool truncated);
    /** Comes in portions; @p last with the final one of a request. */
    void excerptsReady(quint64 ticket, const QHash<qint64, QString> &excerpts, bool last);

private:
    const SearchTickets *const m_tickets;
};

Q_DECLARE_METATYPE(DocumentRow)
