// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "library.h"

#include "canvas.h"
#include "database.h"
#include "databasefolder.h"
#include "email.h"
#include "extractor.h"
#include "extractorpool.h"
#include "indeks_core_debug.h"
#include "indexer.h"
#include "indexing.h"
#include "jsonformat.h"
#include "mailbox.h"
#include "maintenance.h"
#include "ocr.h"
#include "office.h"
#include "paths.h"
#include "remotescanner.h"
#include "rules.h"
#include "scripthost.h"
#include "scripts.h"
#include "searchquery.h"
#include "textformat.h"
#include "store.h"
#include "task.h"
#include "todocalendar.h"
#include "weblink.h"

#include <KDirWatch>
#include <KFileMetaData/PropertyInfo>
#include <KFormat>
#include <KIO/CopyJob>
#include <KIO/FileCopyJob>
#include <KLocalizedString>

#include <QBuffer>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QImage>
#include <QLineF>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QSaveFile>
#include <QThread>
#include <QUuid>

#include <algorithm>
#include <functional>

using namespace Qt::StringLiterals;

namespace Indeks
{

namespace
{
/** Forgets the columns of views that are no more; see further down. */
void pruneViewColumns();

constexpr qint64 MaxPreviewTextBytes = 4 * 1024 * 1024;
// How much of a plain text file is read to tell what it is written as.
constexpr qint64 GuessedFormatBytes = 64 * 1024;
// How often the room the database takes on the disk is added up anew.
constexpr int MeasureIntervalMs = 60 * 1000;
constexpr int RenditionTimeoutMs = 120000;
constexpr int RenditionMaxAgeDays = 90;
// How much of a JSON Lines file is formatted when only its beginning is asked for.
constexpr qsizetype MaxQuickFormatCharacters = 512 * 1024;
// How many runs of scripts are remembered for looking at how they went.
constexpr int MaxScriptReports = 30;
// Past this many, the list is told that documents changed, not which.
constexpr int MaxChangedOneByOne = 200;

QString metadataLabel(const QString &key)
{
    if (key == "emailFrom"_L1) {
        return i18nc("@label e-mail header", "From");
    }
    if (key == "emailTo"_L1) {
        return i18nc("@label e-mail header", "To");
    }
    if (key == "emailCc"_L1) {
        return i18nc("@label e-mail header", "Cc");
    }
    if (key == "emailDate"_L1) {
        return i18nc("@label e-mail header", "Sent");
    }
    if (key == "emailCount"_L1) {
        return i18nc("@label", "Messages");
    }
    if (key == "emailAttachments"_L1) {
        return i18nc("@label", "Attachments");
    }
    if (key == "unreadable"_L1) {
        return i18nc("@label about the content of a document", "Content");
    }
    if (key == "canvasCards"_L1) {
        return i18nc("@label number of cards on a canvas", "Cards");
    }
    if (key == "canvasConnections"_L1) {
        return i18nc("@label number of connections on a canvas", "Connections");
    }
    if (key == "webAddress"_L1) {
        return i18nc("@label the address of a website", "Address");
    }
    const QString label = KFileMetaData::PropertyInfo::fromName(key).displayName();
    return label.isEmpty() ? key : label;
}

QString formatDate(const QDateTime &dateTime)
{
    return dateTime.isValid() ? QLocale().toString(dateTime.toLocalTime(), QLocale::ShortFormat) : QString();
}

/**
 * Removes everything from HTML that would make the viewer load something from the network.
 * Attributes and style sheets are only looked at inside tags: what the text itself says,
 * in a page about HTML for instance, stays as it is.
 */
QString sanitizeHtml(QString html)
{
    static const QRegularExpression blocks(u"<(script|style|head|iframe|object|embed)\\b.*?</\\1\\s*>"_s,
                                           QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression unwanted(u"<(img|link|meta|base|iframe|object|embed|script)\\b[^>]*>"_s, QRegularExpression::CaseInsensitiveOption);
    // A tag, with quoted values that may hold a ">"; where the quotes do not add up, up to
    // the next ">".
    static const QRegularExpression tag(u"<[a-zA-Z](?:\"[^\"]*\"|'[^']*'|[^'\">])*>|<[a-zA-Z][^>]*>"_s);
    static const QRegularExpression attributes(u"\\s(background|src|srcset|on\\w+)\\s*=\\s*(\"[^\"]*\"|'[^']*'|[^\\s>]+)"_s,
                                               QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression cssUrl(u"url\\s*\\("_s, QRegularExpression::CaseInsensitiveOption);
    html.remove(blocks);
    html.remove(unwanted);

    QString cleaned;
    cleaned.reserve(html.size());
    qsizetype done = 0;
    QRegularExpressionMatchIterator tags = tag.globalMatch(html);
    while (tags.hasNext()) {
        const QRegularExpressionMatch match = tags.next();
        cleaned += QStringView(html).mid(done, match.capturedStart() - done);
        QString text = match.captured();
        text.remove(attributes);
        text.replace(cssUrl, u"none("_s);
        cleaned += text;
        done = match.capturedEnd();
    }
    cleaned += QStringView(html).mid(done);
    return cleaned;
}

/**
 * HTML from elsewhere, the clipboard for instance, as a page to keep: what its body holds,
 * without what would load something from the network, in a frame that says how it is
 * encoded.
 */
QString pageOfForeignHtml(const QString &html)
{
    static const QRegularExpression bodyStart(u"<body\\b[^>]*>"_s, QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression bodyEnd(u"</body\\s*>"_s, QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression frame(u"<!DOCTYPE[^>]*>|</?html\\b[^>]*>"_s, QRegularExpression::CaseInsensitiveOption);

    QString body = sanitizeHtml(html);
    const QRegularExpressionMatch start = bodyStart.match(body);
    if (start.hasMatch()) {
        body = body.mid(start.capturedEnd());
    }
    const QRegularExpressionMatch end = bodyEnd.match(body);
    if (end.hasMatch()) {
        body.truncate(end.capturedStart());
    }
    body.remove(frame);
    return u"<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\"></head><body>\n"_s + body.trimmed() + u"\n</body></html>\n"_s;
}

QString cleanName(const QString &name)
{
    QString cleaned = name.trimmed();
    cleaned.replace(u'/', u'_');
    return cleaned;
}
}

QList<qint64> toIdList(const QVariantList &ids)
{
    QList<qint64> result;
    result.reserve(ids.size());
    for (const QVariant &id : ids) {
        const qint64 value = id.toLongLong();
        if (value > 0) {
            result.append(value);
        }
    }
    return result;
}

Library::Library(QObject *parent)
    : QObject(parent)
    , m_indexer(new Indexer(this))
    , m_remote(new RemoteScanner(m_indexer, this))
    , m_scripts(new ScriptHost(this))
    , m_watch(new KDirWatch(this))
    , m_scriptWatch(new KDirWatch(this))
{
    attachDatabase(); // open and migrate before anything else touches the index
    m_renditionPool.setMaxThreadCount(1);
    m_countPool.setMaxThreadCount(1);
    m_calendarPool.setMaxThreadCount(1);

    // The calendar of to-dos follows the list, and the documents the to-dos are about,
    // whose names are in it.
    m_calendarTimer.setSingleShot(true);
    m_calendarTimer.setInterval(400);
    connect(&m_calendarTimer, &QTimer::timeout, this, &Library::writeTodoCalendar);
    connect(this, &Library::todosChanged, this, &Library::scheduleTodoCalendar);
    connect(this, &Library::changed, this, &Library::scheduleTodoCalendar);
    scheduleTodoCalendar();

    m_changeTimer.setSingleShot(true);
    m_changeTimer.setInterval(120);
    connect(&m_changeTimer, &QTimer::timeout, this, &Library::changed);

    m_watchTimer.setSingleShot(true);
    m_watchTimer.setInterval(1000);
    connect(&m_watchTimer, &QTimer::timeout, this, &Library::dispatchPendingDirectories);

    m_availabilityTimer.setInterval(15000);
    connect(&m_availabilityTimer, &QTimer::timeout, this, &Library::checkAvailability);

    // The size of the database is said beside its name; it grows while files are read.
    m_measureTimer.setInterval(MeasureIntervalMs);
    connect(&m_measureTimer, &QTimer::timeout, this, &Library::measureDatabase);

    m_remoteTimer.setInterval(30 * 60 * 1000);
    connect(&m_remoteTimer, &QTimer::timeout, this, [this] {
        const QList<Source> all = Store::sources();
        for (const Source &source : all) {
            if (!source.isLocal()) {
                m_remote->scanSource(source.id);
            }
        }
    });

    // Scripts are files anyone may edit with any editor; the list follows.
    m_scriptTimer.setSingleShot(true);
    m_scriptTimer.setInterval(200);
    connect(&m_scriptTimer, &QTimer::timeout, this, &Library::scriptsChanged);
    for (auto signal : {&KDirWatch::dirty, &KDirWatch::created, &KDirWatch::deleted}) {
        connect(m_scriptWatch, signal, &m_scriptTimer, qOverload<>(&QTimer::start));
    }
    watchScripts();

    connect(m_scripts, &ScriptHost::busyChanged, this, [this] {
        Q_EMIT busyChanged();
        Q_EMIT scriptsRunningChanged();
        Q_EMIT statusChanged();
        Q_EMIT activityChanged();
    });
    connect(m_scripts, &ScriptHost::statusChanged, this, [this] {
        Q_EMIT statusChanged();
        Q_EMIT activityChanged();
    });
    connect(m_scripts, &ScriptHost::documentsChanged, this, [this](const QList<qint64> &ids) {
        if (ids.size() <= MaxChangedOneByOne) {
            for (qint64 id : ids) {
                Q_EMIT documentChanged(id);
            }
        }
        touch();
    });
    connect(m_scripts, &ScriptHost::reported, this, [this](const QVariantMap &report) {
        if (report.value(u"trial"_s).toBool()) {
            Q_EMIT scriptTried(report);
            return;
        }
        m_scriptReports.prepend(report);
        while (m_scriptReports.size() > MaxScriptReports) {
            m_scriptReports.removeLast();
        }
        Q_EMIT scriptFinished(report);
    });
    connect(m_scripts, &ScriptHost::finished, this, [this](quint64 ticket, int documents, bool took) {
        if (took) {
            dropAbandoned();
        }
        touch();
        if (m_ruleRuns.contains(ticket)) {
            Q_EMIT message(i18ncp("@info", "The rule “%2” was applied to %1 document.", "The rule “%2” was applied to %1 documents.", documents, m_ruleRuns.take(ticket)));
        }
    });

    connect(m_indexer, &Indexer::changed, this, &Library::touch);
    connect(m_indexer, &Indexer::busyChanged, this, &Library::busyChanged);
    connect(m_indexer, &Indexer::statusChanged, this, &Library::statusChanged);
    connect(m_indexer, &Indexer::documentsAdded, this, &Library::applyImportRules);
    connect(m_remote, &RemoteScanner::changed, this, &Library::touch);
    connect(m_remote, &RemoteScanner::busyChanged, this, &Library::busyChanged);
    connect(m_remote, &RemoteScanner::statusChanged, this, &Library::statusChanged);
    connect(m_remote, &RemoteScanner::failed, this, &Library::message);
    connect(m_remote, &RemoteScanner::folderFound, this, [this](const QUrl &url) {
        addSource(url);
    });
    const auto vanished = [this](const QStringList &names) {
        Q_EMIT message(names.size() == 1
                           ? i18nc("@info", "“%1” is gone from where it was. It is in the trash now, and leaves it by itself if the file comes back.", names.constFirst())
                           : i18ncp("@info",
                                    "%1 file is gone from where it was. It is in the trash now, and leaves it by itself if the file comes back.",
                                    "%1 files are gone from where they were. They are in the trash now, and leave it by themselves if the files come back.",
                                    names.size()));
    };
    connect(m_indexer, &Indexer::filesVanished, this, vanished);
    connect(m_remote, &RemoteScanner::filesVanished, this, vanished);
    connect(m_indexer, &Indexer::queueChanged, this, &Library::activityChanged);
    connect(m_indexer, &Indexer::unreadable, this, [this](const QStringList &names) {
        Q_EMIT message(names.size() == 1
                           ? i18nc("@info", "“%1” could not be read: the program that reads files gave up on it. It is found by its name only.", names.constFirst())
                           : i18ncp("@info",
                                    "%1 file could not be read: the program that reads files gave up on it. It is found by its name only.",
                                    "%1 files could not be read: the program that reads files gave up on them. They are found by their names only.",
                                    names.size()));
    });
    connect(m_indexer, &Indexer::tidied, this, [this](qint64 bytes) {
        // Worth a word only when it is an amount someone would notice on their disk.
        if (bytes >= 64LL * 1024 * 1024) {
            Q_EMIT message(i18nc("@info", "Indeks tidied up its index and gave %1 of disk space back.", KFormat().formatByteSize(double(bytes))));
        }
    });
    connect(m_remote, &RemoteScanner::queueChanged, this, &Library::activityChanged);

    auto onScanned = [this](qint64 sourceId, bool available) {
        const bool wasAvailable = !m_unavailableSources.contains(sourceId);
        if (available) {
            m_unavailableSources.remove(sourceId);
        } else {
            m_unavailableSources.insert(sourceId);
        }
        if (wasAvailable != available) {
            Q_EMIT sourcesChanged();
        }
        touch();
    };
    connect(m_indexer, &Indexer::sourceScanned, this, onScanned);
    connect(m_remote, &RemoteScanner::sourceScanned, this, onScanned);

    connect(m_watch, &KDirWatch::dirty, this, [this](const QString &path) {
        onPathChanged(path, false);
    });
    connect(m_watch, &KDirWatch::created, this, [this](const QString &path) {
        onPathChanged(path, true);
    });
    connect(m_watch, &KDirWatch::deleted, this, [this](const QString &path) {
        onPathChanged(path, false);
    });

    m_indexer->setOptions(m_options);
    m_remote->setOptions(m_options);
}

Library::~Library()
{
    // A to-do ticked off just before Indeks is closed is not to stay in the calendar.
    flushTodoCalendar();
    m_scripts->shutdown(5000);
    m_renditionAbort = true;
    m_renditionPool.waitForDone();
    m_countPool.waitForDone();
    DatabaseFolder::release(Paths::dataDir());
}

void Library::attachDatabase()
{
    Database::connection();
    if (!Database::isOpen()) {
        return;
    }
    DatabaseFolder::acquire(Paths::dataDir());
    DatabaseFolder::adoptLocation();
    m_databaseLost = false;
}

int Library::ocrDefaultJobs() const
{
    return Ocr::defaultParallelism();
}

int Library::indexDefaultJobs() const
{
    return ExtractorPool::defaultProcesses();
}

int Library::processorCount() const
{
    return qMax(1, QThread::idealThreadCount());
}

QString Library::databaseDir() const
{
    return Paths::dataDir();
}

QString Library::databaseProblem(const QString &dir, bool create)
{
    const DatabaseFolder::State state = DatabaseFolder::inspect(dir);
    if (!state.reachable) {
        return i18nc("@info", "The folder “%1” cannot be reached. If it is on a network share or an external drive, connect that first.", dir);
    }
    if (state.foreignIndex) {
        return i18nc("@info %1 is a file name, %2 a folder",
                     "The file “%1” in “%2” is not an Indeks database. Indeks leaves it as it is.",
                     Paths::indexFileName(),
                     dir);
    }
    if (state.unreadableIndex) {
        return i18nc("@info", "The database in “%1” cannot be read right now.", dir);
    }
    if (!state.hasIndex && !create) {
        return i18nc("@info", "There is no Indeks database in “%1”.", dir);
    }
    if (!state.writable) {
        return i18nc("@info", "The folder “%1” cannot be written to.", dir);
    }
    if (state.tooNew) {
        return i18nc("@info", "The database in “%1” was written by a newer version of Indeks and cannot be opened with this one.", dir);
    }
    if (!state.inUseBy.isEmpty()) {
        return i18nc("@info", "The database in “%1” is open in Indeks on the computer “%2”. A database can only be used from one place at a time.", dir, state.inUseBy);
    }
    return {};
}

QString Library::databaseOfIndexFile(const QString &file, QString *problem)
{
    const QFileInfo info(file);
    QString reason;
    if (info.isDir()) {
        reason = i18nc("@info %1 is a file name", "Choose the file “%1” of the database, not its folder.", Paths::indexFileName());
    } else if (!info.isFile()) {
        reason = i18nc("@info", "There is no file “%1”.", file);
    } else if (info.fileName() != Paths::indexFileName()) {
        reason = i18nc("@info %1 is the file chosen, %2 the name it would have to have",
                       "“%1” is not the index of an Indeks database. That file is called “%2”.",
                       info.fileName(),
                       Paths::indexFileName());
    }
    if (problem) {
        *problem = reason;
    }
    return reason.isEmpty() ? QDir::cleanPath(info.absolutePath()) : QString();
}

QString Library::switchDatabase(const QString &dir, bool create)
{
    const QString target = QDir::cleanPath(dir.isEmpty() ? Paths::defaultDataDir() : dir);
    const QString previous = QDir::cleanPath(Paths::dataDir());
    if (target == previous && Database::isOpen()) {
        return {};
    }
    // The local database is the one that is always to be had: Indeks goes back to it when
    // another cannot be reached. It is started on the way if it was never used, as on an
    // installation that began with a database elsewhere.
    const bool local = target == QDir::cleanPath(Paths::defaultDataDir());
    const QString problem = databaseProblem(target, create || local);
    if (!problem.isEmpty()) {
        return problem;
    }

    // Everything that reads or writes the index comes to rest first.
    m_availabilityTimer.stop();
    m_remoteTimer.stop();
    m_watchTimer.stop();
    m_changeTimer.stop();
    m_remote->stop();
    m_indexer->stop();
    m_renditionAbort = true;
    m_renditionPool.waitForDone();
    m_countPool.waitForDone();
    m_renditionAbort = false;
    m_renditionRunning = false;
    m_renditionWaiting = {};
    const QList<Source> watched = Store::sources();
    for (const Source &source : watched) {
        unwatchSource(source);
    }
    m_watchedSources.clear();
    m_pendingDirectories.clear();
    m_unavailableSources.clear();
    m_fetching.clear();

    m_scripts->rest(10000);
    m_ruleRuns.clear();
    m_scriptReports.clear();
    // What changed in the to-dos of this database goes into its calendar before it is closed.
    flushTodoCalendar();
    m_calendarProblem.clear();
    Q_EMIT databaseAboutToChange();
    ++m_databaseTicket;
    // Nothing is known about the size of the other one yet.
    m_databaseSize = -1;
    Q_EMIT databaseSizeChanged();
    DatabaseFolder::release(previous);
    Database::closeConnection();
    Paths::setDatabaseDir(target);
    attachDatabase();

    QString error;
    if (!Database::isOpen()) {
        error = i18nc("@info", "The database in “%1” could not be opened.", target);
        Database::closeConnection();
        Paths::setDatabaseDir(previous);
        attachDatabase();
    }

    m_indexer->restart();
    m_remote->resume();
    m_indexer->setOptions(m_options);
    if (m_started) {
        start(m_rescanOnStart);
    }
    watchScripts();
    Q_EMIT databaseChanged();
    Q_EMIT sourcesChanged();
    Q_EMIT todosChanged();
    Q_EMIT todoCalendarChanged();
    Q_EMIT viewColumnsChanged();
    Q_EMIT scriptsChanged();
    Q_EMIT changed();
    return error;
}

void Library::setOptions(const IndexOptions &options)
{
    const bool ocrSwitchedOn = options.ocrEnabled && !m_options.ocrEnabled;
    const bool hiddenSwitchedOn = options.indexHidden && !m_options.indexHidden;
    m_options = options;
    m_indexer->setOptions(options);
    m_remote->setOptions(options);
    if (ocrSwitchedOn && m_started) {
        m_indexer->processOcrQueue();
    }
    if (hiddenSwitchedOn && m_started) {
        // Hidden files come into the index now, and they are not new: every location is gone
        // through as if for the first time, so that they do not fill the inbox.
        m_indexer->unsettleSources();
        rescanAll();
    }
}

void Library::start(bool rescan)
{
    m_started = true;
    m_rescanOnStart = rescan;
    Office::pruneRenditions(RenditionMaxAgeDays);
    Mailbox::pruneCache(RenditionMaxAgeDays);
    checkAvailability();
    m_availabilityTimer.start();
    m_remoteTimer.start();
    if (rescan) {
        rescanAll();
    }
    m_indexer->processOcrQueue();
    // Pictures and PDFs that came in before Indeks noted what they look like. It asks after
    // the scans, which look at what they read themselves.
    m_indexer->lookAtDocuments();
    // Documents that are only here for work of the user's which has since gone, and the space
    // that removed documents left behind. After the scans, which may remove more.
    dropAbandonedLater();
    measureDatabase();
    m_measureTimer.start();
}

bool Library::isBusy() const
{
    return m_indexer->isBusy() || m_remote->isBusy() || m_scripts->isBusy();
}

QString Library::status() const
{
    const QString indexing = m_indexer->status();
    if (!indexing.isEmpty()) {
        return indexing;
    }
    const QString scripts = m_scripts->status();
    return scripts.isEmpty() ? m_remote->status() : scripts;
}

QString Library::statusDetail() const
{
    return m_indexer->status().isEmpty() ? m_scripts->statusDetail() : m_indexer->statusDetail();
}

QList<QueueItem> Library::activity() const
{
    // What is being worked on comes first, whoever does it.
    QList<QueueItem> list = m_indexer->queue();
    list.append(m_remote->queue());
    if (m_scripts->isBusy()) {
        list.append(m_scripts->queueItem());
    }
    std::stable_partition(list.begin(), list.end(), [](const QueueItem &item) {
        return item.isActive();
    });
    return list;
}

int Library::activityOverflow() const
{
    return m_indexer->queueOverflow();
}

bool Library::shutdown(int timeoutMs)
{
    m_availabilityTimer.stop();
    m_remoteTimer.stop();
    m_remote->stop();
    m_renditionAbort = true;
    m_scripts->cancel(); // gives up at once; waited for below
    const bool indexerStopped = m_indexer->stop(timeoutMs);
    const bool countStopped = m_countPool.waitForDone(timeoutMs);
    const bool scriptsStopped = m_scripts->shutdown(timeoutMs);
    return m_renditionPool.waitForDone(timeoutMs) && countStopped && indexerStopped && scriptsStopped;
}

double Library::progress() const
{
    return m_indexer->status().isEmpty() ? m_scripts->progress() : m_indexer->progress();
}

bool Library::ocrAvailable() const
{
    return Ocr::isAvailable();
}

QStringList Library::ocrLanguages() const
{
    return Ocr::installedLanguages();
}

void Library::touch()
{
    if (!m_changeTimer.isActive()) {
        m_changeTimer.start();
    }
}

void Library::touchDocument(qint64 id)
{
    Q_EMIT documentChanged(id);
    touch();
}

// ---------------------------------------------------------------------------------------------
// Indexed locations

QVariantList Library::sources(bool counted) const
{
    const QHash<qint64, int> counts = counted ? Store::sourceCounts() : QHash<qint64, int>();

    QVariantList result;
    const QList<Source> all = Store::sources();
    for (const Source &source : all) {
        result.append(QVariantMap{
            {u"id"_s, source.id},
            {u"name"_s, source.name},
            {u"url"_s, source.url},
            {u"display"_s, source.isLocal() ? source.url.toLocalFile() : source.url.toDisplayString()},
            {u"local"_s, source.isLocal()},
            {u"recursive"_s, source.recursive},
            {u"available"_s, isSourceAvailable(source.id)},
            {u"count"_s, counts.value(source.id)},
            {u"lastScan"_s, formatDate(source.lastScan)},
            {u"inbox"_s, source.inbox},
        });
    }
    return result;
}

bool Library::canFindSimilar(qint64 id)
{
    const Document doc = Store::document(id);
    if (!doc.isValid() || doc.isPart() || !Fingerprint::applies(doc.kind)) {
        return false;
    }
    Fingerprint print = Store::fingerprint(id);
    const bool current = Database::value(u"SELECT hash FROM fingerprints WHERE doc_id = ?"_s, {id}).toString() == doc.hash;
    if (!print.isValid() || !current) {
        // Not looked at yet: a document from before, whose turn in the background has not
        // come. It is being shown at this moment, so reading it here is nothing new.
        const QString path = Store::contentPath(doc);
        if (path.isEmpty()) {
            return print.isValid();
        }
        print = Fingerprint::ofFile(path, doc.kind);
        Store::setFingerprint(id, doc.hash, print);
    }
    return print.isValid();
}

bool Library::isSourceAvailable(qint64 id) const
{
    return !m_unavailableSources.contains(id);
}

qint64 Library::addSource(const QUrl &rawUrl, const QString &name, bool recursive)
{
    QUrl url = rawUrl.adjusted(QUrl::NormalizePathSegments | QUrl::StripTrailingSlash);
    if (!url.isValid() || url.scheme().isEmpty()) {
        Q_EMIT message(i18nc("@info", "“%1” is not a valid location.", rawUrl.toDisplayString()));
        return 0;
    }
    if (url.isLocalFile()) {
        const QFileInfo info(url.toLocalFile());
        if (!info.isDir()) {
            Q_EMIT message(i18nc("@info", "“%1” is not a folder.", url.toLocalFile()));
            return 0;
        }
        url = QUrl::fromLocalFile(QDir::cleanPath(info.absoluteFilePath()));
        const QString own = QDir::cleanPath(Paths::dataDir());
        const QString path = url.toLocalFile();
        if (path == own || path.startsWith(own + u'/')) {
            Q_EMIT message(i18nc("@info", "Indeks' own data folder cannot be an indexed location."));
            return 0;
        }
    }
    const qint64 existing = Database::value(u"SELECT id FROM sources WHERE url = ?"_s, {Store::urlKey(url)}).toLongLong();
    if (existing != 0) {
        rescanSource(existing);
        return existing;
    }

    QString label = name.trimmed();
    if (label.isEmpty()) {
        label = url.fileName().isEmpty() ? url.toDisplayString() : url.fileName();
    }
    QSqlQuery insert = Database::exec(u"INSERT INTO sources (url, name, recursive) VALUES (?, ?, ?)"_s, {Store::urlKey(url), label, recursive});
    const qint64 id = insert.lastInsertId().toLongLong();
    if (id == 0) {
        return 0;
    }
    Q_EMIT sourcesChanged();
    touch();
    checkAvailability();
    rescanSource(id);
    return id;
}

void Library::removeSource(qint64 id)
{
    const Source source = Store::source(id);
    if (source.id == 0) {
        return;
    }
    unwatchSource(source);

    QList<qint64> docs;
    // The parts of a document stay or go with it.
    QSqlQuery query = Database::exec(u"SELECT id FROM documents WHERE source_id = ? AND parent_id = 0"_s, {id});
    while (query.next()) {
        docs.append(query.value(0).toLongLong());
    }
    query.finish();
    {
        Transaction transaction;
        for (qint64 doc : std::as_const(docs)) {
            // Documents the user worked with stay, for as long as that work is attached to them.
            if (Store::hasUserData(doc)) {
                Database::exec(u"UPDATE documents SET source_id = 0, leftover = 1 WHERE id = ?"_s, {doc});
                Database::exec(u"UPDATE documents SET source_id = 0 WHERE parent_id = ?"_s, {doc});
            } else {
                Store::removeDocument(doc);
            }
        }
        Database::exec(u"DELETE FROM sources WHERE id = ?"_s, {id});
        transaction.commit();
    }
    pruneViewColumns();
    m_unavailableSources.remove(id);
    Q_EMIT sourcesChanged();
    touch();
    tidyIfDue();
}

bool Library::sourceUsesInbox(qint64 id) const
{
    return Store::source(id).inbox;
}

void Library::setSourceUsesInbox(qint64 id, bool inbox)
{
    const Source source = Store::source(id);
    if (source.id == 0 || source.inbox == inbox) {
        return;
    }
    Database::exec(u"UPDATE sources SET inbox = ? WHERE id = ?"_s, {inbox, id});
    Q_EMIT sourcesChanged();
}

void Library::tidyIfDue()
{
    if (m_started && Maintenance::isDue()) {
        m_indexer->tidy();
    }
}

QString Library::databaseSizeText() const
{
    return m_databaseSize < 0 ? QString() : KFormat().formatByteSize(double(m_databaseSize), 1);
}

void Library::measureDatabase()
{
    if (m_measuring) {
        return;
    }
    m_measuring = true;
    m_countPool.start([this, database = m_databaseTicket, dir = Paths::dataDir()] {
        // Everything in the folder: the index and what belongs to it, the documents made
        // in Indeks, the working copies, the scripts. Going through a folder of many copies
        // takes a moment, which is why this is not done where the window would wait.
        qint64 total = 0;
        QDirIterator files(dir, QDir::Files | QDir::Hidden | QDir::NoSymLinks, QDirIterator::Subdirectories);
        while (files.hasNext() && !m_renditionAbort) {
            total += files.nextFileInfo().size();
        }
        QMetaObject::invokeMethod(
            this,
            [this, database, total] {
                m_measuring = false;
                if (database != m_databaseTicket) {
                    measureDatabase(); // another database by now: that one, then
                    return;
                }
                if (total != m_databaseSize) {
                    m_databaseSize = total;
                    Q_EMIT databaseSizeChanged();
                }
            },
            Qt::QueuedConnection);
    });
}

void Library::dropAbandonedLater()
{
    m_countPool.start([this, database = m_databaseTicket] {
        // With a connection of its own, for this once: the thread is the pool's, and what
        // it keeps open would be in the way when another database is switched to.
        const bool any = Store::hasAbandonedCandidates();
        Database::closeConnection();
        QMetaObject::invokeMethod(
            this,
            [this, database, any] {
                if (database != m_databaseTicket || !Database::isOpen()) {
                    return; // another database by now
                }
                if (any) {
                    // By now the list of documents is in memory, and this is quick.
                    dropAbandoned();
                }
                tidyIfDue();
            },
            Qt::QueuedConnection);
    });
}

void Library::dropAbandoned()
{
    // Only documents that are here for the user's work alone can be abandoned: those whose
    // file is gone and those left over from a location that is no longer indexed. There are
    // few of them, so looking at all is cheap.
    QList<Document> candidates;
    QSqlQuery query = Database::exec(u"SELECT %1 FROM documents d WHERE %2"_s.arg(Store::documentColumns(), Store::abandonedCondition()));
    while (query.next()) {
        candidates.append(Store::readDocument(query));
    }
    query.finish();

    QStringList removed;
    QStringList trashed;
    for (const Document &doc : std::as_const(candidates)) {
        if (Store::hasUserData(doc.id)) {
            continue;
        }
        if (doc.missing && doc.sourceId != 0) {
            // Its file is gone from a location that is still indexed. Like every document
            // of a file that went, it is not dropped but put into the trash.
            if (Store::fileVanished(doc.id)) {
                trashed.append(doc.name);
            }
        } else {
            // This forgets the document and deletes Indeks' own copy of it, if there is one.
            // A file that still exists in its place is the user's and is not touched.
            Store::removeDocument(doc.id);
            removed.append(doc.name);
        }
    }
    if (removed.isEmpty() && trashed.isEmpty()) {
        return;
    }
    if (!removed.isEmpty()) {
        Q_EMIT message(removed.size() == 1
                           ? i18nc("@info", "“%1” was only kept for what was attached to it and is no longer in Indeks.", removed.constFirst())
                           : i18ncp("@info",
                                    "%1 document was only kept for what was attached to it and is no longer in Indeks.",
                                    "%1 documents were only kept for what was attached to them and are no longer in Indeks.",
                                    removed.size()));
    }
    if (!trashed.isEmpty()) {
        Q_EMIT message(trashed.size() == 1
                           ? i18nc("@info", "“%1” was only kept for what was attached to it. Its file is gone, so it is in the trash now.", trashed.constFirst())
                           : i18ncp("@info",
                                    "%1 document was only kept for what was attached to it. Its file is gone, so it is in the trash now.",
                                    "%1 documents were only kept for what was attached to them. Their files are gone, so they are in the trash now.",
                                    trashed.size()));
    }
    touch();
    tidyIfDue();
}

void Library::rescanSource(qint64 id)
{
    const Source source = Store::source(id);
    if (source.id == 0) {
        return;
    }
    if (source.isLocal()) {
        m_indexer->scanSource(id);
    } else {
        m_remote->scanSource(id);
    }
}

void Library::rescanAll()
{
    const QList<Source> all = Store::sources();
    for (const Source &source : all) {
        rescanSource(source.id);
    }
}

void Library::watchSource(const Source &source)
{
    if (!source.isLocal() || m_watchedSources.contains(source.id)) {
        return;
    }
    KDirWatch::WatchModes modes = KDirWatch::WatchFiles;
    if (source.recursive) {
        modes |= KDirWatch::WatchSubDirs;
    }
    m_watch->addDir(source.url.toLocalFile(), modes);
    m_watchedSources.insert(source.id);
}

void Library::unwatchSource(const Source &source)
{
    if (m_watchedSources.remove(source.id)) {
        m_watch->removeDir(source.url.toLocalFile());
    }
}

void Library::checkAvailability()
{
    // The database itself first: a share can go away while Indeks is running.
    const QString database = Paths::dataDir();
    if (!QFileInfo(database + "/index.sqlite"_L1).isFile()) {
        if (!m_databaseLost) {
            m_databaseLost = true;
            Q_EMIT databaseLost();
        }
        return;
    }
    DatabaseFolder::refresh(database);

    bool anyChange = false;
    const QList<Source> all = Store::sources();
    for (const Source &source : all) {
        if (!source.isLocal()) {
            continue;
        }
        const bool available = QFileInfo(source.url.toLocalFile()).isDir();
        const bool wasAvailable = !m_unavailableSources.contains(source.id);
        if (available) {
            watchSource(source);
        } else {
            unwatchSource(source);
        }
        if (available == wasAvailable) {
            continue;
        }
        anyChange = true;
        if (available) {
            // A drive was plugged in or a share mounted: catch up on what changed meanwhile.
            m_unavailableSources.remove(source.id);
            if (m_started) {
                m_indexer->scanSource(source.id);
            }
        } else {
            m_unavailableSources.insert(source.id);
        }
    }
    if (anyChange) {
        Q_EMIT sourcesChanged();
        touch();
    }
}

void Library::onPathChanged(const QString &path, bool created)
{
    const QFileInfo info(path);
    if (info.isDir()) {
        // A new directory may arrive with content already in it.
        m_pendingDirectories[path] = m_pendingDirectories.value(path) || created;
    } else {
        const QString parent = info.path();
        if (!m_pendingDirectories.contains(parent)) {
            m_pendingDirectories.insert(parent, false);
        }
        if (!info.exists()) {
            // Possibly a deleted directory: a recursive scan of it drops the documents below.
            m_pendingDirectories[path] = true;
        }
    }
    m_watchTimer.start();
}

void Library::dispatchPendingDirectories()
{
    const QHash<QString, bool> pending = m_pendingDirectories;
    m_pendingDirectories.clear();
    const QList<Source> all = Store::sources();
    for (auto it = pending.constBegin(); it != pending.constEnd(); ++it) {
        const Source *best = nullptr;
        qsizetype bestLength = -1;
        for (const Source &source : all) {
            if (!source.isLocal()) {
                continue;
            }
            const QString root = source.url.toLocalFile();
            if ((it.key() == root || it.key().startsWith(root + u'/')) && root.size() > bestLength) {
                best = &source;
                bestLength = root.size();
            }
        }
        if (best) {
            m_indexer->scanDirectory(best->id, it.key(), it.value());
        }
    }
}

// ---------------------------------------------------------------------------------------------
// Adding documents

qint64 Library::addFolders(const QList<QUrl> &urls)
{
    qint64 added = 0;
    QStringList files;
    for (const QUrl &url : urls) {
        if (!url.isValid()) {
            continue;
        }
        if (!url.isLocalFile()) {
            // Only the server knows whether this is a folder; see folderFound().
            m_remote->checkFolder(url);
            continue;
        }
        const QFileInfo info(url.toLocalFile());
        if (info.isDir()) {
            if (const qint64 id = addSource(url); id != 0) {
                added = id;
            }
        } else if (info.exists()) {
            files.append(info.fileName());
        } else {
            Q_EMIT message(i18nc("@info", "“%1” does not exist.", url.toLocalFile()));
        }
    }
    if (files.size() == 1) {
        Q_EMIT message(i18nc("@info %1 is the name of a file",
                             "“%1” is a file. Indeks takes folders, not single files: add the folder it is in as an indexed location.",
                             files.constFirst()));
    } else if (!files.isEmpty()) {
        Q_EMIT message(i18ncp("@info",
                              "%1 file was not added. Indeks takes folders, not single files: add the folder it is in as an indexed location.",
                              "%1 files were not added. Indeks takes folders, not single files: add the folders they are in as indexed locations.",
                              files.size()));
    }
    return added;
}

qint64 Library::adoptOwnedFile(const QString &path)
{
    Indexing::FileJob job;
    job.path = path;
    job.url = QUrl::fromLocalFile(path);
    job.owned = true;
    job.inbox = true;
    Indexing::Result result;
    {
        Transaction transaction;
        result = Indexing::indexFile(job, m_options);
        transaction.commit();
    }
    if (result.id == 0) {
        return 0;
    }
    applyImportRules({result.id});
    if (result.wantsOcr) {
        m_indexer->processOcrQueue();
    }
    touch();
    Q_EMIT documentCreated(result.id);
    return result.id;
}

qint64 Library::createOwnedFile(const QString &name, const QString &extension, const QByteArray &data)
{
    QString fileName = cleanName(name);
    if (fileName.isEmpty()) {
        fileName = i18nc("default name of a new document", "Untitled");
    }
    if (!fileName.endsWith(extension, Qt::CaseInsensitive)) {
        fileName += extension;
    }
    const QString path = Paths::uniqueFilePath(Paths::filesDir(), fileName);

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) < 0 || !file.commit()) {
        Q_EMIT message(i18nc("@info", "Cannot create “%1”: %2", fileName, file.errorString()));
        return 0;
    }
    return adoptOwnedFile(path);
}

qint64 Library::createDocument(const QString &name, const QString &format, const QString &content)
{
    const QString extension = format == "markdown"_L1 ? u".md"_s : format == "html"_L1 ? u".html"_s : u".txt"_s;
    return createOwnedFile(name, extension, content.toUtf8());
}

qint64 Library::createDocumentFromHtml(const QString &name, const QString &html)
{
    return createOwnedFile(name, u".html"_s, pageOfForeignHtml(html).toUtf8());
}

qint64 Library::createWebLink(const QString &name, const QUrl &address)
{
    // Asked again here: only an address a browser is the right program for is kept.
    const QUrl checked = WebLink::addressOfText(QString::fromUtf8(address.toEncoded()));
    if (!checked.isValid()) {
        Q_EMIT message(i18nc("@info", "Cannot create “%1”: “%2” is not the address of a website.", name, address.toDisplayString()));
        return 0;
    }
    return createOwnedFile(name, u".url"_s, WebLink::fileContent(checked));
}

QString Library::webAddress(qint64 id) const
{
    const Document doc = Store::document(id);
    if (!doc.isValid() || doc.kind != Kind::Website) {
        return {};
    }
    const QString path = contentPath(id);
    return path.isEmpty() ? QString() : QString::fromUtf8(WebLink::read(path).toEncoded());
}

qint64 Library::createPicture(const QString &name, const QImage &image)
{
    QByteArray data;
    QBuffer buffer(&data);
    if (image.isNull() || !buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG")) {
        Q_EMIT message(i18nc("@info", "Cannot create “%1”: the picture could not be written.", name));
        return 0;
    }
    return createOwnedFile(name, u".png"_s, data);
}

// ---------------------------------------------------------------------------------------------
// Documents

QVariantMap Library::documentInfo(qint64 id) const
{
    const Document doc = Store::document(id);
    if (!doc.isValid()) {
        return {{u"valid"_s, false}, {u"id"_s, 0}};
    }
    static const QMimeDatabase mimeDatabase;
    const QMimeType mime = mimeDatabase.mimeTypeForName(doc.mimeType);
    const Source source = doc.sourceId != 0 ? Store::source(doc.sourceId) : Source();
    const bool local = doc.url.isLocalFile();
    const QString contentPath = Store::contentPath(doc);
    // A message is in its mailbox, and that is where it is said to be.
    const Document parent = doc.isPart() ? Store::document(doc.parentId) : Document();
    const bool container = isContainerMimeType(doc.mimeType);

    QStringList tags;
    QSqlQuery tagQuery =
        Database::exec(u"SELECT t.name FROM document_tags dt JOIN tags t ON t.id = dt.tag_id WHERE dt.doc_id = ? ORDER BY t.name COLLATE NOCASE"_s, {id});
    while (tagQuery.next()) {
        tags.append(tagQuery.value(0).toString());
    }

    QVariantList groupList;
    QSqlQuery groupQuery =
        Database::exec(u"SELECT f.id, f.name FROM document_folders df JOIN folders f ON f.id = df.folder_id WHERE df.doc_id = ? ORDER BY f.name COLLATE NOCASE"_s,
                       {id});
    while (groupQuery.next()) {
        groupList.append(QVariantMap{{u"id"_s, groupQuery.value(0)}, {u"name"_s, groupQuery.value(1)}});
    }

    QVariantList metadata;
    QSqlQuery metaQuery = Database::exec(u"SELECT key, value FROM metadata WHERE doc_id = ? ORDER BY key"_s, {id});
    while (metaQuery.next()) {
        const QString key = metaQuery.value(0).toString();
        const QVariant value = key == "unreadable"_L1 ? QVariant(i18nc("@info about the content of a document", "Could not be read; found by its name only")) : metaQuery.value(1);
        metadata.append(QVariantMap{{u"label"_s, metadataLabel(key)}, {u"value"_s, value}});
    }

    const bool annotated = Database::value(u"SELECT EXISTS (SELECT 1 FROM annotations WHERE doc_id = ?)"_s, {id}).toBool();
    const QString location = doc.isPart() ? i18nc("@info where a message is: in the mailbox of this name", "In the mailbox “%1”", parent.name)
        : local ? QFileInfo(doc.url.toLocalFile()).path()
                : doc.url.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash).toDisplayString();

    return {
        {u"valid"_s, true},
        {u"id"_s, doc.id},
        {u"name"_s, doc.name},
        {u"url"_s, doc.url},
        {u"displayUrl"_s, doc.isPart() ? (parent.url.isLocalFile() ? parent.url.toLocalFile() : parent.url.toDisplayString())
                                       : local ? doc.url.toLocalFile() : doc.url.toDisplayString()},
        {u"parentId"_s, doc.parentId},
        {u"parentName"_s, parent.name},
        // A document that holds others, and how many.
        {u"container"_s, container},
        {u"parts"_s, container ? Store::partCount(doc.id) : 0},
        {u"location"_s, doc.owned ? i18nc("@info where a document is stored", "Stored in Indeks") : location},
        {u"local"_s, local},
        {u"mime"_s, doc.mimeType},
        {u"mimeComment"_s, doc.kind == Kind::Canvas    ? i18nc("@info type of document", "Obsidian canvas")
                               : doc.kind == Kind::Website ? i18nc("@info type of document", "Website")
                               : mime.isValid()            ? mime.comment()
                                                           : doc.mimeType},
        // For a website: where it is on the web. Empty if the file holds no such address.
        {u"webAddress"_s, doc.kind == Kind::Website ? webAddress(doc.id) : QString()},
        {u"kind"_s, int(doc.kind)},
        {u"icon"_s, container ? u"folder-mail"_s : doc.kind == Kind::Website ? u"globe"_s : mime.isValid() ? mime.iconName() : kindIconName(doc.kind)},
        {u"size"_s, doc.size},
        {u"sizeText"_s, KFormat().formatByteSize(double(doc.size))},
        {u"modifiedText"_s, formatDate(doc.modified)},
        {u"addedText"_s, formatDate(doc.added)},
        {u"tags"_s, tags},
        {u"groups"_s, groupList},
        {u"comment"_s, doc.comment},
        {u"flagged"_s, doc.flagged},
        {u"inbox"_s, doc.inbox},
        {u"trashed"_s, doc.trashed},
        {u"missing"_s, doc.missing},
        {u"owned"_s, doc.owned},
        {u"hasLocalCopy"_s, !doc.localCopy.isEmpty()},
        {u"office"_s, Office::isOfficeDocument(doc.mimeType)},
        {u"editable"_s, doc.owned || !doc.localCopy.isEmpty()},
        {u"sourceId"_s, doc.sourceId},
        {u"sourceName"_s, source.name},
        {u"offline"_s, doc.sourceId != 0 && !isSourceAvailable(doc.sourceId)},
        {u"contentPath"_s, contentPath},
        {u"contentUrl"_s, contentPath.isEmpty() ? QUrl() : QUrl::fromLocalFile(contentPath)},
        {u"pages"_s, doc.pages},
        {u"ocr"_s, int(doc.ocr)},
        {u"annotated"_s, annotated},
        {u"metadata"_s, metadata},
    };
}

int Library::count(const QString &queryText) const
{
    return Store::count(queryText);
}

QString Library::contentPath(qint64 id) const
{
    return Store::ensureContent(Store::document(id));
}

void Library::requestContent(qint64 id)
{
    const Document doc = Store::document(id);
    if (!doc.isValid()) {
        Q_EMIT contentFailed(id, i18nc("@info", "This document is no longer in the index."));
        return;
    }
    // A message of a mailbox is taken out of it here, into a file of its own.
    const QString path = Store::ensureContent(doc);
    if (!path.isEmpty()) {
        // Individually added files are not watched; notice changes when they are looked at.
        if (doc.sourceId == 0 && !doc.owned && doc.localCopy.isEmpty() && doc.url.isLocalFile()) {
            const QFileInfo info(path);
            if (info.size() != doc.size || info.lastModified().toSecsSinceEpoch() != doc.modified.toSecsSinceEpoch()) {
                m_indexer->reindexDocument(id);
            }
        }
        Q_EMIT contentReady(id, path);
        return;
    }
    if (doc.isPart()) {
        Q_EMIT contentFailed(id, i18nc("@info", "The mailbox “%1” this message is in cannot be read at the moment.", Store::document(doc.parentId).name));
        return;
    }
    if (doc.url.isLocalFile()) {
        const bool offline = doc.sourceId != 0 && !isSourceAvailable(doc.sourceId);
        Q_EMIT contentFailed(id,
                             offline ? i18nc("@info", "The location “%1” is not connected at the moment.", Store::source(doc.sourceId).name)
                                     : i18nc("@info", "The file “%1” cannot be found.", doc.url.toLocalFile()));
        return;
    }
    if (m_fetching.contains(id)) {
        return;
    }
    m_fetching.insert(id);
    const QString target = Store::cachePath(doc);
    QDir().mkpath(QFileInfo(target).path());
    KIO::FileCopyJob *job = KIO::file_copy(doc.url, QUrl::fromLocalFile(target), 0600, KIO::HideProgressInfo | KIO::Overwrite);
    connect(job, &KJob::result, this, [this, id, target, job] {
        m_fetching.remove(id);
        if (job->error()) {
            Q_EMIT contentFailed(id, job->errorString());
        } else {
            Q_EMIT contentReady(id, target);
        }
    });
}

bool Library::officeViewAvailable() const
{
    return Office::converterAvailable();
}

void Library::requestRendition(qint64 id)
{
    const Document doc = Store::document(id);
    const QString path = doc.isValid() ? Store::contentPath(doc) : QString();
    if (path.isEmpty()) {
        Q_EMIT renditionFailed(id, i18nc("@info", "The file cannot be read."));
        return;
    }
    const RenditionRequest request{id, path};
    if (m_renditionRunning) {
        m_renditionWaiting = request;
        return;
    }
    startRendition(request);
}

void Library::startRendition(const RenditionRequest &request)
{
    m_renditionRunning = true;
    m_renditionPool.start([this, request] {
        Task::setAbortFlag(&m_renditionAbort);
        QString error;
        const QString pdf = Office::renderToPdf(request.path, hashFile(request.path), u"view"_s, RenditionTimeoutMs, &error);
        if (m_renditionAbort) {
            return; // the library is going away
        }
        QMetaObject::invokeMethod(
            this,
            [this, request, pdf, error] {
                m_renditionRunning = false;
                if (pdf.isEmpty()) {
                    Q_EMIT renditionFailed(request.id, error);
                } else {
                    Q_EMIT renditionReady(request.id, QUrl::fromLocalFile(pdf));
                }
                if (m_renditionWaiting.id != 0) {
                    const RenditionRequest next = m_renditionWaiting;
                    m_renditionWaiting = {};
                    startRendition(next);
                }
            },
            Qt::QueuedConnection);
    });
}

QString Library::ensureLocalCopy(qint64 id)
{
    const Document doc = Store::document(id);
    if (!doc.isValid()) {
        return {};
    }
    if (doc.owned) {
        return doc.url.toLocalFile();
    }
    if (!doc.localCopy.isEmpty() && QFileInfo::exists(doc.localCopy)) {
        return doc.localCopy;
    }
    const QString source = Store::contentPath(doc);
    if (source.isEmpty()) {
        return {};
    }
    const QString dir = Store::copyDir(id);
    QDir().mkpath(dir);
    const QString target = dir + u'/' + cleanName(doc.name);
    QFile::remove(target);
    if (!QFile::copy(source, target)) {
        Q_EMIT message(i18nc("@info", "Cannot copy “%1” into Indeks' data folder.", doc.name));
        return {};
    }
    QFile::setPermissions(target, QFile::permissions(target) | QFileDevice::WriteOwner);
    Database::exec(u"UPDATE documents SET local_copy = ? WHERE id = ?"_s, {target, id});
    touchDocument(id);
    return target;
}

void Library::discardLocalCopy(qint64 id)
{
    const Document doc = Store::document(id);
    if (!doc.isValid() || doc.owned || doc.localCopy.isEmpty()) {
        return;
    }
    QDir(Store::copyDir(id)).removeRecursively();
    Database::exec(u"UPDATE documents SET local_copy = '' WHERE id = ?"_s, {id});
    m_indexer->reindexDocument(id);
    touchDocument(id);
    dropAbandoned();
}

namespace
{
/**
 * The lines of a file that have something on them, which for JSON Lines is the number of
 * records. Returns -1 if @p cancelled said to stop, which is asked once per megabyte.
 */
int countJsonLines(const QString &path, const std::function<bool()> &cancelled)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return 0;
    }
    int total = 0;
    bool content = false;
    while (!file.atEnd()) {
        if (cancelled && cancelled()) {
            return -1;
        }
        const QByteArray chunk = file.read(1024 * 1024);
        for (const char c : chunk) {
            if (c == '\n') {
                total += content ? 1 : 0;
                content = false;
            } else if (c != ' ' && c != '\t' && c != '\r') {
                content = true;
            }
        }
    }
    return total + (content ? 1 : 0);
}
}

