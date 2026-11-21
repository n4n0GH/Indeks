// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "types.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QThreadPool>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <atomic>

class KDirWatch;

namespace Indeks
{
class Indexer;
class RemoteScanner;
class ScriptHost;

/**
 * The document library: everything the user interface does goes through here.
 *
 * Indeks never changes the files it indexes. Documents only point at them; tags, groups, notes
 * and drawings live in the index. Editing or drawing on a document first copies the file into
 * Indeks' own data directory and continues on that copy.
 *
 * Lists of document ids are passed as QVariantList so that the methods can be called from QML.
 */
class Library : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by App")
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString statusDetail READ statusDetail NOTIFY statusChanged)
    Q_PROPERTY(double progress READ progress NOTIFY statusChanged)
    Q_PROPERTY(bool ocrAvailable READ ocrAvailable CONSTANT)
    /** Folder of the database that is open. */
    Q_PROPERTY(QString databaseDir READ databaseDir NOTIFY databaseChanged)
    /** Whether Office documents can be shown as they look, which takes LibreOffice. */
    Q_PROPERTY(bool officeViewAvailable READ officeViewAvailable CONSTANT)
    Q_PROPERTY(QStringList ocrLanguages READ ocrLanguages CONSTANT)
    /** How many pages are read for text at once when the setting leaves it to Indeks. */
    Q_PROPERTY(int ocrDefaultJobs READ ocrDefaultJobs CONSTANT)
    /** Processor cores of this computer: the most that is worth setting. */
    Q_PROPERTY(int processorCount READ processorCount CONSTANT)
    /** How many files are read at once when the setting leaves it to Indeks. */
    Q_PROPERTY(int indexDefaultJobs READ indexDefaultJobs CONSTANT)
    /** Whether a script is running or waiting to, by itself or for a rule. */
    Q_PROPERTY(bool scriptsRunning READ scriptsRunning NOTIFY scriptsRunningChanged)
