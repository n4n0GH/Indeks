// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "scripts.h"

#include "paths.h"

#include <KLocalizedString>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>

using namespace Qt::StringLiterals;

namespace Indeks::Scripts
{

namespace
{
// Scripts are small; anything larger is not one.
constexpr qint64 MaxScriptBytes = 4 * 1024 * 1024;

QMutex s_allowedMutex;

QString allowedFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/indeks-allowed-scripts"_L1;
}

QByteArray fingerprint(const QString &source)
{
    return QCryptographicHash::hash(source.toUtf8(), QCryptographicHash::Sha256).toHex();
}

// To be called with the mutex held.
QSet<QByteArray> readAllowed()
{
    QSet<QByteArray> allowed;
    QFile file(allowedFile());
    if (file.open(QIODevice::ReadOnly)) {
        const QList<QByteArray> lines = file.readAll().split('\n');
        for (const QByteArray &line : lines) {
            if (!line.trimmed().isEmpty()) {
                allowed.insert(line.trimmed());
            }
        }
    }
    return allowed;
}

bool fail(QString *problem, const QString &message)
{
    if (problem) {
        *problem = message;
    }
    return false;
}
}

QString dir()
{
    return Paths::dataDir() + "/scripts"_L1;
}

bool isFileName(const QString &file)
{
    return file.size() > 3 && file.size() <= 120 && file.endsWith(".py"_L1) && !file.startsWith(u'.') && !file.contains(u'/') && !file.contains(QChar(0))
        && file == file.trimmed();
}

QString fileNameFor(const QString &name)
{
    QString base = name.simplified();
    base.replace(u'/', u'-');
    if (base.endsWith(".py"_L1, Qt::CaseInsensitive)) {
        base.chop(3);
    }
    while (base.startsWith(u'.')) {
        base.remove(0, 1);
    }
    base = base.trimmed();
    const QString file = base + ".py"_L1;
    return !base.isEmpty() && isFileName(file) ? file : QString();
}

QList<Script> list()
{
    QList<Script> scripts;
    const bool approval = needApproval();
    const QFileInfoList files = QDir(dir()).entryInfoList({u"*.py"_s}, QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &info : files) {
        QString source;
        if (!isFileName(info.fileName()) || !read(info.fileName(), &source)) {
            continue;
        }
        scripts.append({info.fileName(), info.completeBaseName(), description(source), info.lastModified(), !approval || isAllowed(source)});
    }
    return scripts;
}

bool read(const QString &file, QString *source)
{
    if (!isFileName(file)) {
        return false;
    }
    QFile script(dir() + u'/' + file);
    if (!script.open(QIODevice::ReadOnly) || script.size() > MaxScriptBytes) {
        return false;
    }
    *source = QString::fromUtf8(script.readAll());
    return true;
}

bool write(const QString &file, const QString &source, QString *problem)
{
    if (!isFileName(file)) {
        return fail(problem, i18nc("@info", "“%1” cannot be the name of a script.", file));
    }
    if (!QDir().mkpath(dir())) {
        return fail(problem, i18nc("@info", "The folder for scripts, %1, cannot be created.", dir()));
    }
    QSaveFile script(dir() + u'/' + file);
    if (!script.open(QIODevice::WriteOnly) || script.write(source.toUtf8()) < 0 || !script.commit()) {
        return fail(problem, i18nc("@info", "The script could not be saved: %1", script.errorString()));
    }
    // What was written here, by the person at this computer, may run here.
    allow(source);
    return true;
}

bool rename(const QString &file, const QString &newFile, QString *problem)
{
    if (!isFileName(file) || !isFileName(newFile)) {
        return fail(problem, i18nc("@info", "“%1” cannot be the name of a script.", newFile));
    }
    const QString target = dir() + u'/' + newFile;
    if (QFileInfo::exists(target)) {
        return fail(problem, i18nc("@info", "A script named “%1” already exists.", QFileInfo(newFile).completeBaseName()));
    }
    if (!QFile::rename(dir() + u'/' + file, target)) {
        return fail(problem, i18nc("@info", "The script could not be renamed."));
    }
    return true;
}

bool remove(const QString &file)
{
    return isFileName(file) && QFile::moveToTrash(dir() + u'/' + file);
}

QString description(const QString &source)
{
    // The docstring: a string that is the first thing in the file after comments.
    static const QRegularExpression docstring(uR"(\A(?:\s*(?:#[^\n]*)?\n)*\s*[rRuU]?(\"\"\"|'''|\"|')(.*?)\1)"_s, QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch match = docstring.match(source);
    if (!match.hasMatch()) {
        return {};
    }
    const QStringList lines = match.captured(2).split(u'\n');
    for (const QString &line : lines) {
        if (!line.trimmed().isEmpty()) {
            return line.simplified();
        }
    }
    return {};
}

bool needApproval()
{
    return QDir::cleanPath(Paths::dataDir()) != QDir::cleanPath(Paths::defaultDataDir());
}

bool isAllowed(const QString &source)
{
    QMutexLocker locker(&s_allowedMutex);
    return readAllowed().contains(fingerprint(source));
}

void allow(const QString &source)
{
    QMutexLocker locker(&s_allowedMutex);
    const QByteArray print = fingerprint(source);
    if (readAllowed().contains(print)) {
        return;
    }
    QDir().mkpath(QFileInfo(allowedFile()).path());
    QFile file(allowedFile());
    if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        file.write(print + '\n');
    }
}

QString starter()
{
    // The comments are for whoever writes the script, so they are in their language.
    const QStringList lines = {
        u"\"\"\"%1\"\"\""_s.arg(i18nc("first line of a new script; shown as its description", "Say here what the script does.")),
        QString(),
        QString(),
        u"def process(document):"_s,
        u"    # %1"_s.arg(i18nc("comment in a new script", "Indeks calls this once for every document the script is run on.")),
        u"    # %1"_s.arg(i18nc("comment in a new script", "Look at the document, then say what is to happen to it. An example:")),
        u"    if \"invoice\" in document.text.lower():"_s,
        u"        document.add_tag(\"invoice\")"_s,
        QString(),
    };
    return lines.join(u'\n');
}

}