QString Library::readText(qint64 id) const
{
    const QString path = contentPath(id);
    return path.isEmpty() ? QString() : readTextFile(path, MaxPreviewTextBytes);
}

QString Library::guessedTextFormat(qint64 id) const
{
    const Document doc = Store::document(id);
    if (!doc.isValid() || doc.kind != Kind::PlainText || doc.mimeType != "text/plain"_L1) {
        return {};
    }
    const QString path = contentPath(id);
    if (path.isEmpty()) {
        return {};
    }
    switch (TextFormat::guess(readTextFile(path, GuessedFormatBytes))) {
    case TextFormat::Guess::Markdown:
        return u"markdown"_s;
    case TextFormat::Guess::Table:
        return u"table"_s;
    case TextFormat::Guess::Plain:
        break;
    }
    return {};
}

QVariantMap Library::formattedText(qint64 id, int maxRecords) const
{
    const Document doc = Store::document(id);
    const QString path = contentPath(id);
    if (!doc.isValid() || path.isEmpty() || doc.kind != Kind::PlainText) {
        return {};
    }
    static const QMimeDatabase mimeDatabase;
    const bool lines = isJsonLinesFile(doc.name);
    const bool json = !lines && mimeDatabase.mimeTypeForName(doc.mimeType).inherits(u"application/json"_s);
    if (!lines && !json) {
        return {};
    }
    const QString text = readTextFile(path, MaxPreviewTextBytes);

    if (json) {
        if (!isMinifiedJson(text)) {
            return {};
        }
        return {
            {u"text"_s, reindentJson(text)},
            {u"recordLines"_s, QVariant::fromValue(QList<int>{0})},
            {u"sourceLines"_s, QVariant::fromValue(QList<int>{1})},
            {u"shown"_s, 1},
            {u"total"_s, 1},
            {u"lines"_s, false},
        };
    }

    const FormattedJson formatted = formatJsonLines(text, maxRecords, maxRecords > 0 ? MaxQuickFormatCharacters : 0);
    // How many records there are in all, including what the preview does not read. For a file
    // that goes beyond what was read here that means reading all of it, gigabytes maybe: not
    // now, while the user waits for the document to show. See countRecords().
    const int shown = int(formatted.recordLines.size());
    const bool beyond = QFileInfo(path).size() > MaxPreviewTextBytes;
    int total = shown;
    if (beyond) {
        total = -1;
    } else if (formatted.cut) {
        total = qMax(shown, countJsonLines(path, nullptr));
    }
    return {
        {u"text"_s, formatted.text},
        {u"recordLines"_s, QVariant::fromValue(formatted.recordLines)},
        {u"sourceLines"_s, QVariant::fromValue(formatted.sourceLines)},
        {u"shown"_s, shown},
        {u"total"_s, total},
        {u"more"_s, formatted.cut || beyond},
        {u"lines"_s, true},
    };
}