public:
    explicit Library(QObject *parent = nullptr);
    ~Library() override;

    void setOptions(const IndexOptions &options);
    IndexOptions options() const
    {
        return m_options;
    }
    /** Starts watching the indexed locations and, if @p rescan, brings the index up to date. */
    void start(bool rescan);
    Indexer *indexer() const
    {
        return m_indexer;
    }

    bool isBusy() const;
    QString status() const;
    /** The file or location the status is about, if it is about one. */
    QString statusDetail() const;
    /** Share of the running work that is done, 0 to 1; negative while that cannot be told. */
    double progress() const;
    bool ocrAvailable() const;
    QString databaseDir() const;

    /**
     * What stands in the way of opening the database in @p dir, in words for the user; empty
     * if nothing does. With @p create, a folder without a database counts as fine: one will be
     * started there.
     */
    static QString databaseProblem(const QString &dir, bool create);

    /**
     * Closes the database that is open and opens the one in @p dir (empty: the default one)
     * instead. Indexing is called off first. If the other database cannot be opened, the
     * current one stays open and the reason is returned; otherwise the result is empty.
     */
    QString switchDatabase(const QString &dir, bool create = false);
    bool officeViewAvailable() const;
    QStringList ocrLanguages() const;
    int ocrDefaultJobs() const;
    int processorCount() const;
    int indexDefaultJobs() const;

    // Indexed locations
    Q_INVOKABLE QVariantList sources() const;
    Q_INVOKABLE qint64 addSource(const QUrl &url, const QString &name = QString(), bool recursive = true);
    Q_INVOKABLE void removeSource(qint64 id);
    Q_INVOKABLE void rescanSource(qint64 id);
    /** Whether files that turn up in an indexed location later on are put into the inbox. */
    Q_INVOKABLE bool sourceUsesInbox(qint64 id) const;
    Q_INVOKABLE void setSourceUsesInbox(qint64 id, bool inbox);
    /** What the indexer is working on, followed by what waits; see QueueItem. */
    QList<QueueItem> activity() const;
    /** How many waiting documents activity() leaves out. */
    int activityOverflow() const;

    /**
     * Calls off indexing and transfers before the program ends. Returns false if the indexer
     * is still inside a file after @p timeoutMs; the index is safe to abandon even then, since
     * everything is written in transactions.
     */
    bool shutdown(int timeoutMs);

    Q_INVOKABLE void rescanAll();
    bool isSourceAvailable(qint64 id) const;

    // Adding documents
    /** Files go to the inbox, folders become indexed locations. */
    Q_INVOKABLE void addUrls(const QList<QUrl> &urls);
    /** Creates a document in Indeks' own directory. @p format is "text", "markdown" or "html". */
    Q_INVOKABLE qint64 createDocument(const QString &name, const QString &format, const QString &content = QString());

    // Documents
    Q_INVOKABLE QVariantMap documentInfo(qint64 id) const;
    Q_INVOKABLE int count(const QString &query) const;
    Q_INVOKABLE QString contentPath(qint64 id) const;
    /** Makes the content available locally; answers with contentReady() or contentFailed(). */
    Q_INVOKABLE void requestContent(qint64 id);
    /**
     * Asks for a PDF rendition of an Office document whose content is at hand (see
     * requestContent()). Answered with renditionReady() or renditionFailed(). Of several
     * requests made while one is being worked on, only the newest is kept: the others were
     * for documents the user has moved on from.
     */
    Q_INVOKABLE void requestRendition(qint64 id);
    /** Path of a file that may be modified: the document itself if Indeks owns it, else a copy. */
    Q_INVOKABLE QString ensureLocalCopy(qint64 id);
    Q_INVOKABLE void discardLocalCopy(qint64 id);
    Q_INVOKABLE QString readText(qint64 id) const;
    /**
     * The text of a JSON Lines document, or of JSON written without line breaks, laid out for
     * reading: indented, one member per line. Never what gets saved. The map is empty for
     * anything else; otherwise it holds
     *   text         the formatted text
     *   recordLines  lines of the text (from 0) at which a record begins
     *   sourceLines  the line of the file (from 1) each of these records is
     *   shown, total how many records the text holds and the file has; the total is -1
     *                for a file too large to count on the spot (see countRecords())
     *   more         whether the file has records the text does not hold
     *   lines        whether the file is JSON Lines (else: one JSON value)
     * At most @p maxRecords records are formatted (0: as many as the preview takes).
     */
    Q_INVOKABLE QVariantMap formattedText(qint64 id, int maxRecords) const;
    /**
     * Counts the records of a JSON Lines file in the background and reports with
     * recordsCounted(). For files too large to count while the user waits, for which
     * formattedText() gives a total of -1.
     */
    Q_INVOKABLE void countRecords(qint64 id);
    /** Text documents larger than this are shown in part, and cannot be edited for that reason. */
    Q_INVOKABLE qint64 maxTextSize() const;
    Q_INVOKABLE QString indexedText(qint64 id) const;
    Q_INVOKABLE bool saveText(qint64 id, const QString &text);
    /** Tells the library that the editable file of @p id was written by someone else. */
    Q_INVOKABLE void contentSaved(qint64 id);
    Q_INVOKABLE bool renameDocument(qint64 id, const QString &name);
    Q_INVOKABLE void setComment(qint64 id, const QString &comment);
    /** Pins documents or takes the pin off; "flagged" is what pinned was called once. */
    Q_INVOKABLE void setFlagged(const QVariantList &ids, bool flagged);
    Q_INVOKABLE void setInbox(const QVariantList &ids, bool inbox);
    Q_INVOKABLE void moveToTrash(const QVariantList &ids);
    Q_INVOKABLE void restoreFromTrash(const QVariantList &ids);
    Q_INVOKABLE void emptyTrash();
    Q_INVOKABLE void reindex(const QVariantList &ids);
    Q_INVOKABLE void requestOcr(const QVariantList &ids);

    // Tags
    Q_INVOKABLE QVariantList tags() const;
    Q_INVOKABLE QStringList tagNames() const;
    Q_INVOKABLE void addTag(const QVariantList &ids, const QString &name);
    Q_INVOKABLE void removeTag(const QVariantList &ids, const QString &name);
    Q_INVOKABLE qint64 createTag(const QString &name);
    Q_INVOKABLE bool renameTag(qint64 id, const QString &name);
    Q_INVOKABLE void deleteTag(qint64 id);

    // Groups
    /** All groups in tree order, each with its depth. */
    Q_INVOKABLE QVariantList groups() const;
    Q_INVOKABLE qint64 createGroup(const QString &name, qint64 parentId = 0);
    Q_INVOKABLE void renameGroup(qint64 id, const QString &name);
    Q_INVOKABLE void deleteGroup(qint64 id);
    Q_INVOKABLE bool moveGroup(qint64 id, qint64 newParentId);
    Q_INVOKABLE void addToGroup(const QVariantList &ids, qint64 groupId);
    Q_INVOKABLE void removeFromGroup(const QVariantList &ids, qint64 groupId);

    // Saved searches
    Q_INVOKABLE QVariantList savedSearches() const;
    /** Creates the search if @p id is 0. */
    Q_INVOKABLE qint64 saveSearch(qint64 id, const QString &name, const QString &query);
    Q_INVOKABLE void deleteSearch(qint64 id);

    // Rules
    Q_INVOKABLE QVariantList rules() const;
    Q_INVOKABLE QVariantMap rule(qint64 id) const;
    /**
     * Creates the rule if the map has no id. Keys: name, query (the condition, in the query
     * language), enabled, onImport, and two lists:
     *   filters  the condition as it was put together, kept for the editor as it is
     *   actions  what to do with a document that matches, in order; maps with `type`
     *            tag, untag (and `name`), group, ungroup (and `id`), flag, unflag,
     *            inbox (take out of the inbox), or script (and `name`, the script's file)
     */
    Q_INVOKABLE qint64 saveRule(const QVariantMap &rule);
    Q_INVOKABLE void deleteRule(qint64 id);
    /**
     * Runs a rule on all documents it matches and returns how many that were. A rule that
     * runs scripts takes its time: it is started, -1 is returned, and message() tells when it
     * is through.
     */
    Q_INVOKABLE int applyRule(qint64 id);

    // Scripts: Python files kept in the database folder, run in a sandbox; see ScriptHost.
    /** Why scripts cannot run on this computer; empty if they can. */
    Q_INVOKABLE QString scriptsProblem() const;
    bool scriptsRunning() const;
    /** Folder the scripts of the open database are in. */
    Q_INVOKABLE QString scriptsDir() const;
    /**
     * The scripts of the open database: maps with `file` (what rules know a script by),
     * `name`, `description`, `modifiedText`, `allowed` (may run on this computer; see
     * Scripts) and `rules`, the names of the rules that run it.
     */
    Q_INVOKABLE QVariantList scripts() const;
    /** One script, with its text as `source`. Empty if there is none of that name. */
    Q_INVOKABLE QVariantMap script(const QString &file) const;
    /** What a new script starts out as. */
    Q_INVOKABLE QString newScriptText() const;
    /**
     * Saves a script and returns the name of its file; empty, after a message(), if that did
     * not work. An empty @p file makes a new script called @p name. A @p name other than the
     * file's renames the script, in the rules that run it as well.
     */
    Q_INVOKABLE QString saveScript(const QString &file, const QString &name, const QString &source);
    /** Moves the script's file to the trash. Rules that ran it skip it from then on. */
    Q_INVOKABLE bool deleteScript(const QString &file);
    /** Lets a script that came from elsewhere run on this computer, as it is now. */
    Q_INVOKABLE void allowScript(const QString &file);
    /** Runs a script on documents, in the background; scriptFinished() tells how it went. */
    Q_INVOKABLE void runScript(const QString &file, const QVariantList &ids);
    /**
     * Runs @p source on documents without doing what it asks for: for trying out what is in
     * the editor. Answered with scriptTried().
     */
    Q_INVOKABLE void tryScript(const QString &name, const QString &source, const QVariantList &ids);
    Q_INVOKABLE void stopScripts();
    /** How the latest runs went, newest first; see ScriptHost::reported() for what is in them. */
    Q_INVOKABLE QVariantList scriptReports() const;
    Q_INVOKABLE void clearScriptReports();

    // Annotations
    /**
     * The notes of a document: maps with `id`, `text`, `modifiedText`, and for a note that is
     * stuck to a place, like a sticky note, the `page` (from 0) and where on it, `x` and `y`,
     * as shares of the page's width and height. A note that belongs to the document as a
     * whole has a page of -1.
     */
    Q_INVOKABLE QVariantList notes(qint64 documentId) const;
    Q_INVOKABLE qint64 addNote(qint64 documentId, const QString &text, int page = -1, double x = -1, double y = -1);
    /** Puts a sticky note somewhere else on its page. */
    Q_INVOKABLE void moveNote(qint64 noteId, double x, double y);
    Q_INVOKABLE void updateNote(qint64 noteId, const QString &text);
    Q_INVOKABLE void removeNote(qint64 noteId);
    // Columns of the list -------------------------------------------------------------------
    /**
     * Which columns the list of documents shows is the user's choice for each view: a place
     * of the sidebar, a group, a tag, a saved search and so on, named by its key there
     * ("place:inbox", "group:12"). A view the user has not chosen columns for has none of
     * its own and shows the standard ones. Kept in the index, as the groups and tags are.
     */
    Q_INVOKABLE bool hasViewColumns(const QString &view) const;
    Q_INVOKABLE QStringList viewColumns(const QString &view) const;
    Q_INVOKABLE void setViewColumns(const QString &view, const QStringList &columns);
    /** Has a view show the standard columns again. */
    Q_INVOKABLE void clearViewColumns(const QString &view);
    /** Has every view show the standard columns again. */
    Q_INVOKABLE void clearAllViewColumns();

    // To-dos -----------------------------------------------------------------------------------
    /**
     * The user's list of things to do, those that are still open first and of them the ones
     * that are due soonest. Each has: id, text, done, due (2026-11-30, or empty), dueText,
     * state ("overdue", "soon" or empty), and, if it is about a document, documentId,
     * documentName, documentIcon and documentTrashed.
     */
    Q_INVOKABLE QVariantList todos() const;
    /**
     * From how many days before its day a to-do counts as due soon. With none, only what is
     * due today does.
     */
    static constexpr int MaxTodoWarningDays = 365;
    int todoWarningDays() const
    {
        return m_todoWarningDays;
    }
    void setTodoWarningDays(int days);
    /** How many are "open", how many of those are due "soon" or "overdue", and how many "done". */
    Q_INVOKABLE QVariantMap todoCounts() const;
    Q_INVOKABLE QVariantMap todo(qint64 id) const;
    /** @p due as 2026-11-30, or empty for something that has no day. */
    Q_INVOKABLE qint64 addTodo(const QString &text, qint64 documentId = 0, const QString &due = QString());
    Q_INVOKABLE void updateTodo(qint64 id, const QString &text, qint64 documentId, const QString &due);
    Q_INVOKABLE void setTodoDone(qint64 id, bool done);
    Q_INVOKABLE void removeTodo(qint64 id);
    /** Takes what is done off the list; returns how many that were. */
    Q_INVOKABLE int removeDoneTodos();

    /** Drawing on @p page as JSON; page 0 for single-page documents. */
    Q_INVOKABLE QString ink(qint64 documentId, int page) const;
    Q_INVOKABLE void setInk(qint64 documentId, int page, const QString &json);
    Q_INVOKABLE bool hasInk(qint64 documentId) const;
    Q_INVOKABLE void clearInk(qint64 documentId);

    // E-mail
    Q_INVOKABLE QVariantMap email(qint64 documentId, int message = 0) const;
    /** Writes an attachment into the cache and returns where, for opening it. */
    Q_INVOKABLE QUrl extractAttachment(qint64 documentId, int message, int index);
    /** Stores an attachment as a new document in the inbox. */
    Q_INVOKABLE qint64 importAttachment(qint64 documentId, int message, int index);

    // Obsidian canvas
    /**
     * A canvas laid out for drawing: `width` and `height` of the board, `nodes` with their
     * rectangles and content, `edges` with the points of their curves and arrow heads. All
     * coordinates are relative to the board's top left corner.
     */
    Q_INVOKABLE QVariantMap canvas(qint64 documentId) const;

