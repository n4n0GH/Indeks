// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "databasefolder.h"

#include "database.h"
#include "indeks_core_debug.h"
#include "paths.h"
#include "store.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStorageInfo>
#include <QSysInfo>
#include <QUrl>

using namespace Qt::StringLiterals;

namespace Indeks::DatabaseFolder
{

namespace
{
// A note that has not been renewed for this long was left behind by a crash or a lost
// connection. Indeks renews its own every quarter of a minute.
constexpr qint64 StaleAfterSeconds = 180;

QString indexFile(const QString &dir)
{
    return dir + "/index.sqlite"_L1;
}

QString noteFile(const QString &dir)
{
    return dir + "/indeks-in-use"_L1;
}

QString thisComputer()
{
    return QSysInfo::machineHostName();
}

void writeNote(const QString &dir)
{
    QFile file(noteFile(dir));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(thisComputer().toUtf8() + '\n' + QByteArray::number(QCoreApplication::applicationPid()) + '\n');
    }
}

/** Who holds the database according to the note, unless that is this very process. */
QString holderOf(const QString &dir)
{
    QFile file(noteFile(dir));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QList<QByteArray> lines = file.readAll().split('\n');
    const QString host = QString::fromUtf8(lines.value(0)).trimmed();
    const qint64 pid = lines.value(1).trimmed().toLongLong();
    if (host.isEmpty()) {
        return {};
    }
    if (host == thisComputer()) {
        // Another Indeks on this computer, if that process is still around.
        const bool alive = pid > 0 && pid != QCoreApplication::applicationPid() && QFileInfo::exists("/proc/"_L1 + QString::number(pid));
        return alive ? host : QString();
    }
    const qint64 age = QFileInfo(file).lastModified().secsTo(QDateTime::currentDateTime());
    return age <= StaleAfterSeconds ? host : QString();
}

int schemaVersionOf(const QString &dir)
{
    static int counter = 0;
    const QString name = u"indeks-probe-%1"_s.arg(++counter);
    int version = 0;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(u"QSQLITE"_s, name);
        db.setDatabaseName(indexFile(dir));
        db.setConnectOptions(u"QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=3000"_s);
        if (db.open()) {
            QSqlQuery query(db);
            if (query.exec(u"PRAGMA user_version"_s) && query.next()) {
                version = query.value(0).toInt();
            }
            query.finish();
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(name);
    return version;
}
}

bool isNetworkStorage(const QString &path)
{
    static const QSet<QByteArray> networkTypes = {
        "nfs", "nfs4", "cifs", "smb", "smb2", "smb3", "smbfs", "ncpfs", "afs", "9p", "ceph", "glusterfs", "lustre", "davfs", "fuse.davfs",
        "fuse.sshfs", "fuse.gvfsd-fuse", "fuse.rclone", "fuse.kio-fuse", "fuse.cifs", "fuse.glusterfs", "ocfs2", "gfs2",
    };
    const QStorageInfo storage(path);
    return storage.isValid() && networkTypes.contains(storage.fileSystemType().toLower());
}

State inspect(const QString &dir)
{
    State state;
    const QFileInfo info(dir);
    state.reachable = info.isDir();
    if (!state.reachable) {
        return state;
    }
    state.writable = info.isWritable();
    state.network = isNetworkStorage(dir);
    state.hasIndex = QFileInfo(indexFile(dir)).isFile();
    if (state.hasIndex) {
        state.tooNew = schemaVersionOf(dir) > Database::SchemaVersion;
        state.inUseBy = holderOf(dir);
    }
    return state;
}

bool acquire(const QString &dir, QString *holder)
{
    const QString other = holderOf(dir);
    if (!other.isEmpty()) {
        if (holder) {
            *holder = other;
        }
        return false;
    }
    writeNote(dir);
    return true;
}

void refresh(const QString &dir)
{
    if (QFileInfo(dir).isDir()) {
        writeNote(dir);
    }
}

void release(const QString &dir)
{
    // Only a note of our own: another computer may have taken over after a lost connection.
    if (holderOf(dir).isEmpty() && QFileInfo::exists(noteFile(dir))) {
        QFile::remove(noteFile(dir));
    }
}

void adoptLocation()
{
    const QString dir = QDir::cleanPath(Paths::dataDir());
    const QString recorded = Database::value(u"SELECT value FROM meta WHERE key = 'folder'"_s).toString();
    if (recorded == dir) {
        return;
    }
    if (!recorded.isEmpty()) {
        qCInfo(INDEKS_CORE) << "The database folder was" << recorded << "and is now" << dir;
        Transaction transaction;
        // Documents created in Indeks are known by their address, working copies by their path.
        const QString oldUrl = Store::urlKey(QUrl::fromLocalFile(recorded + u'/'));
        const QString newUrl = Store::urlKey(QUrl::fromLocalFile(dir + u'/'));
        Database::exec(u"UPDATE documents SET url = ? || substr(url, ?) WHERE substr(url, 1, ?) = ?"_s, {newUrl, oldUrl.size() + 1, oldUrl.size(), oldUrl});
        const QString oldPath = recorded + u'/';
        const QString newPath = dir + u'/';
        Database::exec(u"UPDATE documents SET local_copy = ? || substr(local_copy, ?) WHERE substr(local_copy, 1, ?) = ?"_s,
                       {newPath, oldPath.size() + 1, oldPath.size(), oldPath});
        Database::exec(u"INSERT OR REPLACE INTO meta (key, value) VALUES ('folder', ?)"_s, {dir});
        transaction.commit();
        return;
    }
    Database::exec(u"INSERT OR REPLACE INTO meta (key, value) VALUES ('folder', ?)"_s, {dir});
}

}