void Library::countRecords(qint64 id)
{
    const QString path = contentPath(id);
    if (path.isEmpty()) {
        return;
    }
    // Only the document asked about last is of interest: whoever looks through a folder of
    // large files does not wait for the ones passed by.
    const quint64 ticket = ++m_countTicket;
    m_countPool.start([this, id, path, ticket] {
        if (ticket != m_countTicket) {
            return;
        }
        const int total = countJsonLines(path, [this, ticket] {
            return m_renditionAbort || ticket != m_countTicket;
        });
        if (total < 0) {
            return;
        }
        QMetaObject::invokeMethod(
            this,
            [this, id, total] {
                Q_EMIT recordsCounted(id, total);
            },
            Qt::QueuedConnection);
    });
}

qint64 Library::maxTextSize() const
{
    return MaxPreviewTextBytes;
}

QString Library::indexedText(qint64 id) const
{
    return Store::fullText(id);
}

bool Library::saveText(qint64 id, const QString &text)
{
    const QString path = ensureLocalCopy(id);
    if (path.isEmpty()) {
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(text.toUtf8()) < 0 || !file.commit()) {
        Q_EMIT message(i18nc("@info", "Cannot save: %1", file.errorString()));
        return false;
    }
    contentSaved(id);
    return true;
}

void Library::contentSaved(qint64 id)
{
    {
        Transaction transaction;
        Indexing::reindexDocument(id, m_options);
        transaction.commit();
    }
    touchDocument(id);
}

