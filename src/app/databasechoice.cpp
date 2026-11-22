// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "databasechoice.h"

#include "databasefolder.h"
#include "indekssettings.h"
#include "library.h"
#include "paths.h"

#include <KLocalizedString>

#include <QDir>
#include <QUrl>

using namespace Indeks;
using namespace Qt::StringLiterals;

namespace DatabaseChoice
{

namespace
{
QString standardFolder()
{
    return QDir::cleanPath(Paths::defaultDataDir());
}

bool isListed(const QString &path)
{
    return Config::self()->databasePaths().contains(path);
}
}

QString localPath(const QString &input)
{
    const QString trimmed = input.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }
    const QUrl url = QUrl::fromUserInput(trimmed, QDir::homePath(), QUrl::AssumeLocalFile);
    return url.isLocalFile() ? QDir::cleanPath(url.toLocalFile()) : QString();
}

QVariantMap inspectIndexFile(const QString &file)
{
    const QString chosen = localPath(file);
    if (chosen.isEmpty()) {
        return {{u"path"_s, QString()}, {u"valid"_s, false}};
    }
    QString problem;
    const QString path = Library::databaseOfIndexFile(chosen, &problem);
    if (path.isEmpty()) {
        return {{u"path"_s, QString()}, {u"valid"_s, true}, {u"problem"_s, problem}};
    }
    const bool known = path == standardFolder() || isListed(path);
    return {
        {u"path"_s, path},
        {u"valid"_s, true},
        {u"network"_s, DatabaseFolder::isNetworkStorage(path)},
        {u"known"_s, known},
        {u"problem"_s, known ? i18nc("@info", "This database is in the list already.") : Library::databaseProblem(path, false)},
    };
}

QVariantMap inspectNewFolder(const QString &folder, bool standardIsFree)
{
    const QString path = localPath(folder);
    if (path.isEmpty()) {
        return {{u"path"_s, QString()}, {u"valid"_s, false}};
    }
    const DatabaseFolder::State state = DatabaseFolder::inspect(path);
    const bool standard = path == standardFolder();
    const bool known = (standard && !standardIsFree) || isListed(path);
    QString problem;
    if (known) {
        problem = i18nc("@info", "This database is in the list already.");
    } else if (state.hasIndex) {
        problem = i18nc("@info", "This folder holds an Indeks database already. To open it, use “Add Database…” and choose its index file.");
    } else {
        problem = Library::databaseProblem(path, true);
    }
    return {
        {u"path"_s, path},
        {u"valid"_s, true},
        {u"empty"_s, state.empty},
        {u"standard"_s, standard},
        {u"network"_s, state.network},
        {u"known"_s, known},
        {u"problem"_s, problem},
    };
}

void list(const QString &name, const QString &path)
{
    Config *config = Config::self();
    QStringList paths = config->databasePaths();
    QStringList names = config->databaseNames();
    while (names.size() < paths.size()) {
        names.append(QDir(paths.at(names.size())).dirName());
    }
    paths.append(path);
    names.append(name.trimmed().isEmpty() ? QDir(path).dirName() : name.trimmed());
    config->setDatabasePaths(paths);
    config->setDatabaseNames(names);
    config->setActiveDatabase(path);
    config->save();
}

}
