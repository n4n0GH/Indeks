// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "welcome.h"

#include "database.h"
#include "databasechoice.h"
#include "indekssettings.h"
#include "paths.h"

#include <KLocalizedString>

#include <QDir>

using namespace Indeks;
using namespace Qt::StringLiterals;

namespace
{
Q_GLOBAL_STATIC(QString, s_folderToStartIn)
}

bool Welcome::isNeeded()
{
    const Config *config = Config::self();
    if (!config->activeDatabase().isEmpty() || !config->databasePaths().isEmpty()) {
        return false;
    }
    const Database::FileKind index = Database::inspectFile(Paths::defaultDataDir() + u'/' + Paths::indexFileName()).kind;
    return index == Database::FileKind::Missing || index == Database::FileKind::Blank;
}

QString Welcome::folderToStartIn()
{
    return *s_folderToStartIn();
}

QString Welcome::standardFolder() const
{
    return QDir::cleanPath(Paths::defaultDataDir());
}

QUrl Welcome::standardFolderUrl() const
{
    return QUrl::fromLocalFile(standardFolder());
}

QString Welcome::localPath(const QString &input) const
{
    return DatabaseChoice::localPath(input);
}

QVariantMap Welcome::inspectNewFolder(const QString &folder) const
{
    return DatabaseChoice::inspectNewFolder(folder, true);
}

QVariantMap Welcome::inspectIndexFile(const QString &file) const
{
    return DatabaseChoice::inspectIndexFile(file);
}

QString Welcome::startNew(const QString &folder)
{
    const QVariantMap inspected = inspectNewFolder(folder);
    if (!inspected.value(u"valid"_s).toBool()) {
        return i18nc("@info", "Choose a folder on this computer or on a mounted share.");
    }
    const QString problem = inspected.value(u"problem"_s).toString();
    if (!problem.isEmpty()) {
        return problem;
    }
    // The folder of the local database needs no word: it is where Indeks starts one anyway.
    if (!inspected.value(u"standard"_s).toBool()) {
        const QString path = inspected.value(u"path"_s).toString();
        DatabaseChoice::list(QString(), path);
        *s_folderToStartIn() = path;
    }
    m_decided = true;
    Q_EMIT decided();
    return {};
}

QString Welcome::openExisting(const QString &file)
{
    const QVariantMap inspected = inspectIndexFile(file);
    if (!inspected.value(u"valid"_s).toBool()) {
        return i18nc("@info %1 is a file name", "Choose the file “%1” of a database, on this computer or on a mounted share.", Paths::indexFileName());
    }
    const QString problem = inspected.value(u"problem"_s).toString();
    if (!problem.isEmpty()) {
        return problem;
    }
    DatabaseChoice::list(QString(), inspected.value(u"path"_s).toString());
    m_decided = true;
    Q_EMIT decided();
    return {};
}

#include "moc_welcome.cpp"
