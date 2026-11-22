// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "paths.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QMutex>
#include <QStandardPaths>

using namespace Qt::StringLiterals;

namespace Indeks::Paths
{

static QString ensured(const QString &path)
{
    QDir().mkpath(path);
    return path;
}

namespace
{
Q_GLOBAL_STATIC(QMutex, s_mutex)
Q_GLOBAL_STATIC(QString, s_databaseDir)

/** The chosen database folder; empty while the default one is in use. */
QString chosenDatabaseDir()
{
    QMutexLocker locker(s_mutex());
    return *s_databaseDir();
}

/** Subfolders are only made where the database folder itself is there to hold them. */
QString inDataDir(const QString &name)
{
    const QString dir = dataDir();
    const QString path = dir + u'/' + name;
    if (QFileInfo(dir).isDir()) {
        QDir().mkpath(path);
    }
    return path;
}
}

QString defaultDataDir()
{
    const QString override = qEnvironmentVariable("INDEKS_DATA_DIR");
    if (!override.isEmpty()) {
        return ensured(override);
    }
    return ensured(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/indeks"_L1);
}

void setDatabaseDir(const QString &dir)
{
    const QString cleaned = dir.isEmpty() ? QString() : QDir::cleanPath(dir);
    QMutexLocker locker(s_mutex());
    *s_databaseDir() = cleaned == QDir::cleanPath(defaultDataDir()) ? QString() : cleaned;
}

QString dataDir()
{
    const QString chosen = chosenDatabaseDir();
    return chosen.isEmpty() ? defaultDataDir() : chosen;
}

QString filesDir()
{
    return inDataDir(u"files"_s);
}

QString copiesDir()
{
    return inDataDir(u"copies"_s);
}

QString cacheDir()
{
    const QString override = qEnvironmentVariable("INDEKS_DATA_DIR");
    const QString base = override.isEmpty() ? QString(QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + "/indeks"_L1)
                                            : QString(override + "/cache"_L1);
    // The cache stays on this computer. What is in it goes by document numbers, which every
    // database counts for itself, so each database gets a corner of its own.
    const QString chosen = chosenDatabaseDir();
    if (chosen.isEmpty()) {
        return ensured(base);
    }
    const QByteArray key = QCryptographicHash::hash(chosen.toUtf8(), QCryptographicHash::Sha1).toHex().left(16);
    return ensured(base + "/databases/"_L1 + QString::fromLatin1(key));
}

QString indexFileName()
{
    return u"index.sqlite"_s;
}

QString databaseFile()
{
    return dataDir() + u'/' + indexFileName();
}

QString uniqueFilePath(const QString &dir, const QString &fileName)
{
    QString safe = fileName.trimmed();
    safe.replace(u'/', u'_');
    if (safe.isEmpty() || safe == "."_L1 || safe == ".."_L1) {
        safe = u"document"_s;
    }
    const QFileInfo info(safe);
    const QString base = info.completeBaseName().isEmpty() ? safe : info.completeBaseName();
    const QString suffix = info.completeBaseName().isEmpty() || info.suffix().isEmpty() ? QString() : u'.' + info.suffix();

    QString candidate = dir + u'/' + safe;
    for (int i = 2; QFileInfo::exists(candidate); ++i) {
        candidate = dir + u'/' + base + u' ' + QString::number(i) + suffix;
    }
    return candidate;
}

}
