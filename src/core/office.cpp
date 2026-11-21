// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "office.h"

#include "indeks_core_debug.h"
#include "paths.h"
#include "task.h"

#include <KLocalizedString>

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>

using namespace Qt::StringLiterals;

namespace Indeks::Office
{

namespace
{
QString renditionDir()
{
    return Paths::cacheDir() + "/renditions"_L1;
}

QString sofficePath()
{
    static const QString path = [] {
        const QString soffice = QStandardPaths::findExecutable(u"soffice"_s);
        return soffice.isEmpty() ? QStandardPaths::findExecutable(u"libreoffice"_s) : soffice;
    }();
    return path;
}

/** The arguments that put a program into a sandbox without network; empty if there is none. */
QStringList sandboxArguments(QString *program)
{
    static QMutex mutex;
    static bool tried = false;
    static QString bwrap;

    // Everything stays reachable but the network. A process namespace of its own makes sure
    // that all of LibreOffice goes when Indeks stops waiting for it.
    static const QStringList arguments = {u"--dev-bind"_s, u"/"_s, u"/"_s, u"--unshare-net"_s, u"--unshare-pid"_s, u"--die-with-parent"_s, u"--"_s};

    QMutexLocker locker(&mutex);
    if (!tried) {
        tried = true;
        const QString found = QStandardPaths::findExecutable(u"bwrap"_s);
        if (!found.isEmpty()) {
            // Installed is not the same as allowed to make namespaces: try it out.
            QProcess probe;
            probe.start(found, arguments + QStringList{u"true"_s});
            if (probe.waitForFinished(5000) && probe.exitStatus() == QProcess::NormalExit && probe.exitCode() == 0) {
                bwrap = found;
            } else {
                probe.kill();
                probe.waitForFinished(1000);
                qCInfo(INDEKS_CORE) << "bubblewrap cannot be used; LibreOffice runs without a sandbox";
            }
        }
    }
    if (bwrap.isEmpty()) {
        return {};
    }
    *program = bwrap;
    return arguments;
}
}

bool isOfficeDocument(const QString &mimeType)
{
    static const QSet<QString> types = {
        u"application/msword"_s,
        u"application/vnd.openxmlformats-officedocument.wordprocessingml.document"_s,
        u"application/vnd.openxmlformats-officedocument.wordprocessingml.template"_s,
        u"application/vnd.ms-word.document.macroEnabled.12"_s,
        u"application/vnd.ms-word.document.macroenabled.12"_s,
        u"application/vnd.ms-excel"_s,
        u"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"_s,
        u"application/vnd.openxmlformats-officedocument.spreadsheetml.template"_s,
        u"application/vnd.ms-excel.sheet.macroEnabled.12"_s,
        u"application/vnd.ms-excel.sheet.macroenabled.12"_s,
        u"application/vnd.ms-powerpoint"_s,
        u"application/vnd.openxmlformats-officedocument.presentationml.presentation"_s,
        u"application/vnd.openxmlformats-officedocument.presentationml.slideshow"_s,
        u"application/vnd.openxmlformats-officedocument.presentationml.template"_s,
        u"application/vnd.ms-powerpoint.presentation.macroEnabled.12"_s,
        u"application/vnd.ms-powerpoint.presentation.macroenabled.12"_s,
        u"application/vnd.oasis.opendocument.text"_s,
        u"application/vnd.oasis.opendocument.text-template"_s,
        u"application/vnd.oasis.opendocument.spreadsheet"_s,
        u"application/vnd.oasis.opendocument.spreadsheet-template"_s,
        u"application/vnd.oasis.opendocument.presentation"_s,
        u"application/vnd.oasis.opendocument.presentation-template"_s,
        u"application/vnd.oasis.opendocument.graphics"_s,
        u"application/rtf"_s,
        u"text/rtf"_s,
    };
    return types.contains(mimeType);
}

bool converterAvailable()
{
    return !sofficePath().isEmpty();
}

QString renditionPath(const QString &contentHash)
{
    return renditionDir() + u'/' + contentHash + ".pdf"_L1;
}

QString renderToPdf(const QString &path, const QString &contentHash, const QString &profile, int timeoutMs, QString *error)
{
    auto fail = [error](const QString &message) {
        if (error) {
            *error = message;
        }
        return QString();
    };
    if (contentHash.isEmpty() || !QFileInfo(path).isFile()) {
        return fail(i18nc("@info", "The file cannot be read."));
    }

    const QString target = renditionPath(contentHash);
    if (QFileInfo(target).isFile()) {
        // Used again: it stays in the cache for another while.
        QFile file(target);
        if (file.open(QIODevice::ReadWrite)) {
            file.setFileTime(QDateTime::currentDateTime(), QFileDevice::FileModificationTime);
        }
        return target;
    }
    if (!converterAvailable()) {
        return fail(i18nc("@info", "LibreOffice is needed to show this kind of document and is not installed."));
    }

    QDir().mkpath(renditionDir());
    QTemporaryDir work(renditionDir() + "/work-XXXXXX"_L1);
    if (!work.isValid()) {
        return fail(i18nc("@info", "Cannot write to the cache folder."));
    }
    // LibreOffice names its output after its input. A link with a name of our choosing gives
    // a known output name whatever the document is called; the document itself is only read.
    const QString suffix = QFileInfo(path).suffix();
    const QString input = work.filePath(suffix.isEmpty() ? u"document"_s : QString("document."_L1 + suffix));
    if (!QFile::link(path, input)) {
        return fail(i18nc("@info", "Cannot write to the cache folder."));
    }
    const QString output = work.filePath(u"document.pdf"_s);

    const QString profileDir = Paths::cacheDir() + "/libreoffice-"_L1 + profile;
    QString program = sofficePath();
    QStringList arguments = {
        "-env:UserInstallation="_L1 + QUrl::fromLocalFile(profileDir).toString(QUrl::FullyEncoded),
        u"--headless"_s,
        u"--norestore"_s,
        u"--nolockcheck"_s,
        u"--nodefault"_s,
        u"--nofirststartwizard"_s,
        u"--convert-to"_s,
        u"pdf"_s,
        u"--outdir"_s,
        work.path(),
        input,
    };
    QString sandbox;
    const QStringList sandboxing = sandboxArguments(&sandbox);
    if (!sandbox.isEmpty()) {
        arguments = sandboxing + QStringList{program} + arguments;
        program = sandbox;
    }

    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(program, arguments);
    if (!process.waitForStarted(10000)) {
        return fail(i18nc("@info", "LibreOffice could not be started: %1", process.errorString()));
    }
    process.closeWriteChannel();
    QElapsedTimer waited;
    waited.start();
    while (!process.waitForFinished(200)) {
        const bool aborted = Task::aborted();
        if (aborted || waited.elapsed() > timeoutMs || process.state() == QProcess::NotRunning) {
            process.kill();
            process.waitForFinished(2000);
            return fail(aborted ? QString() : i18nc("@info", "LibreOffice took too long to prepare this document."));
        }
    }
    if (!QFileInfo(output).isFile()) {
        qCInfo(INDEKS_CORE) << "LibreOffice made no PDF of" << path << process.readAll().left(400);
        return fail(i18nc("@info", "LibreOffice could not read this document."));
    }
    // Another thread may have made the same rendition in the meantime; either is as good.
    if (!QFile::rename(output, target) && !QFileInfo(target).isFile()) {
        return fail(i18nc("@info", "Cannot write to the cache folder."));
    }
    return target;
}

void pruneRenditions(int maxAgeDays)
{
    // Only what this file put there: PDF files directly inside the rendition folder.
    const QDateTime limit = QDateTime::currentDateTime().addDays(-maxAgeDays);
    const QFileInfoList renditions = QDir(renditionDir()).entryInfoList({u"*.pdf"_s}, QDir::Files | QDir::NoSymLinks);
    for (const QFileInfo &rendition : renditions) {
        if (rendition.lastModified() < limit) {
            QFile::remove(rendition.absoluteFilePath());
        }
    }
}

}