Q_SIGNALS:
    /** Something about documents, tags, groups, searches or rules changed. Coalesced. */
    void changed();
    /**
     * Another database is about to be opened. Whoever reads the index from a thread of its
     * own has to close that thread's connection now, before the signal returns.
     */
    void databaseAboutToChange();
    /** Another database was opened: everything known about documents is to be read anew. */
    void databaseChanged();
    /** The folder of the open database can no longer be reached. */
    void databaseLost();
    void documentChanged(qint64 id);
    void documentCreated(qint64 id);
    void sourcesChanged();
    void todosChanged();
    void viewColumnsChanged();
    void busyChanged();
    void statusChanged();
    void activityChanged();
    void contentReady(qint64 id, const QString &path);
    void contentFailed(qint64 id, const QString &message);
    /** Answers to requestRendition(). */
    void renditionReady(qint64 id, const QUrl &pdf);
    void recordsCounted(qint64 id, int total);
    void renditionFailed(qint64 id, const QString &message);
    /** A short text for a passive notification. */
    void message(const QString &text);
    /** Scripts were added, changed, removed or allowed. */
    void scriptsChanged();
    void scriptsRunningChanged();
    /** A script was run on documents; see ScriptHost::reported(). */
    void scriptFinished(const QVariantMap &report);
    /** Answer to tryScript(). */
    void scriptTried(const QVariantMap &report);