bool Library::renameDocument(qint64 id, const QString &name)
{
    const Document doc = Store::document(id);
    const QString newName = cleanName(name);
    if (!doc.isValid() || newName.isEmpty() || newName == doc.name) {
        return false;
    }
    if (!doc.owned) {
        Q_EMIT message(i18nc("@info", "Only documents stored in Indeks can be renamed. Indeks never changes the original files."));
        return false;
    }
    const QString oldPath = doc.url.toLocalFile();
    const QString newPath = Paths::uniqueFilePath(QFileInfo(oldPath).path(), newName);
    if (!QFile::rename(oldPath, newPath)) {
        Q_EMIT message(i18nc("@info", "Cannot rename “%1”.", doc.name));
        return false;
    }
    const QUrl url = QUrl::fromLocalFile(newPath);
    Database::exec(u"UPDATE documents SET url = ?, name = ? WHERE id = ?"_s, {Store::urlKey(url), url.fileName(), id});
    contentSaved(id); // the new extension may change the type
    return true;
}

void Library::setComment(qint64 id, const QString &comment)
{
    // No comment is an empty one, never none at all.
    Database::exec(u"UPDATE documents SET comment = ? WHERE id = ?"_s, {comment.isEmpty() ? u""_s : comment, id});
    Store::refreshNotes(id);
    touchDocument(id);
    if (comment.isEmpty()) {
        dropAbandoned();
    }
}

void Library::setFlagged(const QVariantList &ids, bool flagged)
{
    const QList<qint64> list = toIdList(ids);
    if (list.isEmpty()) {
        return;
    }
    Database::exec(u"UPDATE documents SET flagged = ? WHERE id IN (%1)"_s.arg(Store::idList(list)), {flagged});
    if (flagged) {
        // A document that was pinned by hand has been looked at: it no longer waits in the inbox.
        Database::exec(u"UPDATE documents SET inbox = 0 WHERE inbox = 1 AND id IN (%1)"_s.arg(Store::idList(list)));
    }
    for (qint64 id : list) {
        Q_EMIT documentChanged(id);
    }
    touch();
    if (!flagged) {
        dropAbandoned();
    }
}

