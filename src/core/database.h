// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>
#include <QVariantList>

namespace Indeks
{

/**
 * Access to the SQLite index.
 *
 * SQLite connections must not be shared between threads, so every thread gets
 * its own connection on first use. The database runs in WAL mode, which lets the
 * indexer thread write while the user interface reads.
 */
class Database
{
public:
    /** Connection of the calling thread; opens and migrates the database on first use. */
    static QSqlDatabase connection();

    /** Closes the calling thread's connection. Call before a worker thread ends. */
    static void closeConnection();

    /** Prepares, binds positionally and executes @p sql. Logs failures. */
    static QSqlQuery exec(const QString &sql, const QVariantList &binds = {});

    /** Convenience for single-value queries. */
    static QVariant value(const QString &sql, const QVariantList &binds = {});

    /** Whether the calling thread's connection is open, i.e. the index can be used. */
    static bool isOpen();

    static constexpr int SchemaVersion = 12;
    /**
     * What SQLite calls the application id of a file: a number in its header that says
     * whose it is. An index of Indeks carries this one ("Indk"). It is how a file that is
     * merely called like an index is told from one, before anything is written to it.
     */
    static constexpr int ApplicationId = 0x496E646B;

    /** What a file that is, or would be, the index turns out to be. */
    enum class FileKind {
        Missing, ///< no such file
        Blank, ///< a file with nothing in it: where a new index can start
        Own, ///< an index of Indeks
        Foreign, ///< something else: the database of another program, or no database at all
        Unreadable, ///< an SQLite database that cannot be looked into right now
    };
    struct FileState {
        FileKind kind = FileKind::Missing;
        /** The schema version of an index of Indeks. */
        int version = 0;
    };
    /** Looks at @p file without changing it. */
    static FileState inspectFile(const QString &file);

private:
    static bool migrate(QSqlDatabase &db);
    static FileState classify(QSqlDatabase &db);
};

/** RAII transaction on the calling thread's connection; rolls back unless committed. */
class Transaction
{
public:
    Transaction();
    ~Transaction();
    bool commit();

private:
    Q_DISABLE_COPY_MOVE(Transaction)
    bool m_active = false;
};

}
