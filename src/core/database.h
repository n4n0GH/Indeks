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

    static constexpr int SchemaVersion = 11;

private:
    static bool migrate(QSqlDatabase &db);
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