void Library::setInbox(const QVariantList &ids, bool inbox)
{
    const QList<qint64> list = toIdList(ids);
    if (list.isEmpty()) {
        return;
    }
    Database::exec(u"UPDATE documents SET inbox = ? WHERE id IN (%1)"_s.arg(Store::idList(list)), {inbox});
    for (qint64 id : list) {
        Q_EMIT documentChanged(id);
    }
    touch();
}

void Library::moveToTrash(const QVariantList &ids)
{
    const QList<qint64> list = toIdList(ids);
    if (list.isEmpty()) {
        return;
    }
    // What a document holds goes with it, and comes back with it.
    Database::exec(u"UPDATE documents SET trashed = 1 WHERE trashed = 0 AND (id IN (%1) OR parent_id IN (%1))"_s.arg(Store::idList(list)));
    touch();
}

void Library::restoreFromTrash(const QVariantList &ids)
{
    const QList<qint64> list = toIdList(ids);
    if (list.isEmpty()) {
        return;
    }
    // A document that is in the trash because its file went has nothing to come back to,
    // unless something of the user's is attached to it: then it returns as a document whose
    // file is missing. Where the file is there again, it is read.
    QList<qint64> restored;
    QList<qint64> back;
    QStringList gone;
    for (qint64 id : list) {
        const Document doc = Store::document(id);
        if (!doc.isValid()) {
            continue;
        }
        if (doc.missing && !doc.isPart()) {
            if (doc.url.isLocalFile() && QFileInfo::exists(doc.url.toLocalFile())) {
                back.append(id);
            } else if (!Store::hasUserData(id)) {
                gone.append(doc.name);
                continue;
            }
        }
        restored.append(id);
    }
    if (!restored.isEmpty()) {
        const QString ids = Store::idList(restored);
        Database::exec(u"UPDATE documents SET trashed = 0 WHERE trashed = 1 AND (id IN (%1) OR parent_id IN (%1))"_s.arg(ids));
        // From here on it is missing like any document whose file went and that was kept.
        Database::exec(u"UPDATE documents SET missing = 1 WHERE missing = 2 AND id IN (%1)"_s.arg(ids));
        for (qint64 id : std::as_const(back)) {
            m_indexer->reindexDocument(id);
        }
    }
    if (!gone.isEmpty()) {
        Q_EMIT message(gone.size() == 1
                           ? i18nc("@info", "The file of “%1” is gone, so there is nothing to restore. Put the file back where it was and the document leaves the trash by itself.", gone.constFirst())
                           : i18ncp("@info",
                                    "The file of %1 document is gone, so there is nothing to restore. Put the file back where it was and the document leaves the trash by itself.",
                                    "The files of %1 documents are gone, so there is nothing to restore. Put the files back where they were and the documents leave the trash by themselves.",
                                    gone.size()));
    }
    touch();
}

void Library::emptyTrash()
{
    QList<Document> trashed;
    QSqlQuery query = Database::exec(u"SELECT %1 FROM documents d WHERE d.trashed = 1"_s.arg(Store::documentColumns()));
    while (query.next()) {
        trashed.append(Store::readDocument(query));
    }
    query.finish();

    QList<QUrl> ownedFiles;
    {
        Transaction transaction;
        // Whole documents before parts of documents: what is in a document goes with it.
        std::stable_partition(trashed.begin(), trashed.end(), [](const Document &doc) {
            return !doc.isPart();
        });
        QSet<qint64> forgotten;
        for (const Document &doc : std::as_const(trashed)) {
            if (forgotten.contains(doc.parentId)) {
                continue;
            }
            if (doc.owned) {
                ownedFiles.append(doc.url);
                Store::removeDocument(doc.id);
                forgotten.insert(doc.id);
            } else if (doc.missing && !doc.isPart()) {
                // Its file is gone: there is nothing a scan could bring back.
                Store::removeDocument(doc.id);
                forgotten.insert(doc.id);
            } else if (doc.sourceId != 0 || doc.isPart()) {
                // The file still sits in an indexed location, or the message in its mailbox.
                // Keep a hidden entry so that the next scan does not bring the document back.
                Database::exec(u"UPDATE documents SET trashed = 2, inbox = 0 WHERE id = ?"_s, {doc.id});
                // It is gone as far as the user can see, and nothing is linked to what is gone.
                Database::exec(u"DELETE FROM document_links WHERE a_id = ? OR b_id = ?"_s, {doc.id, doc.id});
                Store::writeFullText(doc.id, doc.name, QString());
            } else {
                Store::removeDocument(doc.id);
            }
        }
        transaction.commit();
    }
    // Files Indeks created itself go to the desktop's trash, from where they can be restored.
    // Files that were merely indexed are left alone.
    if (!ownedFiles.isEmpty()) {
        KIO::trash(ownedFiles, KIO::HideProgressInfo);
    }
    touch();
    // A document elsewhere may have been kept for nothing but its link to one of these.
    dropAbandoned();
    tidyIfDue();
}

void Library::reindex(const QVariantList &ids)
{
    const QList<qint64> list = toIdList(ids);
    for (qint64 id : list) {
        // A message is read again by reading its mailbox again.
        const Document doc = Store::document(id);
        m_indexer->reindexDocument(doc.isPart() ? doc.parentId : id);
    }
}

void Library::requestOcr(const QVariantList &ids)
{
    if (!Ocr::isAvailable()) {
        Q_EMIT message(i18nc("@info", "Text recognition needs the program “tesseract”, which is not installed."));
        return;
    }
    const QList<qint64> list = toIdList(ids);
    bool any = false;
    for (qint64 id : list) {
        const Document doc = Store::document(id);
        if (!doc.isValid() || (doc.kind != Kind::Pdf && doc.kind != Kind::Image) || Store::contentPath(doc).isEmpty()) {
            continue;
        }
        // Extract afresh so that recognised text is not appended twice, then queue the document.
        Transaction transaction;
        Database::exec(u"UPDATE documents SET ocr = 0, hash = '' WHERE id = ?"_s, {id});
        Indexing::reindexDocument(id, m_options);
        Database::exec(u"UPDATE documents SET ocr = 1 WHERE id = ?"_s, {id});
        transaction.commit();
        any = true;
    }
    if (any) {
        m_indexer->processOcrQueue();
        touch();
    } else {
        Q_EMIT message(i18nc("@info", "Text recognition works on images and PDF documents whose file is available."));
    }
}

// ---------------------------------------------------------------------------------------------
// Tags

QVariantList Library::tags() const
{
    QVariantList result;
    QSqlQuery query = Database::exec(
        u"SELECT t.id, t.name, (SELECT COUNT(*) FROM document_tags dt JOIN documents d ON d.id = dt.doc_id WHERE dt.tag_id = t.id AND d.trashed = 0) "
        "FROM tags t ORDER BY t.name COLLATE NOCASE"_s);
    while (query.next()) {
        result.append(QVariantMap{{u"id"_s, query.value(0)}, {u"name"_s, query.value(1)}, {u"count"_s, query.value(2)}});
    }
    return result;
}

QStringList Library::tagNames() const
{
    QStringList result;
    QSqlQuery query = Database::exec(u"SELECT name FROM tags ORDER BY name COLLATE NOCASE"_s);
    while (query.next()) {
        result.append(query.value(0).toString());
    }
    return result;
}

qint64 Library::createTag(const QString &name)
{
    const qint64 id = Rules::tagId(name, true);
    touch();
    return id;
}

void Library::addTag(const QVariantList &ids, const QString &name)
{
    const QList<qint64> list = toIdList(ids);
    if (list.isEmpty()) {
        return;
    }
    Transaction transaction;
    const qint64 tag = Rules::tagId(name, true);
    if (tag == 0) {
        return;
    }
    for (qint64 id : list) {
        Database::exec(u"INSERT OR IGNORE INTO document_tags (doc_id, tag_id) VALUES (?, ?)"_s, {id, tag});
    }
    // Tagged by hand is worked with, and what is worked with leaves the inbox.
    Database::exec(u"UPDATE documents SET inbox = 0 WHERE inbox = 1 AND id IN (%1)"_s.arg(Store::idList(list)));
    transaction.commit();
    for (qint64 id : list) {
        Q_EMIT documentChanged(id);
    }
    touch();
}

void Library::removeTag(const QVariantList &ids, const QString &name)
{
    const QList<qint64> list = toIdList(ids);
    const qint64 tag = Rules::tagId(name, false);
    if (list.isEmpty() || tag == 0) {
        return;
    }
    Database::exec(u"DELETE FROM document_tags WHERE tag_id = ? AND doc_id IN (%1)"_s.arg(Store::idList(list)), {tag});
    for (qint64 id : list) {
        Q_EMIT documentChanged(id);
    }
    touch();
    dropAbandoned();
}

bool Library::renameTag(qint64 id, const QString &rawName)
{
    QString name = rawName.simplified();
    name.remove(u'"');
    if (name.isEmpty()) {
        return false;
    }
    const qint64 clash = Database::value(u"SELECT id FROM tags WHERE name = ? COLLATE NOCASE AND id <> ?"_s, {name, id}).toLongLong();
    if (clash != 0) {
        Q_EMIT message(i18nc("@info", "A tag named “%1” already exists.", name));
        return false;
    }
    Database::exec(u"UPDATE tags SET name = ? WHERE id = ?"_s, {name, id});
    touch();
    return true;
}

void Library::deleteTag(qint64 id)
{
    Database::exec(u"DELETE FROM tags WHERE id = ?"_s, {id});
    pruneViewColumns();
    touch();
    dropAbandoned();
}

// ---------------------------------------------------------------------------------------------
// Groups

QVariantList Library::groups() const
{
    struct Group {
        qint64 id;
        qint64 parent;
        QString name;
    };
    QList<Group> all;
    QSqlQuery query = Database::exec(u"SELECT id, COALESCE(parent_id, 0), name FROM folders ORDER BY name COLLATE NOCASE"_s);
    while (query.next()) {
        all.append({query.value(0).toLongLong(), query.value(1).toLongLong(), query.value(2).toString()});
    }
    QHash<qint64, int> counts;
    QSqlQuery countQuery = Database::exec(
        u"SELECT df.folder_id, COUNT(*) FROM document_folders df JOIN documents d ON d.id = df.doc_id WHERE d.trashed = 0 GROUP BY df.folder_id"_s);
    while (countQuery.next()) {
        counts.insert(countQuery.value(0).toLongLong(), countQuery.value(1).toInt());
    }
    QMultiHash<qint64, int> children; // parent id to index in `all`
    for (int i = int(all.size()) - 1; i >= 0; --i) {
        children.insert(all.at(i).parent, i); // reversed, QMultiHash returns the latest insert first
    }

    QVariantList result;
    std::function<void(qint64, int, const QString &)> walk = [&](qint64 parent, int depth, const QString &parentPath) {
        const QList<int> indexes = children.values(parent);
        for (int index : indexes) {
            const Group &group = all.at(index);
            const QString path = parentPath.isEmpty() ? group.name : QString(parentPath + " / "_L1 + group.name);
            result.append(QVariantMap{
                {u"id"_s, group.id},
                {u"parentId"_s, group.parent},
                {u"name"_s, group.name},
                {u"path"_s, path},
                {u"depth"_s, depth},
                {u"count"_s, counts.value(group.id)},
                {u"hasChildren"_s, children.contains(group.id)},
            });
            if (depth < 32) {
                walk(group.id, depth + 1, path);
            }
        }
    };
    walk(0, 0, QString());
    return result;
}

qint64 Library::createGroup(const QString &name, qint64 parentId)
{
    const QString trimmed = name.simplified();
    if (trimmed.isEmpty()) {
        return 0;
    }
    const qint64 id = Database::exec(u"INSERT INTO folders (name, parent_id) VALUES (?, ?)"_s, {trimmed, parentId > 0 ? QVariant(parentId) : QVariant()})
                          .lastInsertId()
                          .toLongLong();
    touch();
    return id;
}

void Library::renameGroup(qint64 id, const QString &name)
{
    const QString trimmed = name.simplified();
    if (trimmed.isEmpty()) {
        return;
    }
    Database::exec(u"UPDATE folders SET name = ? WHERE id = ?"_s, {trimmed, id});
    touch();
}

void Library::deleteGroup(qint64 id)
{
    // Subgroups and memberships go with it through the foreign keys; the documents stay.
    Database::exec(u"DELETE FROM folders WHERE id = ?"_s, {id});
    pruneViewColumns();
    touch();
    dropAbandoned();
}

bool Library::moveGroup(qint64 id, qint64 newParentId)
{
    // Refuse to move a group into itself or one of its descendants.
    for (qint64 ancestor = newParentId; ancestor != 0;) {
        if (ancestor == id) {
            return false;
        }
        ancestor = Database::value(u"SELECT COALESCE(parent_id, 0) FROM folders WHERE id = ?"_s, {ancestor}).toLongLong();
    }
    Database::exec(u"UPDATE folders SET parent_id = ? WHERE id = ?"_s, {newParentId > 0 ? QVariant(newParentId) : QVariant(), id});
    touch();
    return true;
}

void Library::addToGroup(const QVariantList &ids, qint64 groupId)
{
    const QList<qint64> list = toIdList(ids);
    if (list.isEmpty() || groupId <= 0) {
        return;
    }
    {
        Transaction transaction;
        for (qint64 id : list) {
            Database::exec(u"INSERT OR IGNORE INTO document_folders (doc_id, folder_id) VALUES (?, ?)"_s, {id, groupId});
        }
        // Filed documents leave the inbox.
        Database::exec(u"UPDATE documents SET inbox = 0 WHERE id IN (%1)"_s.arg(Store::idList(list)));
        transaction.commit();
    }
    for (qint64 id : list) {
        Q_EMIT documentChanged(id);
    }
    touch();
}

void Library::removeFromGroup(const QVariantList &ids, qint64 groupId)
{
    const QList<qint64> list = toIdList(ids);
    if (list.isEmpty()) {
        return;
    }
    Database::exec(u"DELETE FROM document_folders WHERE folder_id = ? AND doc_id IN (%1)"_s.arg(Store::idList(list)), {groupId});
    for (qint64 id : list) {
        Q_EMIT documentChanged(id);
    }
    touch();
    dropAbandoned();
}

// ---------------------------------------------------------------------------------------------
// Links between documents