private:
    void attachDatabase();
    /**
     * Removes documents that were kept only for the user's work on them, now that the work is
     * gone. To be called after anything that takes notes, drawings, tags and the like away.
     */
    void dropAbandoned();
    /** Has the indexer reclaim space in the index file if enough of it is unused. */
    void tidyIfDue();
    void touch();
    void touchDocument(qint64 id);
    void watchSource(const Source &source);
    void unwatchSource(const Source &source);
    void onPathChanged(const QString &path, bool created);
    void dispatchPendingDirectories();
    void checkAvailability();
    void applyImportRules(const QList<qint64> &ids);
    int applyRuleTo(qint64 ruleId, const QList<qint64> &restrictTo, bool *took);
    void watchScripts();
    qint64 adoptOwnedFile(const QString &path);

    Indexer *m_indexer = nullptr;
    RemoteScanner *m_remote = nullptr;
    ScriptHost *m_scripts = nullptr;
    KDirWatch *m_watch = nullptr;
    KDirWatch *m_scriptWatch = nullptr;
    QString m_watchedScripts; ///< the folder of scripts that is being watched
    QTimer m_scriptTimer;
    QVariantList m_scriptReports;
    QHash<quint64, QString> m_ruleRuns; ///< rules applied by hand that are still at it, by request
    IndexOptions m_options;
    int m_todoWarningDays = 3;
    QTimer m_changeTimer;
    QTimer m_watchTimer;
    QTimer m_availabilityTimer;
    QTimer m_remoteTimer;
    QHash<QString, bool> m_pendingDirectories; ///< directory to "scan recursively"
    QSet<qint64> m_unavailableSources;
    QSet<qint64> m_watchedSources;
    QSet<qint64> m_fetching;

    // Renditions of Office documents, made one at a time away from the user interface.
    struct RenditionRequest {
        qint64 id = 0;
        QString path;
    };
    void startRendition(const RenditionRequest &request);
    QThreadPool m_renditionPool;
    /** Calls off renditions and counting alike. */
    std::atomic_bool m_renditionAbort = false;
    // Counting the records of large JSON Lines files, likewise.
    QThreadPool m_countPool;
    std::atomic<quint64> m_countTicket = 0;
    bool m_renditionRunning = false;
    RenditionRequest m_renditionWaiting; ///< the newest request while another one is being made
    bool m_started = false;
    bool m_rescanOnStart = false;
    bool m_databaseLost = false;
};

/** Converts ids coming from QML. */
QList<qint64> toIdList(const QVariantList &ids);

}