QVariantList Library::linkedDocuments(qint64 id) const
{
    QList<qint64> others;
    // What was emptied from the trash is only remembered, not there.
    QSqlQuery query = Database::exec(u"SELECT d.id FROM documents d WHERE d.trashed <> 2 AND d.id IN "
                                     "(SELECT b_id FROM document_links WHERE a_id = ? UNION SELECT a_id FROM document_links WHERE b_id = ?) "
                                     "ORDER BY d.name COLLATE NOCASE, d.id"_s,
                                     {id, id});
    while (query.next()) {
        others.append(query.value(0).toLongLong());
    }
    query.finish();

    QVariantList result;
    for (qint64 other : std::as_const(others)) {
        const QVariantMap info = documentInfo(other);
        result.append(QVariantMap{
            {u"id"_s, other},
            {u"name"_s, info.value(u"name"_s)},
            {u"icon"_s, info.value(u"icon"_s)},
            {u"location"_s, info.value(u"location"_s)},
            {u"trashed"_s, info.value(u"trashed"_s).toInt() != 0},
            {u"missing"_s, info.value(u"missing"_s).toInt() != 0},
        });
    }
    return result;
}

int Library::linkDocuments(const QVariantList &ids)
{
    QList<qint64> list;
    const QList<qint64> given = toIdList(ids);
    for (qint64 id : given) {
        if (!list.contains(id) && Store::document(id).isValid()) {
            list.append(id);
        }
    }
    if (list.size() < 2) {
        return 0;
    }
    if (list.size() > MaxLinkedAtOnce) {
        // Every one with every other: the links of a hundred documents would be thousands.
        Q_EMIT message(i18ncp("@info",
                              "No more than %1 document can be linked with one another at once.",
                              "No more than %1 documents can be linked with one another at once.",
                              MaxLinkedAtOnce));
        return -1;
    }
    int added = 0;
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    {
        Transaction transaction;
        for (int i = 0; i < list.size(); ++i) {
            for (int j = i + 1; j < list.size(); ++j) {
                added += Database::exec(u"INSERT OR IGNORE INTO document_links (a_id, b_id, created) VALUES (?, ?, ?)"_s,
                                        {qMin(list.at(i), list.at(j)), qMax(list.at(i), list.at(j)), now})
                             .numRowsAffected();
            }
        }
        // Linked by hand is worked with, and what is worked with leaves the inbox.
        Database::exec(u"UPDATE documents SET inbox = 0 WHERE inbox = 1 AND id IN (%1)"_s.arg(Store::idList(list)));
        transaction.commit();
    }
    for (qint64 id : std::as_const(list)) {
        Q_EMIT documentChanged(id);
    }
    touch();
    return added;
}

void Library::unlinkDocuments(qint64 id, qint64 otherId)
{
    if (Database::exec(u"DELETE FROM document_links WHERE a_id = ? AND b_id = ?"_s, {qMin(id, otherId), qMax(id, otherId)}).numRowsAffected() <= 0) {
        return;
    }
    Q_EMIT documentChanged(id);
    Q_EMIT documentChanged(otherId);
    touch();
    // Either of them may have been kept for this link alone.
    dropAbandoned();
}

// ---------------------------------------------------------------------------------------------
// Saved searches

QVariantList Library::savedSearches() const
{
    QVariantList result;
    QSqlQuery query = Database::exec(u"SELECT id, name, query FROM searches ORDER BY name COLLATE NOCASE"_s);
    while (query.next()) {
        result.append(QVariantMap{{u"id"_s, query.value(0)}, {u"name"_s, query.value(1)}, {u"query"_s, query.value(2)}});
    }
    return result;
}

// Where the filters of a saved search are kept: with the single values of the database, as
// the columns of a view are, so that the table of searches is the one older versions know.
static QString searchFiltersKey(qint64 id)
{
    return u"search-filters:%1"_s.arg(id);
}

qint64 Library::saveSearch(qint64 id, const QString &name, const QString &query, const QVariantList &filters)
{
    const QString trimmed = name.simplified();
    const QString text = query.trimmed();
    if (trimmed.isEmpty() || text.isEmpty()) {
        return 0;
    }
    Transaction transaction;
    if (id > 0) {
        Database::exec(u"UPDATE searches SET name = ?, query = ? WHERE id = ?"_s, {trimmed, text, id});
    } else {
        id = Database::exec(u"INSERT INTO searches (name, query) VALUES (?, ?)"_s, {trimmed, text}).lastInsertId().toLongLong();
    }
    if (filters.isEmpty()) {
        Database::exec(u"DELETE FROM meta WHERE key = ?"_s, {searchFiltersKey(id)});
    } else {
        // With the query they come to: whoever changes the query alone leaves them behind,
        // and they are not taken for it then.
        const QJsonObject stored{{u"query"_s, text}, {u"filters"_s, QJsonArray::fromVariantList(filters)}};
        Database::exec(u"INSERT OR REPLACE INTO meta (key, value) VALUES (?, ?)"_s,
                       {searchFiltersKey(id), QString::fromUtf8(QJsonDocument(stored).toJson(QJsonDocument::Compact))});
    }
    transaction.commit();
    touch();
    return id;
}

QVariantList Library::savedSearchFilters(qint64 id) const
{
    const QJsonObject stored = QJsonDocument::fromJson(Database::value(u"SELECT value FROM meta WHERE key = ?"_s, {searchFiltersKey(id)}).toString().toUtf8()).object();
    const QString query = Database::value(u"SELECT query FROM searches WHERE id = ?"_s, {id}).toString();
    if (stored.isEmpty() || stored.value(u"query"_s).toString() != query) {
        return {};
    }
    return stored.value(u"filters"_s).toArray().toVariantList();
}

void Library::deleteSearch(qint64 id)
{
    Database::exec(u"DELETE FROM searches WHERE id = ?"_s, {id});
    Database::exec(u"DELETE FROM meta WHERE key = ?"_s, {searchFiltersKey(id)});
    pruneViewColumns();
    touch();
}

// ---------------------------------------------------------------------------------------------
// Rules

QVariantList Library::rules() const
{
    return Rules::all();
}

QVariantMap Library::rule(qint64 id) const
{
    return Rules::rule(id);
}

qint64 Library::saveRule(const QVariantMap &rule)
{
    const QString name = rule.value(u"name"_s).toString().simplified();
    const QString query = rule.value(u"query"_s).toString().trimmed();
    // Only what is known is kept, each action with what it needs and nothing else.
    const QJsonArray actions = Rules::storable(rule.value(u"actions"_s).toList());
    if (name.isEmpty() || query.isEmpty() || actions.isEmpty()) {
        Q_EMIT message(i18nc("@info", "A rule needs a name, a condition and something to do."));
        return 0;
    }
    const QVariantList values = {
        name,
        query,
        rule.value(u"enabled"_s, true).toBool(),
        rule.value(u"onImport"_s, true).toBool(),
        QString::fromUtf8(QJsonDocument(QJsonArray::fromVariantList(rule.value(u"filters"_s).toList())).toJson(QJsonDocument::Compact)),
        QString::fromUtf8(QJsonDocument(actions).toJson(QJsonDocument::Compact)),
    };
    qint64 id = rule.value(u"id"_s).toLongLong();
    if (id > 0) {
        Database::exec(u"UPDATE rules SET name = ?, query = ?, enabled = ?, on_import = ?, filters = ?, actions = ? WHERE id = ?"_s, values + QVariantList{id});
    } else {
        id = Database::exec(u"INSERT INTO rules (name, query, enabled, on_import, filters, actions) VALUES (?, ?, ?, ?, ?, ?)"_s, values).lastInsertId().toLongLong();
    }
    touch();
    Q_EMIT scriptsChanged(); // which rules run a script is part of what is shown about it
    return id;
}

void Library::deleteRule(qint64 id)
{
    Database::exec(u"DELETE FROM rules WHERE id = ?"_s, {id});
    pruneViewColumns();
    touch();
    Q_EMIT scriptsChanged();
}

int Library::applyRule(qint64 id)
{
    const QVariantMap rule = Rules::rule(id);
    if (Rules::runScripts(rule.value(u"actions"_s).toList())) {
        // Scripts take their time, and not that of the user interface.
        m_ruleRuns.insert(m_scripts->applyRules({id}, {}), rule.value(u"name"_s).toString());
        return -1;
    }
    bool took = false;
    const int affected = applyRuleTo(id, {}, &took);
    if (took) {
        dropAbandoned();
    }
    touch();
    return affected;
}

int Library::applyRuleTo(qint64 ruleId, const QList<qint64> &restrictTo, bool *took)
{
    const QVariantMap rule = Rules::rule(ruleId);
    const QList<qint64> matches = Rules::matching(rule.value(u"query"_s).toString(), restrictTo);
    *took = Rules::apply(matches, rule.value(u"actions"_s).toList()).took || *took;
    return int(matches.size());
}

void Library::applyImportRules(const QList<qint64> &ids)
{
    if (ids.isEmpty()) {
        return;
    }
    QList<qint64> ruleIds;
    bool scripted = false;
    QSqlQuery query = Database::exec(u"SELECT id, actions FROM rules WHERE enabled = 1 AND on_import = 1 ORDER BY id"_s);
    while (query.next()) {
        ruleIds.append(query.value(0).toLongLong());
        scripted = scripted || Rules::runScripts(QJsonDocument::fromJson(query.value(1).toString().toUtf8()).array().toVariantList());
    }
    query.finish();
    if (scripted) {
        // All of them go where the scripts run, to keep their order: a rule may look at
        // what the one before it did.
        m_scripts->applyRules(ruleIds, ids);
        return;
    }
    int affected = 0;
    bool took = false;
    for (qint64 ruleId : std::as_const(ruleIds)) {
        affected += applyRuleTo(ruleId, ids, &took);
    }
    if (affected > 0) {
        touch();
    }
}

// ---------------------------------------------------------------------------------------------
// Scripts

void Library::watchScripts()
{
    const QString dir = Scripts::dir();
    if (dir == m_watchedScripts) {
        return;
    }
    if (!m_watchedScripts.isEmpty()) {
        m_scriptWatch->removeDir(m_watchedScripts);
    }
    // The folder is made when the first script is saved; it is watched for all the same.
    m_scriptWatch->addDir(dir, KDirWatch::WatchFiles);
    m_watchedScripts = dir;
}

QString Library::scriptsProblem() const
{
    return ScriptHost::problem();
}

bool Library::scriptsRunning() const
{
    return m_scripts->isBusy();
}

QString Library::scriptsDir() const
{
    return Scripts::dir();
}

static QStringList rulesRunning(const QVariantList &rules, const QString &file)
{
    QStringList names;
    for (const QVariant &entry : rules) {
        const QVariantMap rule = entry.toMap();
        const QVariantList actions = rule.value(u"actions"_s).toList();
        for (const QVariant &action : actions) {
            if (action.toMap().value(u"type"_s).toString() == "script"_L1 && action.toMap().value(u"name"_s).toString() == file) {
                names.append(rule.value(u"name"_s).toString());
                break;
            }
        }
    }
    return names;
}

QVariantList Library::scripts() const
{
    QVariantList result;
    const QVariantList rules = Rules::all();
    const QList<Scripts::Script> all = Scripts::list();
    for (const Scripts::Script &script : all) {
        result.append(QVariantMap{
            {u"file"_s, script.file},
            {u"name"_s, script.name},
            {u"description"_s, script.description},
            {u"modifiedText"_s, formatDate(script.modified)},
            {u"allowed"_s, script.allowed},
            {u"rules"_s, rulesRunning(rules, script.file)},
        });
    }
    return result;
}

QVariantMap Library::script(const QString &file) const
{
    QString source;
    if (!Scripts::read(file, &source)) {
        return {};
    }
    return {
        {u"file"_s, file},
        {u"name"_s, file.chopped(3)},
        {u"source"_s, source},
        {u"description"_s, Scripts::description(source)},
        {u"allowed"_s, !Scripts::needApproval() || Scripts::isAllowed(source)},
        {u"rules"_s, rulesRunning(Rules::all(), file)},
    };
}

QString Library::newScriptText() const
{
    return Scripts::starter();
}

QString Library::saveScript(const QString &file, const QString &name, const QString &source)
{
    const QString wanted = Scripts::fileNameFor(name);
    if (wanted.isEmpty()) {
        Q_EMIT message(i18nc("@info", "A script needs a name."));
        return {};
    }
    QString problem;
    QString target = file;
    if (file != wanted) {
        // A new script, or a new name for one: neither may take the place of another.
        if (QFileInfo::exists(Scripts::dir() + u'/' + wanted)) {
            Q_EMIT message(i18nc("@info", "A script named “%1” already exists.", wanted.chopped(3)));
            return {};
        }
        if (!file.isEmpty()) {
            if (!Scripts::rename(file, wanted, &problem)) {
                Q_EMIT message(problem);
                return {};
            }
            // The rules that run it know it by the name of its file.
            const QVariantList rules = Rules::all();
            for (const QVariant &entry : rules) {
                const QVariantMap rule = entry.toMap();
                QVariantList actions = rule.value(u"actions"_s).toList();
                bool renamed = false;
                for (QVariant &action : actions) {
                    QVariantMap map = action.toMap();
                    if (map.value(u"type"_s).toString() == "script"_L1 && map.value(u"name"_s).toString() == file) {
                        map.insert(u"name"_s, wanted);
                        action = map;
                        renamed = true;
                    }
                }
                if (renamed) {
                    Database::exec(u"UPDATE rules SET actions = ? WHERE id = ?"_s,
                                   {QString::fromUtf8(QJsonDocument(QJsonArray::fromVariantList(actions)).toJson(QJsonDocument::Compact)), rule.value(u"id"_s)});
                }
            }
        }
        target = wanted;
    }
    if (!Scripts::write(target, source, &problem)) {
        Q_EMIT message(problem);
        return {};
    }
    Q_EMIT scriptsChanged();
    return target;
}

bool Library::deleteScript(const QString &file)
{
    if (!Scripts::remove(file)) {
        Q_EMIT message(i18nc("@info", "The script could not be moved to the trash."));
        return false;
    }
    Q_EMIT scriptsChanged();
    return true;
}

void Library::allowScript(const QString &file)
{
    QString source;
    if (Scripts::read(file, &source)) {
        Scripts::allow(source);
        Q_EMIT scriptsChanged();
    }
}

void Library::runScript(const QString &file, const QVariantList &ids)
{
    const QList<qint64> documents = toIdList(ids);
    if (!documents.isEmpty()) {
        m_scripts->runScript(file, documents);
    }
}

void Library::tryScript(const QString &name, const QString &source, const QVariantList &ids)
{
    m_scripts->tryScript(Scripts::fileNameFor(name).isEmpty() ? u"script.py"_s : Scripts::fileNameFor(name), source, toIdList(ids));
}

void Library::stopScripts()
{
    m_scripts->cancel();
    m_ruleRuns.clear();
}

QVariantList Library::scriptReports() const
{
    return m_scriptReports;
}

void Library::clearScriptReports()
{
    m_scriptReports.clear();
}

// ---------------------------------------------------------------------------------------------
// Annotations

QVariantList Library::notes(qint64 documentId) const
{
    QVariantList result;
    QSqlQuery query =
        Database::exec(u"SELECT id, page, data, modified, x, y FROM annotations WHERE doc_id = ? AND kind = 'note' ORDER BY page, y, id"_s, {documentId});
    while (query.next()) {
        const bool placed = query.value(1).toInt() >= 0 && query.value(4).toDouble() >= 0;
        result.append(QVariantMap{
            {u"id"_s, query.value(0)},
            {u"page"_s, placed ? query.value(1).toInt() : -1},
            {u"text"_s, query.value(2)},
            {u"modifiedText"_s, formatDate(QDateTime::fromSecsSinceEpoch(query.value(3).toLongLong()))},
            {u"x"_s, placed ? query.value(4).toDouble() : -1},
            {u"y"_s, placed ? query.value(5).toDouble() : -1},
        });
    }
    return result;
}

void Library::moveNote(qint64 noteId, double x, double y)
{
    const qint64 documentId = Database::value(u"SELECT doc_id FROM annotations WHERE id = ? AND kind = 'note'"_s, {noteId}).toLongLong();
    if (documentId == 0) {
        return;
    }
    Database::exec(u"UPDATE annotations SET x = ?, y = ? WHERE id = ?"_s, {qBound(0.0, x, 1.0), qBound(0.0, y, 1.0), noteId});
    touchDocument(documentId);
}

qint64 Library::addNote(qint64 documentId, const QString &text, int page, double x, double y)
{
    if (text.trimmed().isEmpty() || !Store::document(documentId).isValid()) {
        return 0;
    }
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    // Stuck to a place only with both a page and a place on it.
    const bool placed = page >= 0 && x >= 0 && y >= 0;
    const qint64 id = Database::exec(u"INSERT INTO annotations (doc_id, page, kind, data, created, modified, x, y) VALUES (?, ?, 'note', ?, ?, ?, ?, ?)"_s,
                                     {documentId, placed ? page : -1, text.trimmed(), now, now, placed ? qBound(0.0, x, 1.0) : -1.0, placed ? qBound(0.0, y, 1.0) : -1.0})
                          .lastInsertId()
                          .toLongLong();
    // Whoever writes a note on a document has dealt with it: it leaves the inbox.
    Database::exec(u"UPDATE documents SET inbox = 0 WHERE inbox = 1 AND id = ?"_s, {documentId});
    Store::refreshNotes(documentId);
    touchDocument(documentId);
    return id;
}

void Library::updateNote(qint64 noteId, const QString &text)
{
    const qint64 documentId = Database::value(u"SELECT doc_id FROM annotations WHERE id = ? AND kind = 'note'"_s, {noteId}).toLongLong();
    if (documentId == 0) {
        return;
    }
    if (text.trimmed().isEmpty()) {
        removeNote(noteId);
        return;
    }
    Database::exec(u"UPDATE annotations SET data = ?, modified = ? WHERE id = ?"_s, {text.trimmed(), QDateTime::currentSecsSinceEpoch(), noteId});
    Store::refreshNotes(documentId);
    touchDocument(documentId);
}

void Library::removeNote(qint64 noteId)
{
    const qint64 documentId = Database::value(u"SELECT doc_id FROM annotations WHERE id = ? AND kind = 'note'"_s, {noteId}).toLongLong();
    if (documentId == 0) {
        return;
    }
    Database::exec(u"DELETE FROM annotations WHERE id = ?"_s, {noteId});
    Store::refreshNotes(documentId);
    touchDocument(documentId);
    dropAbandoned();
}

// ---------------------------------------------------------------------------------------------
// Columns of the list

namespace
{
QString columnsKey(const QString &view)
{
    return "columns:"_L1 + view;
}

/** Forgets the columns of views that are no more: a group, a tag and so on that was deleted. */
void pruneViewColumns()
{
    static const QList<std::pair<QString, QString>> kinds = {
        {u"columns:group:"_s, u"folders"_s},
        {u"columns:tag:"_s, u"tags"_s},
        {u"columns:search:"_s, u"searches"_s},
        {u"columns:rule:"_s, u"rules"_s},
        {u"columns:source:"_s, u"sources"_s},
    };
    for (const auto &[prefix, table] : kinds) {
        // A new group may be given the number of one that is gone; it is not to inherit.
        Database::exec(u"DELETE FROM meta WHERE key LIKE ? AND CAST(substr(key, ?) AS INTEGER) NOT IN (SELECT id FROM %1)"_s.arg(table),
                       {QString(prefix + u'%'), prefix.size() + 1});
    }
}
}

bool Library::hasViewColumns(const QString &view) const
{
    return Database::value(u"SELECT EXISTS (SELECT 1 FROM meta WHERE key = ?)"_s, {columnsKey(view)}).toBool();
}

QStringList Library::viewColumns(const QString &view) const
{
    return Database::value(u"SELECT value FROM meta WHERE key = ?"_s, {columnsKey(view)}).toString().split(u',', Qt::SkipEmptyParts);
}

void Library::setViewColumns(const QString &view, const QStringList &columns)
{
    if (view.isEmpty()) {
        return;
    }
    // No columns at all is a choice too: the row is there, and empty.
    Database::exec(u"INSERT OR REPLACE INTO meta (key, value) VALUES (?, ?)"_s, {columnsKey(view), columns.isEmpty() ? u""_s : columns.join(u',')});
    Q_EMIT viewColumnsChanged();
}

void Library::clearViewColumns(const QString &view)
{
    Database::exec(u"DELETE FROM meta WHERE key = ?"_s, {columnsKey(view)});
    Q_EMIT viewColumnsChanged();
}

void Library::clearAllViewColumns()
{
    Database::exec(u"DELETE FROM meta WHERE key LIKE 'columns:%'"_s);
    Q_EMIT viewColumnsChanged();
}

// ---------------------------------------------------------------------------------------------
// To-dos

namespace
{
QDate todoDay(const QString &due)
{
    return QDate::fromString(due.trimmed(), Qt::ISODate);
}

/** @p warningDays: from how many days before it is due a to-do is pointed out. */
QVariantMap readTodo(const QSqlQuery &query, int warningDays)
{
    // id, text, doc_id, due, done, document's name, mime, kind, trashed
    static const QMimeDatabase mimeDatabase;
    QVariantMap todo{
        {u"id"_s, query.value(0).toLongLong()},
        {u"text"_s, query.value(1).toString()},
        {u"done"_s, query.value(4).toBool()},
        {u"due"_s, QString()},
        {u"dueText"_s, QString()},
        {u"state"_s, QString()},
        {u"documentId"_s, 0},
    };
    const QDate due = todoDay(query.value(3).toString());
    if (due.isValid()) {
        const qint64 days = QDate::currentDate().daysTo(due);
        todo[u"due"_s] = due.toString(Qt::ISODate);
        todo[u"dueText"_s] = days == 0 ? i18nc("@info when a to-do is due", "Today")
            : days == 1                ? i18nc("@info when a to-do is due", "Tomorrow")
            : days == -1               ? i18nc("@info when a to-do was due", "Yesterday")
                                       : QLocale().toString(due, QLocale::ShortFormat);
        if (!todo.value(u"done"_s).toBool()) {
            todo[u"state"_s] = days < 0 ? u"overdue"_s : days <= warningDays ? u"soon"_s : QString();
        }
    }
    // Only a document that is still in the index: the name is none otherwise.
    if (!query.value(5).isNull()) {
        const QString mimeType = query.value(6).toString();
        const QMimeType mime = mimeDatabase.mimeTypeForName(mimeType);
        todo[u"documentId"_s] = query.value(2).toLongLong();
        todo[u"documentName"_s] = query.value(5).toString();
        todo[u"documentIcon"_s] = isContainerMimeType(mimeType) ? u"folder-mail"_s : mime.isValid() ? mime.iconName() : u"text-x-generic"_s;
        todo[u"documentTrashed"_s] = query.value(8).toInt() != 0;
    }
    return todo;
}

const QString TodoColumns = u"t.id, t.text, t.doc_id, t.due, t.done, d.name, d.mime, d.kind, d.trashed"_s;
}

void Library::setTodoWarningDays(int days)
{
    days = qBound(0, days, MaxTodoWarningDays);
    if (days != m_todoWarningDays) {
        m_todoWarningDays = days;
        // The same to-dos, but other ones stand out now.
        Q_EMIT todosChanged();
    }
}

QVariantList Library::todos() const
{
    // What is open before what is done; what has a day before what has none, the nearest day
    // first; and otherwise as they were written down. What is done: the latest first.
    QVariantList result;
    QSqlQuery query = Database::exec(u"SELECT %1 FROM todos t LEFT JOIN documents d ON d.id = t.doc_id AND t.doc_id <> 0 "
                                     "ORDER BY t.done, CASE WHEN t.done = 1 THEN -t.finished ELSE 0 END, t.due = '', t.due, t.created, t.id"_s.arg(TodoColumns));
    while (query.next()) {
        result.append(readTodo(query, m_todoWarningDays));
    }
    return result;
}

QVariantMap Library::todo(qint64 id) const
{
    QSqlQuery query = Database::exec(u"SELECT %1 FROM todos t LEFT JOIN documents d ON d.id = t.doc_id AND t.doc_id <> 0 WHERE t.id = ?"_s.arg(TodoColumns), {id});
    return query.next() ? readTodo(query, m_todoWarningDays) : QVariantMap();
}

QVariantMap Library::todoCounts() const
{
    const QString today = QDate::currentDate().toString(Qt::ISODate);
    const QString horizon = QDate::currentDate().addDays(m_todoWarningDays).toString(Qt::ISODate);
    QSqlQuery query = Database::exec(u"SELECT COALESCE(SUM(done = 0), 0), COALESCE(SUM(done = 0 AND due <> '' AND due < ?), 0), "
                                     "COALESCE(SUM(done = 0 AND due <> '' AND due >= ? AND due <= ?), 0), COALESCE(SUM(done = 1), 0) FROM todos"_s,
                                     {today, today, horizon});
    query.next();
    return {
        {u"open"_s, query.value(0).toInt()},
        {u"overdue"_s, query.value(1).toInt()},
        {u"soon"_s, query.value(2).toInt()},
        {u"done"_s, query.value(3).toInt()},
    };
}

qint64 Library::addTodo(const QString &text, qint64 documentId, const QString &due)
{
    const QString line = text.simplified();
    if (line.isEmpty()) {
        return 0;
    }
    const qint64 about = Store::document(documentId).isValid() ? documentId : 0;
    const QDate day = todoDay(due);
    const qint64 id = Database::exec(u"INSERT INTO todos (text, doc_id, due, created) VALUES (?, ?, ?, ?)"_s,
                                     {line, about, day.isValid() ? day.toString(Qt::ISODate) : u""_s, QDateTime::currentSecsSinceEpoch()})
                          .lastInsertId()
                          .toLongLong();
    Q_EMIT todosChanged();
    return id;
}

void Library::updateTodo(qint64 id, const QString &text, qint64 documentId, const QString &due)
{
    const QString line = text.simplified();
    if (line.isEmpty()) {
        return;
    }
    const qint64 about = Store::document(documentId).isValid() ? documentId : 0;
    const QDate day = todoDay(due);
    Database::exec(u"UPDATE todos SET text = ?, doc_id = ?, due = ? WHERE id = ?"_s, {line, about, day.isValid() ? day.toString(Qt::ISODate) : u""_s, id});
    Q_EMIT todosChanged();
    // The document it was about before may have been kept for it alone.
    dropAbandoned();
}

void Library::setTodoDone(qint64 id, bool done)
{
    Database::exec(u"UPDATE todos SET done = ?, finished = ? WHERE id = ?"_s, {done, done ? QDateTime::currentSecsSinceEpoch() : 0, id});
    Q_EMIT todosChanged();
    if (done) {
        dropAbandoned();
    }
}

void Library::removeTodo(qint64 id)
{
    Database::exec(u"DELETE FROM todos WHERE id = ?"_s, {id});
    Q_EMIT todosChanged();
    dropAbandoned();
}

int Library::removeDoneTodos()
{
    const int removed = int(Database::exec(u"DELETE FROM todos WHERE done = 1"_s).numRowsAffected());
    if (removed > 0) {
        Q_EMIT todosChanged();
    }
    return removed;
}

namespace
{
/** How long after the calendar of to-dos could not be written it is tried again. */
constexpr int CalendarRetryMs = 60 * 1000;
}

bool Library::todoCalendarEnabled() const
{
    return Database::isOpen() && Database::value(u"SELECT value FROM meta WHERE key = 'todo-calendar'"_s).toInt() == 1;
}

QString Library::todoCalendarFile() const
{
    return QDir::cleanPath(Paths::dataDir() + u'/' + TodoCalendar::fileName());
}

void Library::setTodoCalendarEnabled(bool enabled)
{
    if (!Database::isOpen() || enabled == todoCalendarEnabled()) {
        return;
    }
    m_calendarTimer.stop();
    m_calendarProblem.clear();
    m_calendarPath.clear();
    m_calendarWritten.clear();
    if (enabled) {
        Database::exec(u"INSERT OR REPLACE INTO meta (key, value) VALUES ('todo-calendar', '1')"_s);
        // What the to-dos of this database are told by from those of any other; see
        // TodoCalendar::render(). It stays when the calendar is turned off and on again, so
        // that the calendar programs know the to-dos again.
        if (Database::value(u"SELECT value FROM meta WHERE key = 'todo-calendar-id'"_s).toString().isEmpty()) {
            Database::exec(u"INSERT OR REPLACE INTO meta (key, value) VALUES ('todo-calendar-id', ?)"_s, {QUuid::createUuid().toString(QUuid::WithoutBraces)});
        }
        writeTodoCalendar();
    } else {
        Database::exec(u"DELETE FROM meta WHERE key = 'todo-calendar'"_s);
        const QString path = todoCalendarFile();
        m_calendarPool.start([path] {
            TodoCalendar::remove(path);
        });
    }
    Q_EMIT todoCalendarChanged();
}

void Library::setTodoCalendarName(const QString &name)
{
    if (name != m_calendarName) {
        m_calendarName = name;
        scheduleTodoCalendar();
    }
}

void Library::scheduleTodoCalendar()
{
    if (!m_calendarTimer.isActive()) {
        m_calendarTimer.start();
    }
}

void Library::flushTodoCalendar()
{
    if (m_calendarTimer.isActive()) {
        writeTodoCalendar();
    }
    m_calendarPool.waitForDone();
}

void Library::writeTodoCalendar()
{
    m_calendarTimer.stop();
    if (m_databaseLost || !todoCalendarEnabled()) {
        return;
    }
    QList<TodoCalendar::Entry> entries;
    QSqlQuery query = Database::exec(u"SELECT t.id, t.text, t.due, t.created, d.name, d.url FROM todos t "
                                     "LEFT JOIN documents d ON d.id = t.doc_id AND t.doc_id <> 0 WHERE t.done = 0 AND t.due <> ''"_s);
    while (query.next()) {
        TodoCalendar::Entry entry;
        entry.id = query.value(0).toLongLong();
        entry.text = query.value(1).toString();
        entry.due = todoDay(query.value(2).toString());
        entry.created = query.value(3).toLongLong();
        entry.documentName = query.value(4).toString();
        entry.documentUrl = QUrl(query.value(5).toString());
        entries.append(entry);
    }
    const QString id = Database::value(u"SELECT value FROM meta WHERE key = 'todo-calendar-id'"_s).toString();
    const QByteArray calendar = TodoCalendar::render(entries, m_calendarName, id);
    const QString path = todoCalendarFile();
    if (path == m_calendarPath && calendar == m_calendarWritten) {
        return;
    }
    m_calendarPath = path;
    m_calendarWritten = calendar;
    m_calendarPool.start([this, path, calendar] {
        const QString problem = TodoCalendar::write(path, calendar);
        QMetaObject::invokeMethod(
            this,
            [this, path, problem] {
                if (path != m_calendarPath) {
                    return; // another database by now
                }
                if (!problem.isEmpty()) {
                    qCWarning(INDEKS_CORE) << problem;
                    // Tried again in a while, and before that if the to-dos change: not
                    // with everything that happens to a document in the meantime.
                    QTimer::singleShot(CalendarRetryMs, this, [this, path] {
                        if (path == m_calendarPath) {
                            m_calendarWritten.clear();
                            scheduleTodoCalendar();
                        }
                    });
                }
                if (problem != m_calendarProblem) {
                    m_calendarProblem = problem;
                    Q_EMIT todoCalendarChanged();
                }
            },
            Qt::QueuedConnection);
    });
}

QString Library::ink(qint64 documentId, int page) const
{
    return Database::value(u"SELECT data FROM annotations WHERE doc_id = ? AND page = ? AND kind = 'ink'"_s, {documentId, page}).toString();
}

void Library::setInk(qint64 documentId, int page, const QString &json)
{
    if (!Store::document(documentId).isValid()) {
        return;
    }
    const bool empty = json.trimmed().isEmpty() || json.trimmed() == "[]"_L1;
    const qint64 existing = Database::value(u"SELECT id FROM annotations WHERE doc_id = ? AND page = ? AND kind = 'ink'"_s, {documentId, page}).toLongLong();
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (empty) {
        if (existing != 0) {
            Database::exec(u"DELETE FROM annotations WHERE id = ?"_s, {existing});
            touchDocument(documentId);
            dropAbandoned();
            return;
        }
    } else if (existing != 0) {
        Database::exec(u"UPDATE annotations SET data = ?, modified = ? WHERE id = ?"_s, {json, now, existing});
    } else {
        // A drawing refers to exact positions in the file, so pin down the file it was made on:
        // from the first stroke on the document lives on as a copy in Indeks' data folder.
        ensureLocalCopy(documentId);
        Database::exec(u"INSERT INTO annotations (doc_id, page, kind, data, created, modified) VALUES (?, ?, 'ink', ?, ?, ?)"_s, {documentId, page, json, now, now});
    }
    touchDocument(documentId);
}

bool Library::hasInk(qint64 documentId) const
{
    return Database::value(u"SELECT EXISTS (SELECT 1 FROM annotations WHERE doc_id = ? AND kind = 'ink')"_s, {documentId}).toBool();
}

void Library::clearInk(qint64 documentId)
{
    Database::exec(u"DELETE FROM annotations WHERE doc_id = ? AND kind = 'ink'"_s, {documentId});
    touchDocument(documentId);
    dropAbandoned();
}

// ---------------------------------------------------------------------------------------------
// E-mail

QVariantMap Library::email(qint64 documentId, int message) const
{
    const QString path = contentPath(documentId);
    if (path.isEmpty()) {
        return {{u"valid"_s, false}};
    }
    const QList<Email> emails = readEmails(path, message + 1);
    if (message < 0 || message >= emails.size()) {
        return {{u"valid"_s, false}};
    }
    const Email &mail = emails.at(message);
    static const QMimeDatabase mimeDatabase;
    QVariantList attachments;
    for (int i = 0; i < mail.attachments.size(); ++i) {
        const EmailAttachment &attachment = mail.attachments.at(i);
        const QMimeType mime = mimeDatabase.mimeTypeForFile(attachment.name, QMimeDatabase::MatchExtension);
        attachments.append(QVariantMap{
            {u"index"_s, i},
            {u"name"_s, attachment.name},
            {u"sizeText"_s, KFormat().formatByteSize(double(attachment.size))},
            {u"icon"_s, mime.isValid() ? mime.iconName() : u"mail-attachment"_s},
        });
    }
    return {
        {u"valid"_s, true},
        {u"subject"_s, mail.subject},
        {u"from"_s, mail.from},
        {u"to"_s, mail.to},
        {u"cc"_s, mail.cc},
        {u"bcc"_s, mail.bcc},
        {u"date"_s, formatDate(mail.date)},
        {u"text"_s, mail.textBody},
        {u"html"_s, mail.htmlBody.isEmpty() ? QString() : sanitizeHtml(mail.htmlBody)},
        {u"attachments"_s, attachments},
    };
}

QUrl Library::extractAttachment(qint64 documentId, int message, int index)
{
    const QString path = contentPath(documentId);
    if (path.isEmpty()) {
        return {};
    }
    const QList<Email> emails = readEmails(path, message + 1);
    if (message < 0 || message >= emails.size() || index < 0 || index >= emails.at(message).attachments.size()) {
        return {};
    }
    const QByteArray data = readEmailAttachment(path, message, index);
    const QString dir = u"%1/attachments/%2/%3-%4"_s.arg(Paths::cacheDir()).arg(documentId).arg(message).arg(index);
    QDir().mkpath(dir);
    // The name comes from the mail and is not trustworthy: keep only its last component.
    const QString name = cleanName(QFileInfo(emails.at(message).attachments.at(index).name).fileName());
    const QString target = dir + u'/' + (name.isEmpty() ? u"attachment"_s : name);
    QSaveFile file(target);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) < 0 || !file.commit()) {
        Q_EMIT this->message(i18nc("@info", "Cannot save the attachment: %1", file.errorString()));
        return {};
    }
    return QUrl::fromLocalFile(target);
}

qint64 Library::importAttachment(qint64 documentId, int message, int index)
{
    const QUrl extracted = extractAttachment(documentId, message, index);
    if (!extracted.isValid()) {
        return 0;
    }
    const QString target = Paths::uniqueFilePath(Paths::filesDir(), extracted.fileName());
    if (!QFile::copy(extracted.toLocalFile(), target)) {
        Q_EMIT this->message(i18nc("@info", "Cannot store the attachment in Indeks."));
        return 0;
    }
    return adoptOwnedFile(target);
}

// ---------------------------------------------------------------------------------------------
// Obsidian canvas

namespace
{
constexpr qint64 MaxCardTextBytes = 32 * 1024;
constexpr qreal ArrowLength = 16.0;
constexpr qreal ArrowHalfWidth = 7.0;

QPointF sideAnchor(const QRectF &rect, const QString &side)
{
    if (side == "top"_L1) {
        return QPointF(rect.center().x(), rect.top());
    }
    if (side == "bottom"_L1) {
        return QPointF(rect.center().x(), rect.bottom());
    }
    if (side == "left"_L1) {
        return QPointF(rect.left(), rect.center().y());
    }
    return QPointF(rect.right(), rect.center().y());
}

QPointF sideNormal(const QString &side)
{
    if (side == "top"_L1) {
        return QPointF(0, -1);
    }
    if (side == "bottom"_L1) {
        return QPointF(0, 1);
    }
    if (side == "left"_L1) {
        return QPointF(-1, 0);
    }
    return QPointF(1, 0);
}

/** The side of @p from that faces @p to, for connections that do not name one. */
QString facingSide(const QRectF &from, const QRectF &to)
{
    const QPointF delta = to.center() - from.center();
    if (qAbs(delta.x()) >= qAbs(delta.y())) {
        return delta.x() >= 0 ? u"right"_s : u"left"_s;
    }
    return delta.y() >= 0 ? u"bottom"_s : u"top"_s;
}

/** Triangle with its tip at @p tip, for a line arriving from @p from. */
QVariantList arrowHead(const QPointF &tip, const QPointF &from)
{
    QLineF line(tip, from);
    if (line.length() < 0.01) {
        return {};
    }
    line.setLength(ArrowLength);
    const QPointF base = line.p2();
    const QLineF normal = QLineF(base, tip).normalVector();
    QLineF wing(base, normal.p2());
    wing.setLength(ArrowHalfWidth);
    const QPointF offset = wing.p2() - base;
    return {tip, base + offset, base - offset, tip};
}
}

QVariantMap Library::canvas(qint64 documentId) const
{
    const Document doc = Store::document(documentId);
    const QString path = doc.isValid() ? Store::contentPath(doc) : QString();
    const Canvas canvas = path.isEmpty() ? Canvas() : readCanvas(path);
    if (!canvas.valid) {
        return {{u"valid"_s, false}};
    }
    // File cards are looked up next to the original; a working copy or download has no vault around it.
    const QString canvasDirectory = doc.url.isLocalFile() ? QFileInfo(doc.url.toLocalFile()).path() : QString();
    const QPointF origin = canvas.bounds.topLeft();
    static const QMimeDatabase mimeDatabase;

    QHash<QString, QRectF> rects;
    QVariantList nodes;
    for (const CanvasNode &node : canvas.nodes) {
        const QRectF rect = node.rect.translated(-origin);
        rects.insert(node.id, rect);
        QVariantMap entry{
            {u"x"_s, rect.x()},
            {u"y"_s, rect.y()},
            {u"width"_s, rect.width()},
            {u"height"_s, rect.height()},
            {u"color"_s, node.color},
            {u"type"_s, u"text"_s},
            {u"text"_s, node.text},
        };
        switch (node.type) {
        case CanvasNode::Text:
            break;
        case CanvasNode::Group:
            entry[u"type"_s] = u"group"_s;
            entry[u"text"_s] = node.label;
            break;
        case CanvasNode::Link:
            entry[u"type"_s] = u"link"_s;
            entry[u"text"_s] = node.url;
            entry[u"url"_s] = QUrl::fromUserInput(node.url);
            break;
        case CanvasNode::File: {
            entry[u"type"_s] = u"file"_s;
            entry[u"text"_s] = QFileInfo(node.file).fileName();
            entry[u"fileKind"_s] = u"missing"_s;
            entry[u"icon"_s] = u"unknown"_s;
            const QString resolved = resolveCanvasFile(canvasDirectory, node.file);
            if (resolved.isEmpty()) {
                break;
            }
            const QMimeType mime = mimeDatabase.mimeTypeForFile(resolved);
            const Kind kind = kindForFile(resolved, mime.name());
            const QUrl fileUrl = QUrl::fromLocalFile(resolved);
            entry[u"icon"_s] = mime.iconName();
            entry[u"url"_s] = fileUrl;
            entry[u"documentId"_s] = Store::documentByUrl(fileUrl).id;
            if (kind == Kind::Image) {
                entry[u"fileKind"_s] = u"image"_s;
            } else if (kind == Kind::PlainText) {
                entry[u"fileKind"_s] = u"text"_s;
                entry[u"markdown"_s] = mime.name() == "text/markdown"_L1;
                entry[u"fileText"_s] = readTextFile(resolved, MaxCardTextBytes);
            } else {
                entry[u"fileKind"_s] = u"other"_s;
            }
            break;
        }
        }
        nodes.append(entry);
    }

    QVariantList edges;
    for (const CanvasEdge &edge : canvas.edges) {
        const auto from = rects.constFind(edge.fromNode);
        const auto to = rects.constFind(edge.toNode);
        if (from == rects.constEnd() || to == rects.constEnd()) {
            continue;
        }
        const QString fromSide = edge.fromSide.isEmpty() ? facingSide(*from, *to) : edge.fromSide;
        const QString toSide = edge.toSide.isEmpty() ? facingSide(*to, *from) : edge.toSide;
        const QPointF start = sideAnchor(*from, fromSide);
        const QPointF end = sideAnchor(*to, toSide);
        // The curve leaves and arrives at a right angle to the cards, the further apart the wider.
        const qreal reach = qBound(40.0, QLineF(start, end).length() * 0.4, 320.0);
        const QPointF control1 = start + sideNormal(fromSide) * reach;
        const QPointF control2 = end + sideNormal(toSide) * reach;
        const QPointF middle = 0.125 * start + 0.375 * control1 + 0.375 * control2 + 0.125 * end;
        edges.append(QVariantMap{
            {u"start"_s, start},
            {u"end"_s, end},
            {u"control1"_s, control1},
            {u"control2"_s, control2},
            {u"middle"_s, middle},
            {u"startArrow"_s, edge.arrowAtStart ? arrowHead(start, control1) : QVariantList()},
            {u"endArrow"_s, edge.arrowAtEnd ? arrowHead(end, control2) : QVariantList()},
            {u"color"_s, edge.color},
            {u"label"_s, edge.label},
        });
    }

    return {
        {u"valid"_s, true},
        {u"width"_s, canvas.bounds.width()},
        {u"height"_s, canvas.bounds.height()},
        {u"nodes"_s, nodes},
        {u"edges"_s, edges},
    };
}

}

#include "moc_library.cpp"
