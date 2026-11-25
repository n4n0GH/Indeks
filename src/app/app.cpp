// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "app.h"

#include "databasechoice.h"
#include "databasefolder.h"
#include "indekssettings.h"
#include "office.h"
#include "paths.h"
#include "store.h"
#include "weblink.h"
#include "welcome.h"

#include <KApplicationTrader>
#include <KIO/ApplicationLauncherJob>
#include <KIconLoader>
#include <KIO/JobUiDelegateFactory>
#include <KIO/OpenFileManagerWindowJob>
#include <KIO/OpenUrlJob>
#include <KJobWindows>
#include <KLocalizedString>
#include <KService>
#include <KStandardActions>

#include <QAction>
#include <QBuffer>
#include <QClipboard>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QJSEngine>
#include <QMimeData>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QTextDocumentFragment>
#include <QTimer>
#include <QWindow>

using namespace Indeks;
using namespace Qt::StringLiterals;

static App *s_instance = nullptr;

namespace
{
// Why the database the user works with could not be opened at start, to be told once the
// window is there.
Q_GLOBAL_STATIC(QString, s_startupProblem)

/** Picks the database to open, before the library opens anything. */
Library *createLibrary(QObject *parent)
{
    const QString wanted = Config::self()->activeDatabase();
    if (!wanted.isEmpty()) {
        // A database is never started just because its folder is without one: the folder may
        // be what is left of a share that is not mounted. The one exception is the folder a
        // new installation was told to start in a moment ago.
        const QString problem = Library::databaseProblem(wanted, wanted == Welcome::folderToStartIn());
        if (problem.isEmpty()) {
            Paths::setDatabaseDir(wanted);
        } else {
            // The setting stays: the share may well be back the next time.
            *s_startupProblem() = problem;
        }
    }
    return new Library(parent);
}

// What the actions that run one script each are called, followed by the script's file name.
const QLatin1StringView scriptActionPrefix("run_script_");
// Actions that have their fixed place in the bar above the preview, where what they do is
// seen, and are not offered for the toolbar: editing or drawing on the document, and noting.
const QStringList previewBarActions = {QStringLiteral("doc_edit"), QStringLiteral("doc_note")};
// Actions Indeks no longer has, which a toolbar that was put together earlier may still
// name. "Add Files…" went when single files stopped being taken: see Library::addFolders().
const QStringList formerActions = {QStringLiteral("add_files")};
// What stood at fixed places at the far end of the toolbar before it could be chosen, in
// that order, the flexible space that keeps it at that end first.
const QStringList formerlyFixedInToolbar = {QStringLiteral("spacer"),       QStringLiteral("search_field"), QStringLiteral("search_advanced"),
                                            QStringLiteral("save_search"),  QStringLiteral("menu_preview")};
// The two buttons for the preview, which one button with a menu stands for now.
const QStringList formerPreviewButtons = {QStringLiteral("view_preview"), QStringLiteral("view_preview_beside")};
// What is done to the table in the preview, to the column of the cell that is chosen there.
constexpr QLatin1StringView tableActions[] = {
    "table_sort_az"_L1, "table_sort_za"_L1, "table_sort_smallest"_L1, "table_sort_largest"_L1, "table_unsort"_L1, "table_hide_column"_L1, "table_show_columns"_L1,
};
// A script that keeps failing for a rule is not complained about more often than this.
constexpr qint64 ScriptComplaintIntervalSecs = 60;

QString cleanedPath(const QString &path)
{
    return path.isEmpty() ? QString() : QDir::cleanPath(path);
}
}

App::App(QObject *parent)
    : AbstractKirigamiApplication(parent)
    , m_library(createLibrary(this))
    , m_documents(new DocumentModel(m_library, this))
    , m_linkCandidates(new DocumentModel(m_library, this))
    , m_sidebar(new SidebarModel(m_library, this))
    , m_activity(new ActivityModel(m_library, this))
    , m_panels(new PanelLayout(this))
{
    s_instance = this;

    Config *config = Config::self();
    // Editing and taking notes have their place in the bar above the preview now, and some
    // actions are gone. A toolbar that was put together before still names them: they are
    // taken out of it, and with them the dividing lines that would stand side by side or at
    // an end for it. Of several flexible spaces only the first does anything.
    {
        QStringList toolbar;
        const QStringList chosen = config->toolbarActions();
        for (const QString &name : chosen) {
            if (previewBarActions.contains(name) || formerActions.contains(name) || (name == "separator"_L1 && (toolbar.isEmpty() || toolbar.constLast() == "separator"_L1))
                || (name == "spacer"_L1 && toolbar.contains(name))) {
                continue;
            }
            toolbar.append(name);
        }
        while (!toolbar.isEmpty() && toolbar.constLast() == "separator"_L1) {
            toolbar.removeLast();
        }
        // The search field and the buttons beside it had fixed places once and are chosen
        // like the others now. A toolbar from before does not name them; it gets them where
        // they were, once. Without one of its own, the toolbar is the default, which has them.
        const bool before = config->toolbarLayout() < 2;
        if (before && toolbar != config->defaultToolbarActionsValue() && !toolbar.contains(u"search_field"_s)) {
            for (const QString &name : formerlyFixedInToolbar) {
                if (!toolbar.contains(name)) {
                    toolbar.append(name);
                }
            }
        }
        // The button that shows the preview and the one that puts it beside the list are
        // one button with a menu now. It takes the place of the first of the two, once;
        // whoever puts one of them back later keeps it.
        const bool twoButtons = config->toolbarLayout() < 3;
        if (twoButtons) {
            QStringList merged;
            for (const QString &name : std::as_const(toolbar)) {
                if (!formerPreviewButtons.contains(name)) {
                    merged.append(name);
                } else if (!merged.contains(u"menu_preview"_s)) {
                    merged.append(u"menu_preview"_s);
                }
            }
            toolbar = merged;
        }
        if (before || twoButtons || toolbar != chosen) {
            config->setToolbarLayout(3);
            config->setToolbarActions(toolbar);
            config->save();
        }
    }
    m_documents->setSortColumn(config->sortColumn());
    m_documents->setSortDescending(config->sortDescending());
    // Only while its dialog is open and something is typed there.
    m_linkCandidates->setActive(false);
    m_linkCandidates->setSortColumn(u"name"_s);
    m_linkCandidates->setSortDescending(false);
    m_sidebar->setCollapsedSections(config->collapsedSections());
    m_sidebar->setSectionOrder(config->sidebarSections());

    connect(m_sidebar, &SidebarModel::currentChanged, this, [this] {
        m_documents->setScope(m_sidebar->currentQuery());
        updateActions();
    });
    m_documents->setScope(m_sidebar->currentQuery());
    m_documents->reload();

    connect(m_documents, &DocumentModel::sortChanged, this, [this, config] {
        config->setSortColumn(m_documents->sortColumn());
        config->setSortDescending(m_documents->sortDescending());
        config->save();
    });
    connect(m_sidebar, &SidebarModel::sectionOrderChanged, this, [this, config] {
        config->setSidebarSections(m_sidebar->sectionOrder());
        config->save();
        updateActions();
    });
    connect(m_sidebar, &SidebarModel::collapsedSectionsChanged, this, [this, config] {
        config->setCollapsedSections(m_sidebar->collapsedSections());
        config->save();
    });
    // Opening a database is over when both the list and the numbers of the sidebar are there.
    const auto opened = [this, was = isOpening()]() mutable {
        if (was != isOpening()) {
            was = isOpening();
            Q_EMIT openingChanged();
        }
    };
    connect(m_documents, &DocumentModel::openingChanged, this, opened);
    connect(m_sidebar, &SidebarModel::openingChanged, this, opened);
    // "Empty Trash" goes by the number the sidebar has for the trash.
    connect(m_sidebar, &SidebarModel::countsChanged, this, &App::updateActions);
    connect(m_library, &Library::message, this, &App::passiveMessage);
    connect(m_library, &Library::documentCreated, this, [this](qint64 id) {
        // Show what was just made: go to the inbox if the document is not in the current list.
        setSidebarTab(u"database"_s);
        m_documents->reload();
        if (m_documents->rowOf(id) < 0) {
            m_sidebar->setCurrentKey(u"place:inbox"_s);
            m_documents->setFilter(QString());
        }
        m_documents->selectId(id);
    });

    connect(m_library, &Library::databaseChanged, this, [this] {
        // Nothing that was selected or looked at exists in the other database.
        setAnnotating(false);
        m_documents->clearSelection();
        m_documents->setFilter(QString());
        m_documents->setCriteria(QString());
        m_sidebar->setCurrentKey(u"place:inbox"_s);
        setCurrentRule(0);
        setCurrentScript(QString());
        Q_EMIT databasesChanged();
    });
    connect(m_library, &Library::databaseLost, this, [this] {
        const QString lost = databaseName();
        m_library->switchDatabase(QString());
        Q_EMIT passiveMessage(i18nc("@info", "The database “%1” can no longer be reached. Indeks went back to the local database.", lost));
    });
    if (!s_startupProblem()->isEmpty()) {
        QTimer::singleShot(1500, this, [this] {
            Q_EMIT passiveMessage(i18nc("@info", "%1 Indeks opened the local database instead.", *s_startupProblem()));
        });
    }

    setupActions();
    syncScriptActions();
    connect(m_library, &Library::scriptsChanged, this, &App::syncScriptActions);
    connect(m_library, &Library::scriptsRunningChanged, this, &App::updateActions);
    connect(m_library, &Library::scriptFinished, this, &App::reportScript);
    connect(m_documents, &DocumentModel::selectionChanged, this, &App::updateActions);
    connect(m_documents, &DocumentModel::filterChanged, this, &App::updateActions);
    // Whoever searches is looking for documents.
    connect(m_documents, &DocumentModel::filterChanged, this, [this] {
        if (!m_documents->filter().isEmpty()) {
            setSidebarTab(u"database"_s);
        }
    });
    connect(m_documents, &DocumentModel::currentChanged, this, &App::updateActions);
    connect(m_library, &Library::changed, this, &App::updateActions);
    connect(m_library, &Library::sourcesChanged, this, &App::updateActions);
    connect(m_library, &Library::todosChanged, this, &App::updateActions);
    updateActions();

    applySettings();
    m_library->start(config->rescanOnStart());
}

App::~App()
{
    Config::self()->save();
    s_instance = nullptr;
}

bool App::isOpening() const
{
    return m_documents->isOpening() || m_sidebar->isOpening();
}

App *App::instance()
{
    return s_instance;
}

App *App::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

QVariantList App::selection() const
{
    return m_documents->selectedIds();
}

QAction *App::addAction(const QString &name, const QString &text, const QString &icon, const QKeySequence &shortcut, const std::function<void()> &handler)
{
    auto action = new QAction(this);
    action->setText(text);
    action->setIcon(QIcon::fromTheme(icon));
    connect(action, &QAction::triggered, this, handler);
    mainCollection()->addAction(name, action);
    if (!shortcut.isEmpty()) {
        mainCollection()->setDefaultShortcut(action, shortcut);
    }
    return action;
}

QAction *App::addRequest(const QString &name, const QString &text, const QString &icon, const QKeySequence &shortcut)
{
    return addAction(name, text, icon, shortcut, [this, name] {
        Q_EMIT request(name);
    });
}

QAction *App::addToggle(const QString &name,
                        const QString &text,
                        const QString &icon,
                        const QKeySequence &shortcut,
                        bool checked,
                        const std::function<void(bool)> &handler)
{
    auto action = new QAction(this);
    action->setText(text);
    action->setIcon(QIcon::fromTheme(icon));
    action->setCheckable(true);
    action->setChecked(checked);
    connect(action, &QAction::toggled, this, handler);
    mainCollection()->addAction(name, action);
    if (!shortcut.isEmpty()) {
        mainCollection()->setDefaultShortcut(action, shortcut);
    }
    return action;
}

void App::setupActions()
{
    AbstractKirigamiApplication::setupActions();
    Config *config = Config::self();

    // Adding and creating
    addRequest(u"add_folder"_s, i18nc("@action", "Add Indexed Folder…"), u"folder-add"_s, QKeySequence::Open);
    addRequest(u"add_remote"_s, i18nc("@action", "Add Remote Location…"), u"folder-remote"_s);
    addRequest(u"new_text"_s, i18nc("@action", "New Plain Text Document…"), u"document-new"_s, QKeySequence::New);
    addRequest(u"new_markdown"_s, i18nc("@action", "New Markdown Document…"), u"text-markdown"_s);
    addRequest(u"new_richtext"_s, i18nc("@action", "New Formatted Document…"), u"x-office-document"_s);
    addAction(u"new_clipboard"_s, i18nc("@action", "New Document from Clipboard…"), u"edit-paste"_s, Qt::CTRL | Qt::SHIFT | Qt::Key_V, [this] {
        // The dialog is only asked for when there is something to make a document of.
        if (takeClipboard()) {
            Q_EMIT request(u"new_clipboard"_s);
        }
    });
    addRequest(u"db_manage"_s, i18nc("@action", "Databases…"), u"server-database"_s);
    addRequest(u"db_add"_s, i18nc("@action", "Add Database…"), u"document-open"_s);
    addRequest(u"db_new"_s, i18nc("@action", "New Database…"), u"folder-new"_s);
    addRequest(u"db_rename"_s, i18nc("@action", "Rename Database…"), u"edit-rename"_s);
    addAction(u"rescan"_s, i18nc("@action", "Update Index"), u"view-refresh"_s, QKeySequence::Refresh, [this] {
        m_library->rescanAll();
        Q_EMIT passiveMessage(i18nc("@info", "Looking for changes in all indexed locations."));
    });

    // Acting on the selected documents
    addAction(u"doc_open"_s, i18nc("@action", "Open File"), u"document-open-symbolic"_s, Qt::CTRL | Qt::Key_Return, [this] {
        const QList<qint64> ids = m_documents->selection();
        for (qint64 id : ids) {
            openExternally(id);
        }
    });
    addAction(u"doc_open_with"_s, i18nc("@action", "Open With…"), u"document-open"_s, {}, [this] {
        openWith(selection());
    });
    addAction(u"doc_show"_s, i18nc("@action", "Show in File Manager"), u"system-file-manager"_s, {}, [this] {
        showInFileManager(m_documents->currentId());
    });
    addAction(u"doc_copy_location"_s, i18nc("@action", "Copy Location"), u"edit-copy-path"_s, Qt::CTRL | Qt::ALT | Qt::Key_C, [this] {
        const QVariantMap info = m_library->documentInfo(m_documents->currentId());
        copyText(info.value(u"displayUrl"_s).toString());
    });
    addAction(u"doc_copy_address"_s, i18nc("@action", "Copy Web Address"), u"edit-copy"_s, {}, [this] {
        copyText(m_library->webAddress(m_documents->currentId()));
    });
    addRequest(u"doc_rename"_s, i18nc("@action", "Rename…"), u"edit-rename"_s, Qt::Key_F2);
    addRequest(u"doc_tag"_s, i18nc("@action", "Add Tag…"), u"tag-new"_s, Qt::CTRL | Qt::Key_T);
    addRequest(u"doc_group"_s, i18nc("@action", "Add to Group…"), u"folder-new"_s, Qt::CTRL | Qt::Key_G);
    addRequest(u"doc_note"_s, i18nc("@action", "Add Note…"), u"note"_s, Qt::CTRL | Qt::SHIFT | Qt::Key_M);
    addRequest(u"doc_links"_s, i18nc("@action", "Manage Linked Documents…"), u"link"_s, Qt::CTRL | Qt::Key_L);
    addAction(u"doc_link_selected"_s, i18nc("@action", "Link Selected Documents"), u"insert-link"_s, {}, [this] {
        const QVariantList ids = selection();
        const int made = m_library->linkDocuments(ids);
        // Turned down, with the reason, where there were too many.
        if (made == 0) {
            Q_EMIT passiveMessage(i18nc("@info", "The selected documents are linked with one another already."));
        } else if (made > 0) {
            Q_EMIT passiveMessage(i18ncp("@info", "The %1 selected document is linked now.", "The %1 selected documents are linked with one another now.", int(ids.size())));
        }
    });
    // One action for working on the document itself. What that means depends on the document:
    // text is edited, pages and pictures are drawn on. It stays checked while that goes on.
    addToggle(u"doc_edit"_s, i18nc("@action", "Edit"), u"document-edit"_s, Qt::CTRL | Qt::Key_E, false, [this](bool checked) {
        if (checked == (m_annotating || m_textEditing)) {
            return; // the action only follows what already happened
        }
        if (m_editMode == EditMode::Draw) {
            setAnnotating(checked);
        } else if (m_editMode == EditMode::Text) {
            Q_EMIT request(checked ? u"doc_edit"_s : u"doc_edit_done"_s);
        }
        // If nothing came of it, e.g. because the document is too large to edit, the action
        // must not stay checked.
        syncEditAction();
    });
    addRequest(u"doc_clear_drawing"_s, i18nc("@action", "Remove All Drawings"), u"edit-clear"_s);
    // Pinning is what used to be called flagging, and is still called that inside: in the
    // index, in the names of actions, and in rules and searches written before.
    addAction(u"doc_flag"_s, i18nc("@action", "Pin"), u"pin"_s, Qt::CTRL | Qt::SHIFT | Qt::Key_F, [this] {
        // Pin all if any of the selected ones is not pinned yet, else take the pins off.
        bool allFlagged = true;
        const QList<qint64> ids = m_documents->selection();
        for (qint64 id : ids) {
            allFlagged = allFlagged && Store::document(id).flagged;
        }
        m_library->setFlagged(selection(), !allFlagged);
    });
    addAction(u"doc_from_inbox"_s, i18nc("@action", "Remove from Inbox"), u"mail-mark-read"_s, {}, [this] {
        // The one way out of the inbox that is meant as one: nothing is held back in the list.
        m_documents->letGo(m_documents->selection());
        m_library->setInbox(selection(), false);
    });
    addAction(u"doc_to_inbox"_s, i18nc("@action", "Move to Inbox"), u"mail-folder-inbox"_s, {}, [this] {
        m_library->setInbox(selection(), true);
    });
    addAction(u"doc_from_group"_s, i18nc("@action", "Remove from This Group"), u"list-remove"_s, {}, [this] {
        m_documents->letGo(m_documents->selection());
        m_library->removeFromGroup(selection(), m_sidebar->currentId());
    });
    addAction(u"doc_ocr"_s, i18nc("@action", "Recognize Text (OCR)"), u"document-scan"_s, {}, [this] {
        m_library->requestOcr(selection());
    });
    addAction(u"doc_reindex"_s, i18nc("@action", "Read Again"), u"view-refresh"_s, {}, [this] {
        m_library->reindex(selection());
    });
    addAction(u"doc_revert"_s, i18nc("@action", "Discard Copy and Return to Original"), u"document-revert"_s, {}, [this] {
        const QList<qint64> ids = m_documents->selection();
        for (qint64 id : ids) {
            m_library->discardLocalCopy(id);
        }
    });
    addAction(u"doc_trash"_s, i18nc("@action", "Move to Trash"), u"user-trash"_s, QKeySequence::Delete, [this] {
        m_library->moveToTrash(selection());
    });
    addAction(u"doc_restore"_s, i18nc("@action", "Restore from Trash"), u"edit-undo"_s, {}, [this] {
        m_library->restoreFromTrash(selection());
    });
    addRequest(u"empty_trash"_s, i18nc("@action", "Empty Trash…"), u"trash-empty"_s);
    addAction(u"select_all"_s, i18nc("@action", "Select All Documents"), u"edit-select-all"_s, QKeySequence::SelectAll, [this] {
        m_documents->selectAll();
    });

    // Organizing
    addRequest(u"new_group"_s, i18nc("@action", "New Group…"), u"folder-new"_s, Qt::CTRL | Qt::SHIFT | Qt::Key_G);
    addRequest(u"new_tag"_s, i18nc("@action", "New Tag…"), u"tag-new"_s);
    addRequest(u"save_search"_s, i18nc("@action", "Save Search…"), u"document-save"_s, Qt::CTRL | Qt::SHIFT | Qt::Key_S);
    addRequest(u"new_rule"_s, i18nc("@action", "New Rule…"), u"run-build-configure"_s);
    addRequest(u"go_rules"_s, i18nc("@action", "Go to Rules"), u"run-build-configure"_s, Qt::CTRL | Qt::Key_5);
    // What it is called follows what is selected: a rule is deleted, a script is a file and
    // goes to the trash. By the key that moves documents to the trash: the two commands
    // are never both there, see updateActions(), and this one asks before it does anything.
    addRequest(u"automation_delete"_s, i18nc("@action", "Delete Selected Rule or Script…"), u"edit-delete"_s, QKeySequence::Delete);
    // Things to do
    addRequest(u"new_todo"_s, i18nc("@action", "New To-Do…"), u"task-new"_s);
    addRequest(u"doc_todo"_s, i18nc("@action", "Create To-Do for Document…"), u"task-new"_s);
    addAction(u"todos_clear_done"_s, i18nc("@action", "Remove Completed To-Dos"), u"edit-clear-history"_s, {}, [this] {
        const int removed = m_library->removeDoneTodos();
        Q_EMIT passiveMessage(i18ncp("@info", "%1 completed to-do was removed.", "%1 completed to-dos were removed.", removed));
    });
    // The order of the sections of the database view; what dragging a heading does.
    addAction(u"sidebar_section_up"_s, i18nc("@action", "Move Section of the Selected Sidebar Entry Up"), u"go-up"_s, {}, [this] {
        m_sidebar->shiftSection(m_sidebar->currentSection(), -1);
    });
    addAction(u"sidebar_section_down"_s, i18nc("@action", "Move Section of the Selected Sidebar Entry Down"), u"go-down"_s, {}, [this] {
        m_sidebar->shiftSection(m_sidebar->currentSection(), 1);
    });
    addRequest(u"edit_sidebar_item"_s, i18nc("@action", "Edit Selected Sidebar Entry…"), u"document-edit"_s);
    addRequest(u"delete_sidebar_item"_s, i18nc("@action", "Delete Selected Sidebar Entry…"), u"edit-delete"_s);
    {
        // Checked tells how it is with the location that is selected in the sidebar; only
        // the user triggering it changes that.
        auto sourceInbox = new QAction(this);
        sourceInbox->setText(i18nc("@action", "Put New Files of the Selected Location into the Inbox"));
        sourceInbox->setIcon(QIcon::fromTheme(u"mail-folder-inbox"_s));
        sourceInbox->setCheckable(true);
        connect(sourceInbox, &QAction::triggered, this, [this](bool checked) {
            if (m_sidebar->currentKind() == "source"_L1) {
                m_library->setSourceUsesInbox(m_sidebar->currentId(), checked);
            }
        });
        mainCollection()->addAction(u"source_inbox"_s, sourceInbox);
    }
    addAction(u"apply_rule"_s, i18nc("@action", "Apply Selected Rule"), u"media-playback-start"_s, {}, [this] {
        const int affected = m_library->applyRule(m_currentRule);
        // A rule that runs scripts takes its time in the background, where the activity
        // display shows it; the library says when it is through.
        if (affected >= 0) {
            Q_EMIT passiveMessage(i18ncp("@info", "The rule was applied to %1 document.", "The rule was applied to %1 documents.", affected));
        }
    });

    // Scripts
    addRequest(u"scripts_manage"_s, i18nc("@action", "Go to Scripts"), u"text-x-python"_s, Qt::CTRL | Qt::Key_6);
    addRequest(u"new_script"_s, i18nc("@action", "New Script…"), u"document-new"_s);
    addRequest(u"doc_run_script"_s, i18nc("@action", "Run Script…"), u"system-run"_s);
    addRequest(u"scripts_output"_s, i18nc("@action", "Show Script Output"), u"view-list-text"_s);
    addAction(u"scripts_stop"_s, i18nc("@action", "Stop Running Scripts"), u"process-stop"_s, {}, [this] {
        m_library->stopScripts();
    });

    // Going places
    addAction(u"go_inbox"_s, i18nc("@action", "Go to Inbox"), u"mail-folder-inbox"_s, Qt::CTRL | Qt::Key_1, [this] {
        goTo(u"place:inbox"_s);
    });
    addAction(u"go_all"_s, i18nc("@action", "Go to All Documents"), u"folder-documents"_s, Qt::CTRL | Qt::Key_2, [this] {
        goTo(u"place:all"_s);
    });
    addAction(u"go_flagged"_s, i18nc("@action", "Go to Pinned"), u"pin"_s, Qt::CTRL | Qt::Key_3, [this] {
        goTo(u"place:flagged"_s);
    });
    addAction(u"go_trash"_s, i18nc("@action", "Go to Trash"), u"user-trash"_s, Qt::CTRL | Qt::Key_4, [this] {
        goTo(u"place:trash"_s);
    });
    addRequest(u"focus_search"_s, i18nc("@action", "Search"), u"search"_s, QKeySequence::Find);
    addToggle(u"search_advanced"_s, i18nc("@action", "Advanced Search"), u"view-filter"_s, Qt::CTRL | Qt::ALT | Qt::Key_F, false, [this](bool checked) {
        setAdvancedSearch(checked);
    });

    // View
    // Each of the two panels by the keys other programs have for theirs, and by the
    // function key it has had here from the start.
    QAction *leftPanel =
        addToggle(u"view_left_sidebar"_s, i18nc("@action", "Show Sidebar"), u"sidebar-expand-left"_s, Qt::CTRL | Qt::Key_B, config->leftSidebarVisible(), [config](bool checked) {
            config->setLeftSidebarVisible(checked);
            config->save();
        });
    mainCollection()->setDefaultShortcuts(leftPanel, {Qt::CTRL | Qt::Key_B, Qt::Key_F9});
    QAction *rightPanel = addToggle(u"view_right_sidebar"_s,
                                    i18nc("@action", "Show Side Panel"),
                                    u"sidebar-expand-right"_s,
                                    Qt::CTRL | Qt::SHIFT | Qt::Key_B,
                                    config->rightSidebarVisible(),
                                    [config](bool checked) {
                                        config->setRightSidebarVisible(checked);
                                        config->save();
                                    });
    mainCollection()->setDefaultShortcuts(rightPanel, {Qt::CTRL | Qt::SHIFT | Qt::Key_B, Qt::Key_F11});

    // Where the views of the two panels are. What these are called follows where the view
    // is at the moment; see updatePanelActions().
    const QStringList views = PanelLayout::allViews();
    for (const QString &view : views) {
        addAction(u"panel_move_"_s + view, QString(), u"view-split-left-right"_s, {}, [this, view] {
            m_panels->moveAcross(view);
            showSidePanel(view);
        });
        addAction(u"panel_split_"_s + view, QString(), u"view-split-top-bottom"_s, {}, [this, view] {
            if (m_panels->partOf(view) == "bottom"_L1) {
                m_panels->merge(view);
            } else {
                m_panels->split(view);
            }
        });
        // Among the tabs it is with; what dragging a tab along its bar does.
        addAction(u"panel_earlier_"_s + view, QString(), u"go-previous"_s, {}, [this, view] {
            m_panels->shift(view, -1);
        });
        addAction(u"panel_later_"_s + view, QString(), u"go-next"_s, {}, [this, view] {
            m_panels->shift(view, 1);
        });
    }
    addAction(u"panel_reset"_s, i18nc("@action", "Reset Sidebar and Side Panel"), u"edit-reset"_s, {}, [this] {
        m_panels->reset();
    });
    connect(m_panels, &PanelLayout::changed, this, &App::updatePanelActions);
    updatePanelActions();
    addToggle(u"view_preview"_s, i18nc("@action", "Show Preview"), u"view-preview"_s, Qt::Key_F10, config->previewVisible(), [config](bool checked) {
        config->setPreviewVisible(checked);
        config->save();
    });
    addToggle(u"view_preview_beside"_s,
              i18nc("@action", "Preview Beside the List"),
              u"view-split-left-right"_s,
              {},
              config->previewBeside(),
              [this, config](bool checked) {
                  config->setPreviewBeside(checked);
                  config->save();
                  // Moving a preview that is hidden would show nothing of what was asked for.
                  if (QAction *preview = action(u"view_preview"_s); preview && !preview->isChecked()) {
                      preview->setChecked(true);
                  }
              });
    // The same as one choice of three, which is what the button in the toolbar offers:
    // no preview, below the list, or beside it.
    const auto choosePreview = [this, config](bool visible, bool beside) {
        if (QAction *where = action(u"view_preview_beside"_s); where && visible && where->isChecked() != beside) {
            where->setChecked(beside);
        }
        if (QAction *preview = action(u"view_preview"_s); preview && preview->isChecked() != visible) {
            preview->setChecked(visible);
        }
        syncPreviewChoice();
    };
    addAction(u"preview_none"_s, i18nc("@action", "No Preview"), u"view-preview"_s, {}, [choosePreview] {
        choosePreview(false, false);
    })->setCheckable(true);
    addAction(u"preview_below"_s, i18nc("@action", "Preview Below Documents"), u"view-split-top-bottom"_s, {}, [choosePreview] {
        choosePreview(true, false);
    })->setCheckable(true);
    addAction(u"preview_beside"_s, i18nc("@action", "Preview Beside Documents"), u"view-split-left-right"_s, {}, [choosePreview] {
        choosePreview(true, true);
    })->setCheckable(true);
    connect(config, &Config::previewVisibleChanged, this, &App::syncPreviewChoice);
    connect(config, &Config::previewBesideChanged, this, &App::syncPreviewChoice);
    syncPreviewChoice();

    // A table in the preview: what the menu of its columns offers, for the column of the
    // cell that is chosen.
    addRequest(u"table_sort_az"_s, i18nc("@action", "Sort Table by Current Column, A to Z"), u"view-sort-ascending"_s);
    addRequest(u"table_sort_za"_s, i18nc("@action", "Sort Table by Current Column, Z to A"), u"view-sort-descending"_s);
    addRequest(u"table_sort_smallest"_s, i18nc("@action", "Sort Table by Current Column, Smallest Number First"), u"view-sort-ascending"_s);
    addRequest(u"table_sort_largest"_s, i18nc("@action", "Sort Table by Current Column, Largest Number First"), u"view-sort-descending"_s);
    addRequest(u"table_unsort"_s, i18nc("@action", "Show Table in the Order of the File"), u"view-sort"_s);
    addRequest(u"table_hide_column"_s, i18nc("@action", "Hide Current Column of the Table"), u"view-hidden"_s);
    addRequest(u"table_show_columns"_s, i18nc("@action", "Show All Columns of the Table"), u"view-visible"_s);

    addAction(u"view_info"_s, i18nc("@action", "Show Document Information"), u"documentinfo"_s, {}, [this] {
        showSidePanel(u"info"_s);
    });
    addAction(u"view_todos"_s, i18nc("@action", "Show To-Do List"), u"view-task"_s, {}, [this] {
        showSidePanel(u"todo"_s);
    });
    // Searching within the document that is shown, in the panel on the right. Ctrl+F is
    // the search for documents; this is the one for a place in one of them.
    addAction(u"doc_find"_s, i18nc("@action", "Find in Document…"), u"edit-find"_s, Qt::CTRL | Qt::Key_D, [this] {
        showSidePanel(u"find"_s);
        Q_EMIT request(u"doc_find"_s);
    });
    addRequest(u"find_next"_s, i18nc("@action", "Find Next in Document"), u"go-down"_s, Qt::Key_F3);
    addRequest(u"find_previous"_s, i18nc("@action", "Find Previous in Document"), u"go-up"_s, Qt::SHIFT | Qt::Key_F3);

    addRequest(u"view_activity"_s, i18nc("@action", "Show Indexing Activity"), u"view-process-system"_s);
    addRequest(u"view_columns"_s, i18nc("@action", "Choose List Columns and Row Height…"), u"view-file-columns"_s);

    // The preview
    addRequest(u"view_zoom_in"_s, i18nc("@action", "Zoom In"), u"zoom-in"_s, QKeySequence::ZoomIn);
    addRequest(u"view_zoom_out"_s, i18nc("@action", "Zoom Out"), u"zoom-out"_s, QKeySequence::ZoomOut);
    addRequest(u"view_zoom_reset"_s, i18nc("@action", "Reset Zoom"), u"zoom-original"_s, Qt::CTRL | Qt::Key_0);
    // Formatted text as its source, a table as the lines of its file, indented JSON as it was
    // written. What the preview calls it depends on what it shows.
    addToggle(u"view_raw"_s, i18nc("@action", "Show Unformatted"), u"format-text-code"_s, Qt::CTRL | Qt::Key_U, false, [this](bool checked) {
        if (checked != m_rawView) {
            Q_EMIT request(u"view_raw"_s);
        }
        // If nothing came of it, the action must not stay as it was clicked.
        if (QAction *raw = action(u"view_raw"_s)) {
            raw->setChecked(m_rawView);
        }
    });

    addRequest(u"options_configure_toolbars"_s, i18nc("@action", "Configure Toolbar…"), u"configure-toolbars"_s);
    addAction(u"help_manual"_s, i18nc("@action", "Indeks Manual"), u"help-contents"_s, QKeySequence::HelpContents, [this] {
        openManual();
    });

    QAction *preferences = KStandardActions::preferences(
        this,
        [this] {
            Q_EMIT request(u"options_configure"_s);
        },
        this);
    mainCollection()->addAction(preferences->objectName(), preferences);

    // The command bar is Indeks' own (see CommandBar.qml): the stock one of this Kirigami Addons
    // version only reacts to the mouse, not to Enter.
    if (QAction *commandBar = action(u"open_kcommand_bar"_s)) {
        disconnect(commandBar, &QAction::triggered, this, nullptr);
        connect(commandBar, &QAction::triggered, this, [this] {
            Q_EMIT request(u"open_kcommand_bar"_s);
        });
    }

    // Quitting goes through the window, which asks about a rule or a script that was changed
    // and not saved.
    if (QAction *quit = action(u"file_quit"_s)) {
        disconnect(quit, &QAction::triggered, this, nullptr);
        connect(quit, &QAction::triggered, this, [this] {
            Q_EMIT request(u"file_quit"_s);
        });
    }

    readSettings();
}

void App::updateActions()
{
    // While the window is about rules and scripts, the documents are out of sight, and what
    // is selected among them is not what a command or a key is meant for.
    const bool automation = isAutomation();
    const QList<qint64> ids = automation ? QList<qint64>() : m_documents->selection();
    const bool any = !ids.isEmpty();
    const Document current = automation ? Document() : Store::document(m_documents->currentId());
    const bool single = ids.size() == 1 && current.isValid();
    const QString kind = automation ? QString() : m_sidebar->currentKind();
    const bool inTrash = !automation && m_sidebar->currentKey() == "place:trash"_L1;

    bool anyInbox = false;
    bool anyOutsideInbox = false;
    bool anyCopy = false;
    bool anyOcrCandidate = false;
    // Looking at every document of a huge selection is not worth it; assume the general case.
    if (ids.size() <= 200) {
        for (qint64 id : ids) {
            const Document doc = Store::document(id);
            anyInbox = anyInbox || doc.inbox;
            anyOutsideInbox = anyOutsideInbox || !doc.inbox;
            anyCopy = anyCopy || (!doc.owned && !doc.localCopy.isEmpty());
            anyOcrCandidate = anyOcrCandidate || doc.kind == Kind::Pdf || doc.kind == Kind::Image;
        }
    } else {
        anyInbox = anyOutsideInbox = anyCopy = anyOcrCandidate = true;
    }

    auto enable = [this](const QString &name, bool enabled) {
        if (QAction *found = action(name)) {
            found->setEnabled(enabled);
        }
    };
    const bool office = Office::isOfficeDocument(current.mimeType) && m_library->officeViewAvailable();
    const bool drawable = single && (current.kind == Kind::Pdf || current.kind == Kind::Image || office);
    const bool textual = single && !drawable && (current.kind == Kind::PlainText || current.mimeType == "text/html"_L1);

    if (QAction *open = action(u"doc_open"_s)) {
        open->setEnabled(any);
        // Says where the document will open: the application the desktop uses for its type.
        // Several documents only get a name if they all go to the same one.
        QString application;
        if (ids.size() <= 200) {
            QSet<QString> types;
            for (qint64 id : ids) {
                const Document doc = Store::document(id);
                // A website goes to the browser, not to what opens files of its type.
                types.insert(doc.kind == Kind::Website && !m_library->webAddress(id).isEmpty() ? u"x-scheme-handler/https"_s : doc.mimeType);
            }
            for (const QString &type : std::as_const(types)) {
                const KService::Ptr service = KApplicationTrader::preferredService(type);
                // Indeks itself as the default would mean opening the document where it is.
                const QString name = service && service->desktopEntryName() != QGuiApplication::desktopFileName() ? service->name() : QString();
                if (name.isEmpty() || (!application.isEmpty() && name != application)) {
                    application.clear();
                    break;
                }
                application = name;
            }
        }
        open->setText(!any                    ? i18nc("@action", "Open File")
                          : application.isEmpty() ? i18nc("@action", "Open in Default Application")
                                                  : i18nc("@action %1 is the name of an application", "Open in %1", application));
    }
    enable(u"doc_open_with"_s, any);
    enable(u"doc_copy_address"_s, single && current.kind == Kind::Website && !m_library->webAddress(current.id).isEmpty());
    enable(u"doc_show"_s, single && current.url.isLocalFile());
    enable(u"doc_copy_location"_s, single);
    enable(u"doc_rename"_s, single && current.owned);
    enable(u"doc_tag"_s, any);
    enable(u"doc_group"_s, any);
    enable(u"doc_note"_s, single);
    enable(u"doc_links"_s, single && !inTrash);
    enable(u"doc_link_selected"_s, ids.size() >= 2 && !inTrash);
    enable(u"doc_todo"_s, single);
    enable(u"todos_clear_done"_s, m_library->todoCounts().value(u"done"_s).toInt() > 0);
    m_editMode = drawable ? EditMode::Draw : textual ? EditMode::Text : EditMode::None;
    if (QAction *edit = action(u"doc_edit"_s)) {
        edit->setEnabled(m_editMode != EditMode::None);
        edit->setText(drawable ? i18nc("@action", "Draw on Document") : textual ? i18nc("@action", "Edit Text") : i18nc("@action", "Edit"));
        edit->setIcon(QIcon::fromTheme(drawable ? u"draw-freehand"_s : u"document-edit"_s));
    }
    enable(u"doc_clear_drawing"_s, drawable && m_library->hasInk(current.id));
    enable(u"doc_flag"_s, any);
    enable(u"doc_from_inbox"_s, anyInbox);
    enable(u"doc_to_inbox"_s, anyOutsideInbox && !inTrash);
    enable(u"doc_from_group"_s, any && kind == "group"_L1);
    enable(u"doc_ocr"_s, anyOcrCandidate && m_library->ocrAvailable());
    enable(u"doc_reindex"_s, any);
    enable(u"doc_revert"_s, anyCopy);
    enable(u"doc_trash"_s, any && !inTrash);
    enable(u"doc_restore"_s, any && inTrash);
    // Not counted here: that is a walk through all documents, with every change of the
    // selection. The sidebar has the number, a moment later in a large index.
    enable(u"empty_trash"_s, m_sidebar->countOf(u"place:trash"_s) > 0);
    enable(u"sidebar_section_up"_s, !automation && m_sidebar->canShiftSection(m_sidebar->currentSection(), -1));
    enable(u"sidebar_section_down"_s, !automation && m_sidebar->canShiftSection(m_sidebar->currentSection(), 1));
    enable(u"select_all"_s, !automation && m_documents->count() > 0);
    enable(u"save_search"_s, !automation && !m_documents->effectiveFilter().isEmpty());
    const bool editable = kind == "group"_L1 || kind == "tag"_L1 || kind == "search"_L1 || kind == "source"_L1;
    enable(u"edit_sidebar_item"_s, editable && kind != "source"_L1);
    enable(u"delete_sidebar_item"_s, editable);
    const bool onRules = automation && m_automationPage == "rules"_L1;
    const bool onScripts = automation && m_automationPage == "scripts"_L1;
    enable(u"apply_rule"_s, onRules && m_currentRule > 0);
    if (QAction *remove = action(u"automation_delete"_s)) {
        remove->setEnabled((onRules && m_currentRule > 0) || (onScripts && !m_currentScript.isEmpty()));
        remove->setText(onRules     ? i18nc("@action", "Delete Rule…")
                            : onScripts ? i18nc("@action", "Move Script to Trash…")
                                        : i18nc("@action", "Delete Selected Rule or Script…"));
    }
    if (QAction *inbox = action(u"source_inbox"_s)) {
        inbox->setEnabled(kind == "source"_L1);
        inbox->setChecked(kind == "source"_L1 && m_library->sourceUsesInbox(m_sidebar->currentId()));
    }
    enable(u"view_zoom_in"_s, m_previewZoomable);
    enable(u"view_zoom_out"_s, m_previewZoomable);
    enable(u"view_zoom_reset"_s, m_previewZoomable);
    enable(u"view_raw"_s, m_rawViewAvailable);
    for (const QLatin1StringView name : tableActions) {
        enable(name, m_tableShown);
    }
    enable(u"doc_run_script"_s, any && !inTrash);
    enable(u"scripts_stop"_s, m_library->scriptsRunning());
    const QList<QAction *> all = mainCollection()->actions();
    for (QAction *candidate : all) {
        if (candidate->objectName().startsWith(scriptActionPrefix)) {
            candidate->setEnabled(any && !inTrash);
        }
    }

    if (!drawable && m_annotating) {
        setAnnotating(false);
    }
}

void App::syncScriptActions()
{
    // One action per script: that is what puts them into the command bar and lets them have
    // keyboard shortcuts.
    QSet<QString> wanted;
    bool added = false;
    const QVariantList scripts = m_library->scripts();
    for (const QVariant &entry : scripts) {
        const QVariantMap script = entry.toMap();
        const QString file = script.value(u"file"_s).toString();
        const QString name = scriptActionPrefix + file;
        wanted.insert(name);
        if (action(name)) {
            continue;
        }
        auto run = new QAction(this);
        run->setText(i18nc("@action %1 is the name of a script", "Run Script “%1”", script.value(u"name"_s).toString()));
        run->setIcon(QIcon::fromTheme(u"system-run"_s));
        connect(run, &QAction::triggered, this, [this, file] {
            runScript(file);
        });
        mainCollection()->addAction(name, run);
        added = true;
    }
    const QList<QAction *> all = mainCollection()->actions();
    for (QAction *candidate : all) {
        if (candidate->objectName().startsWith(scriptActionPrefix) && !wanted.contains(candidate->objectName())) {
            mainCollection()->removeAction(candidate);
        }
    }
    if (added) {
        readSettings(); // the shortcuts given to them earlier
        updateActions();
    }
}

void App::runScript(const QString &file)
{
    const QVariantList ids = selection();
    if (ids.isEmpty()) {
        Q_EMIT passiveMessage(i18nc("@info", "Select the documents to run the script on first."));
        return;
    }
    const QString problem = m_library->scriptsProblem();
    if (!problem.isEmpty()) {
        Q_EMIT passiveMessage(problem);
        return;
    }
    m_library->runScript(file, ids);
}

void App::openScriptsFolder()
{
    // It is only made when the first script is saved; someone who wants to put a script
    // there by hand needs it now.
    const QString dir = m_library->scriptsDir();
    QDir().mkpath(dir);
    auto job = new KIO::OpenUrlJob(QUrl::fromLocalFile(dir), u"inode/directory"_s);
    connect(job, &KJob::result, this, [this, job] {
        if (job->error()) {
            Q_EMIT passiveMessage(job->errorString());
        }
    });
    job->start();
}

void App::openManual()
{
    // The manual is a PDF that is made when Indeks is built (see doc/) and installed with
    // it. Run from where it was built, Indeks takes the one that was made there.
    QString manual = QStandardPaths::locate(QStandardPaths::GenericDataLocation, u"indeks/Indeks-Manual.pdf"_s);
    if (manual.isEmpty() && QFileInfo::exists(QStringLiteral(INDEKS_BUILT_MANUAL))) {
        manual = QStringLiteral(INDEKS_BUILT_MANUAL);
    }
    if (manual.isEmpty()) {
        Q_EMIT passiveMessage(i18nc("@info", "The manual is not installed."));
        return;
    }
    auto job = new KIO::OpenUrlJob(QUrl::fromLocalFile(manual), u"application/pdf"_s);
    connect(job, &KJob::result, this, [this, job] {
        if (job->error()) {
            Q_EMIT passiveMessage(job->errorString());
        }
    });
    job->start();
}

void App::reportScript(const QVariantMap &report)
{
    const QString name = report.value(u"name"_s).toString();
    const QString rule = report.value(u"rule"_s).toString();
    const QString problem = report.value(u"problem"_s).toString();
    const int documents = report.value(u"documents"_s).toInt();
    const int changed = report.value(u"changed"_s).toInt();
    const int failed = report.value(u"failed"_s).toInt();

    if (!rule.isEmpty()) {
        // Rules run whenever documents come in. They only speak up when something is
        // wrong, and not for every handful of files of a long import.
        if (problem.isEmpty() && failed == 0) {
            return;
        }
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        if (now - m_scriptComplaints.value(name) < ScriptComplaintIntervalSecs) {
            return;
        }
        m_scriptComplaints.insert(name, now);
        Q_EMIT scriptMessage(problem.isEmpty()
                                 ? i18ncp("@info", "The script “%2” of the rule “%3” failed on %1 document.", "The script “%2” of the rule “%3” failed on %1 documents.", failed, name, rule)
                                 : i18nc("@info", "The script “%1” of the rule “%2” did not get through.", name, rule));
        return;
    }
    if (!problem.isEmpty()) {
        Q_EMIT scriptMessage(i18nc("@info", "The script “%1” did not get through.", name));
    } else if (report.value(u"stopped"_s).toBool()) {
        Q_EMIT scriptMessage(i18ncp("@info", "The script “%2” was stopped after %1 document.", "The script “%2” was stopped after %1 documents.", documents, name));
    } else if (failed > 0) {
        Q_EMIT scriptMessage(i18ncp("@info", "The script “%2” failed on %1 document.", "The script “%2” failed on %1 documents.", failed, name));
    } else if (documents == 1) {
        Q_EMIT scriptMessage(changed == 1 ? i18nc("@info", "The script “%1” ran on the document and changed it.", name)
                                          : i18nc("@info", "The script “%1” ran on the document and found nothing to do.", name));
    } else {
        const QString ran = i18ncp("@info", "The script “%2” ran on %1 document.", "The script “%2” ran on %1 documents.", documents, name);
        const QString did = changed == 0 ? i18nc("@info what a script did; follows how many documents it ran on", "It found nothing to do.")
                                         : i18ncp("@info what a script did; follows how many documents it ran on", "It changed %1 of them.", "It changed %1 of them.", changed);
        Q_EMIT scriptMessage(i18nc("@info two sentences about a script that ran", "%1 %2", ran, did));
    }
}

void App::setAnnotating(bool annotating)
{
    if (m_annotating == annotating) {
        return;
    }
    m_annotating = annotating;
    if (annotating) {
        // Drawing happens on Indeks' own copy. Make it now rather than at the first stroke,
        // so that the view does not switch files in the middle of drawing.
        m_library->ensureLocalCopy(m_documents->currentId());
    }
    syncEditAction();
    Q_EMIT annotatingChanged();
}

void App::setAdvancedSearch(bool shown)
{
    if (m_advancedSearch == shown) {
        return;
    }
    m_advancedSearch = shown;
    if (shown) {
        // The filters are for documents.
        setSidebarTab(u"database"_s);
    }
    if (QAction *toggle = action(u"search_advanced"_s)) {
        toggle->setChecked(shown);
    }
    Q_EMIT advancedSearchChanged();
}

void App::setTextEditing(bool editing)
{
    if (m_textEditing == editing) {
        return;
    }
    m_textEditing = editing;
    syncEditAction();
    Q_EMIT textEditingChanged();
}

void App::setPreviewZoomable(bool zoomable)
{
    if (m_previewZoomable != zoomable) {
        m_previewZoomable = zoomable;
        updateActions();
        Q_EMIT previewChanged();
    }
}

void App::setTableShown(bool shown)
{
    if (m_tableShown != shown) {
        m_tableShown = shown;
        Q_EMIT previewChanged();
        updateActions();
    }
}

void App::setRawViewAvailable(bool available)
{
    if (m_rawViewAvailable != available) {
        m_rawViewAvailable = available;
        updateActions();
        Q_EMIT previewChanged();
    }
}

void App::setRawView(bool raw)
{
    if (m_rawView != raw) {
        m_rawView = raw;
        if (QAction *toggle = action(u"view_raw"_s)) {
            toggle->setChecked(raw);
        }
        Q_EMIT previewChanged();
    }
}

void App::syncEditAction()
{
    if (QAction *edit = action(u"doc_edit"_s)) {
        edit->setChecked(m_annotating || m_textEditing);
    }
}

void App::goTo(const QString &sidebarKey)
{
    setSidebarTab(u"database"_s);
    m_sidebar->setCurrentKey(sidebarKey);
}

void App::setSidebarTab(const QString &tab)
{
    const QString wanted = tab == "automation"_L1 ? u"automation"_s : u"database"_s;
    if (m_sidebarTab == wanted) {
        return;
    }
    m_sidebarTab = wanted;
    // The view that goes with it comes up: the one for automation wherever it is, the one
    // for the database where the two share an area and the other was up. A database view
    // that stands behind something else stays there: whoever put it there is looking at
    // that, and a search must not take it away.
    if (wanted == "automation"_L1) {
        m_panels->setCurrent(wanted);
    } else if (m_panels->isCurrent(u"automation"_s) && m_panels->sideOf(u"automation"_s) == m_panels->sideOf(u"database"_s)
               && m_panels->partOf(u"automation"_s) == m_panels->partOf(u"database"_s)) {
        m_panels->setCurrent(wanted);
    }
    Q_EMIT automationChanged();
    updateActions();
}

void App::syncPreviewChoice()
{
    const Config *config = Config::self();
    const bool visible = config->previewVisible();
    const bool beside = config->previewBeside();
    // Whichever was clicked has changed its own mark by then; all three say how it is.
    const std::pair<QLatin1StringView, bool> choices[] = {{"preview_none"_L1, !visible}, {"preview_below"_L1, visible && !beside}, {"preview_beside"_L1, visible && beside}};
    for (const auto &[name, chosen] : choices) {
        if (QAction *choice = action(name)) {
            choice->setChecked(chosen);
        }
    }
}

void App::updatePanelActions()
{
    const QStringList views = PanelLayout::allViews();
    for (const QString &view : views) {
        const QString title = m_panels->title(view);
        if (QAction *move = action(u"panel_move_"_s + view)) {
            move->setText(m_panels->sideOf(view) == "left"_L1 ? i18nc("@action %1 is what a view of a panel is called, e.g. To-Do", "Move %1 to the Right Panel", title)
                                                              : i18nc("@action %1 is what a view of a panel is called, e.g. To-Do", "Move %1 to the Left Panel", title));
        }
        if (QAction *split = action(u"panel_split_"_s + view)) {
            const bool below = m_panels->partOf(view) == "bottom"_L1;
            split->setText(below ? i18nc("@action %1 is what a view of a panel is called, e.g. To-Do", "Merge %1 to the Top of Its Panel", title)
                                 : i18nc("@action %1 is what a view of a panel is called, e.g. To-Do", "Split %1 Below in Its Panel", title));
            split->setEnabled(below || m_panels->canSplit(view));
        }
        if (QAction *earlier = action(u"panel_earlier_"_s + view)) {
            earlier->setText(i18nc("@action %1 is what a view of a panel is called, e.g. To-Do", "Move the %1 Tab One Place to the Left", title));
            earlier->setEnabled(m_panels->canShift(view, -1));
        }
        if (QAction *later = action(u"panel_later_"_s + view)) {
            later->setText(i18nc("@action %1 is what a view of a panel is called, e.g. To-Do", "Move the %1 Tab One Place to the Right", title));
            later->setEnabled(m_panels->canShift(view, 1));
        }
    }
}

void App::setAutomationPage(const QString &page)
{
    const QString wanted = page == "scripts"_L1 ? u"scripts"_s : u"rules"_s;
    if (m_automationPage == wanted) {
        return;
    }
    m_automationPage = wanted;
    Q_EMIT automationChanged();
    updateActions();
}

void App::setCurrentRule(qint64 id)
{
    if (m_currentRule == id) {
        return;
    }
    m_currentRule = id;
    Q_EMIT automationChanged();
    updateActions();
}

void App::setCurrentScript(const QString &file)
{
    if (m_currentScript == file) {
        return;
    }
    m_currentScript = file;
    Q_EMIT automationChanged();
    updateActions();
}

void App::showSidePanel(const QString &view)
{
    const QString side = m_panels->sideOf(view);
    if (side.isEmpty()) {
        return;
    }
    m_panels->setCurrent(view);
    if (QAction *panel = action(side == "left"_L1 ? u"view_left_sidebar"_s : u"view_right_sidebar"_s); panel && !panel->isChecked()) {
        panel->setChecked(true);
    }
}

void App::showDocument(qint64 documentId)
{
    setSidebarTab(u"database"_s);
    if (m_documents->rowOf(documentId) < 0) {
        m_documents->setFilter(QString());
        m_documents->setCriteria(QString());
        goTo(Store::document(documentId).trashed ? u"place:trash"_s : u"place:all"_s);
    }
    m_documents->selectId(documentId);
}

void App::addFolders(const QList<QUrl> &urls)
{
    // The library says what it did not take, and why.
    const qint64 location = m_library->addFolders(urls);
    if (location != 0) {
        m_documents->setFilter(QString());
        goTo(u"source:%1"_s.arg(location));
    }
}

void App::handleArguments(const QStringList &arguments, const QString &workingDirectory)
{
    QCommandLineParser parser;
    parser.addPositionalArgument(u"folders"_s, QString());
    parser.parse(arguments);

    QList<QUrl> folders;
    const QStringList positional = parser.positionalArguments();
    for (const QString &argument : positional) {
        folders.append(QUrl::fromUserInput(argument, workingDirectory.isEmpty() ? QDir::currentPath() : workingDirectory, QUrl::AssumeLocalFile));
    }
    addFolders(folders);
    Q_EMIT raiseWindow();
}

void App::openExternally(qint64 documentId)
{
    const Document doc = Store::document(documentId);
    if (!doc.isValid()) {
        return;
    }
    // A website is opened where websites are looked at. A file of that kind that holds no
    // address on the web is opened like any other file.
    if (doc.kind == Kind::Website) {
        const QUrl address(m_library->webAddress(documentId));
        if (address.isValid()) {
            if (!QDesktopServices::openUrl(address)) {
                Q_EMIT passiveMessage(i18nc("@info %1 is a web address", "Cannot open “%1” in a browser.", address.toDisplayString()));
            }
            return;
        }
    }
    // Prefer what Indeks shows: the working copy if there is one.
    const QString path = Store::ensureContent(doc);
    auto job = new KIO::OpenUrlJob(path.isEmpty() ? doc.url : QUrl::fromLocalFile(path), doc.mimeType);
    connect(job, &KJob::result, this, [this, job] {
        if (job->error()) {
            Q_EMIT passiveMessage(job->errorString());
        }
    });
    job->start();
}

void App::openWith(const QVariantList &documentIds)
{
    QList<QUrl> urls;
    for (const QVariant &id : documentIds) {
        const Document doc = Store::document(id.toLongLong());
        if (!doc.isValid()) {
            continue;
        }
        // As with opening: what Indeks shows, which is the working copy if there is one.
        const QString path = Store::ensureContent(doc);
        urls.append(path.isEmpty() ? doc.url : QUrl::fromLocalFile(path));
    }
    if (urls.isEmpty()) {
        return;
    }
    // A launcher job that is not told which application to use asks: with the desktop's own
    // "Open With" dialog, where the choice can also be made the default for the file type.
    auto job = new KIO::ApplicationLauncherJob();
    job->setUrls(urls);
    job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, nullptr));
    if (QWindow *window = QGuiApplication::focusWindow() ? QGuiApplication::focusWindow() : QGuiApplication::topLevelWindows().value(0)) {
        KJobWindows::setWindow(job, window);
    }
    connect(job, &KJob::result, this, [this, job] {
        // Closing the dialog without choosing is not something to report.
        if (job->error() && job->error() != KJob::KilledJobError) {
            Q_EMIT passiveMessage(job->errorString());
        }
        // The default application may have been changed in the dialog.
        updateActions();
    });
    job->start();
}

void App::showInFileManager(qint64 documentId)
{
    const Document doc = Store::document(documentId);
    if (doc.isValid()) {
        KIO::highlightInFileManager({doc.url});
    }
}

QUrl App::lineIcon(const QString &name) const
{
    // A negative number asks for that size rather than for a group of icons.
    const QString path = KIconLoader::global()->iconPath(name, -22, true);
    return path.isEmpty() ? QUrl() : QUrl::fromLocalFile(path);
}

void App::copyText(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
}

namespace
{
/** Nothing to read: spaces, and the marks that stand for pictures in text made from HTML. */
bool isBlank(const QString &text)
{
    return std::ranges::all_of(text, [](QChar c) {
        return c.isSpace() || c == QChar::ObjectReplacementCharacter;
    });
}

/** The first line of a text that says something, as the name of the document made of it. */
QString nameFromText(const QString &text)
{
    const int longest = 60;
    qsizetype from = 0;
    while (from < text.size()) {
        qsizetype to = text.indexOf(u'\n', from);
        if (to < 0) {
            to = text.size();
        }
        QString name = text.mid(from, std::min<qsizetype>(to - from, 400));
        from = to + 1;
        // Not every place a database can be in takes every name: a share of a NAS has no
        // file with a colon or a question mark in its name.
        static const QRegularExpression unwelcome(u"[\\\\/:*?\"<>|\\x00-\\x1f]"_s);
        name.replace(unwelcome, u" "_s);
        name = name.simplified();
        // A heading in Markdown is named without its marks.
        while (name.startsWith(u'#')) {
            name.remove(0, 1);
        }
        name = name.trimmed();
        if (name.isEmpty()) {
            continue;
        }
        if (name.size() > longest) {
            // Cut between words where there is such a place in the later half.
            const qsizetype space = name.lastIndexOf(u' ', longest);
            name.truncate(space > longest / 2 ? space : longest);
        }
        while (!name.isEmpty() && (name.back().isSpace() || u".,:;"_s.contains(name.back()))) {
            name.chop(1);
        }
        if (!name.isEmpty()) {
            return name;
        }
    }
    return {};
}
}

bool App::takeClipboard()
{
    forgetClipboard();
    const QMimeData *data = QGuiApplication::clipboard()->mimeData();
    if (!data) {
        Q_EMIT passiveMessage(i18nc("@info", "The clipboard is empty."));
        return false;
    }

    // Files that were copied in a file manager are not content to make a document of: files
    // come into Indeks through the folders they are in. The last three are how files travel
    // into a sandbox and how other desktops mark them.
    bool files = data->hasFormat(u"application/vnd.portal.filetransfer"_s) || data->hasFormat(u"application/vnd.portal.files"_s)
        || data->hasFormat(u"x-special/gnome-copied-files"_s);
    if (!files && data->hasUrls()) {
        const QList<QUrl> urls = data->urls();
        files = std::ranges::any_of(urls, &QUrl::isLocalFile);
    }

    if (data->hasImage()) {
        m_clippedPicture = qvariant_cast<QImage>(data->imageData());
    }
    // Beside files, the text is only their addresses.
    if (!files) {
        if (data->hasHtml()) {
            m_clippedHtml = data->html();
        }
        if (data->hasText()) {
            m_clippedText = data->text();
        }
        if (isBlank(m_clippedText) && !m_clippedHtml.isEmpty()) {
            // Formatted text that came without its plain form.
            m_clippedText = QTextDocumentFragment::fromHtml(m_clippedHtml).toPlainText();
            m_clippedText.remove(QChar::ObjectReplacementCharacter);
        }
        m_clippedText.replace(u"\r\n"_s, u"\n"_s);
        if (isBlank(m_clippedText)) {
            // HTML without words is what a browser puts beside a copied picture.
            m_clippedText.clear();
            m_clippedHtml.clear();
        }
    }

    const bool picture = !m_clippedPicture.isNull();
    const bool text = !m_clippedText.isEmpty();
    // Text that is the address of a website and nothing else.
    m_clippedAddress = WebLink::addressOfText(m_clippedText);
    const bool address = m_clippedAddress.isValid();
    if (!picture && !text) {
        forgetClipboard();
        Q_EMIT passiveMessage(files ? i18nc("@info",
                                            "The clipboard holds files. Indeks takes folders, not single files: add the folder they are in as an indexed "
                                            "location.")
                                    : i18nc("@info", "The clipboard holds neither text nor a picture."));
        return false;
    }

    QVariantList formats;
    const auto offer = [&formats](const QString &format, const QString &label) {
        formats.append(QVariantMap{{u"format"_s, format}, {u"label"_s, label}});
    };
    if (address) {
        offer(u"website"_s, i18nc("@item:inlistbox what to make of the clipboard", "Website"));
    }
    if (text && !m_clippedHtml.isEmpty()) {
        offer(u"html"_s, i18nc("@item:inlistbox what to make of the clipboard", "Formatted document"));
    }
    if (text) {
        offer(u"text"_s, i18nc("@item:inlistbox what to make of the clipboard", "Plain text document"));
        offer(u"markdown"_s, i18nc("@item:inlistbox what to make of the clipboard", "Markdown document"));
    }
    if (picture) {
        offer(u"picture"_s, i18nc("@item:inlistbox what to make of the clipboard", "Picture"));
    }

    // Where there is both, the text is what was copied, a table with its picture for
    // instance; but a single word beside a picture is the address the picture came from.
    static const QRegularExpression space(u"\\s"_s);
    const bool pictureFirst = picture && (!text || !m_clippedText.trimmed().contains(space));

    m_clipping = {
        {u"formats"_s, formats},
        {u"format"_s, pictureFirst ? u"picture"_s : address ? u"website"_s : m_clippedHtml.isEmpty() ? u"text"_s : u"html"_s},
        {u"text"_s, m_clippedText.left(1200).trimmed()},
        {u"textName"_s, nameFromText(m_clippedText)},
        {u"websiteName"_s, address ? WebLink::nameFor(m_clippedAddress) : QString()},
        {u"pictureName"_s, i18nc("default name of a picture made from the clipboard", "Picture from the Clipboard")},
        {u"width"_s, m_clippedPicture.width()},
        {u"height"_s, m_clippedPicture.height()},
    };
    if (picture) {
        // A small copy for the dialog to show.
        const QSize room(480, 270);
        const QImage small = m_clippedPicture.width() > room.width() || m_clippedPicture.height() > room.height()
            ? m_clippedPicture.scaled(room, Qt::KeepAspectRatio, Qt::SmoothTransformation)
            : m_clippedPicture;
        QByteArray png;
        QBuffer buffer(&png);
        if (buffer.open(QIODevice::WriteOnly) && small.save(&buffer, "PNG")) {
            m_clipping.insert(u"thumbnail"_s, QString(u"data:image/png;base64,"_s + QString::fromLatin1(png.toBase64())));
        }
    }
    return true;
}

qint64 App::createFromClipboard(const QString &name, const QString &format)
{
    qint64 id = 0;
    if (format == "website"_L1) {
        id = m_library->createWebLink(name, m_clippedAddress);
    } else if (format == "picture"_L1) {
        id = m_library->createPicture(name, m_clippedPicture);
    } else if (format == "html"_L1) {
        id = m_library->createDocumentFromHtml(name, m_clippedHtml);
    } else if (!m_clippedText.isEmpty()) {
        id = m_library->createDocument(name, format, m_clippedText);
    }
    forgetClipboard();
    return id;
}

void App::forgetClipboard()
{
    m_clipping.clear();
    m_clippedText.clear();
    m_clippedHtml.clear();
    m_clippedAddress.clear();
    m_clippedPicture = QImage();
}

void App::applySettings()
{
    Config *config = Config::self();
    IndexOptions options;
    options.indexContent = config->indexContent();
    options.indexHidden = config->indexHidden();
    options.ocrEnabled = config->ocrEnabled();
    options.ocrLanguages = config->ocrLanguages();
    options.ocrJobs = config->ocrJobs();
    options.officeConversion = config->officeConversion();
    options.indexJobs = config->indexJobs();
    options.maxContentSizeMb = config->maxContentSizeMb();
    options.maxRemoteSizeMb = config->maxRemoteSizeMb();
    m_library->setOptions(options);
    m_library->setTodoWarningDays(config->todoWarningDays());
    config->save();
}

QString App::databaseName() const
{
    const QString open = cleanedPath(m_library->databaseDir());
    const QStringList paths = Config::self()->databasePaths();
    const QStringList names = Config::self()->databaseNames();
    const qsizetype index = paths.indexOf(open);
    return index >= 0 && index < names.size() ? names.at(index) : localDatabaseName();
}

QString App::localDatabaseName() const
{
    const QString chosen = Config::self()->localDatabaseName().trimmed();
    return chosen.isEmpty() ? i18nc("@item the database every installation has", "Local Database") : chosen;
}

bool App::renameDatabase(const QString &path, const QString &name)
{
    const QString trimmed = name.simplified();
    if (trimmed.isEmpty()) {
        return false;
    }
    Config *config = Config::self();
    const QString cleaned = cleanedPath(path);
    if (cleaned.isEmpty() || cleaned == cleanedPath(Paths::defaultDataDir())) {
        config->setLocalDatabaseName(trimmed);
    } else {
        const QStringList paths = config->databasePaths();
        QStringList names = config->databaseNames();
        const qsizetype index = paths.indexOf(cleaned);
        if (index < 0) {
            return false;
        }
        // The list of names may be shorter than that of the folders: those go by their folder's name.
        while (names.size() < paths.size()) {
            names.append(QDir(paths.at(names.size())).dirName());
        }
        names[index] = trimmed;
        config->setDatabaseNames(names);
    }
    config->save();
    Q_EMIT databasesChanged();
    return true;
}

QVariantList App::databases() const
{
    const QString open = cleanedPath(m_library->databaseDir());
    const QString local = cleanedPath(Paths::defaultDataDir());
    QVariantList list;
    list.append(QVariantMap{
        {u"name"_s, localDatabaseName()},
        {u"path"_s, local},
        {u"active"_s, open == local},
        {u"builtin"_s, true},
        {u"reachable"_s, true},
        {u"network"_s, false},
        {u"problem"_s, QString()},
    });
    const QStringList paths = Config::self()->databasePaths();
    const QStringList names = Config::self()->databaseNames();
    for (qsizetype i = 0; i < paths.size(); ++i) {
        const QString path = paths.at(i);
        const bool active = open == path;
        const DatabaseFolder::State state = DatabaseFolder::inspect(path);
        list.append(QVariantMap{
            {u"name"_s, names.value(i, QDir(path).dirName())},
            {u"path"_s, path},
            {u"active"_s, active},
            {u"builtin"_s, false},
            {u"reachable"_s, state.reachable},
            {u"network"_s, state.network},
            {u"problem"_s, active ? QString() : Library::databaseProblem(path, false)},
        });
    }
    return list;
}

QString App::localPath(const QString &input) const
{
    return DatabaseChoice::localPath(input);
}

QVariantMap App::inspectDatabaseFile(const QString &file) const
{
    return DatabaseChoice::inspectIndexFile(file);
}

QString App::addDatabase(const QString &name, const QString &file)
{
    const QVariantMap inspected = inspectDatabaseFile(file);
    if (!inspected.value(u"valid"_s).toBool()) {
        return i18nc("@info %1 is a file name", "Choose the file “%1” of a database, on this computer or on a mounted share.", Paths::indexFileName());
    }
    const QString problem = inspected.value(u"problem"_s).toString();
    if (!problem.isEmpty()) {
        return problem;
    }
    return openAndListDatabase(name, inspected.value(u"path"_s).toString(), false);
}

QVariantMap App::inspectNewDatabaseFolder(const QString &folder) const
{
    return DatabaseChoice::inspectNewFolder(folder);
}

QString App::createDatabase(const QString &name, const QString &folder)
{
    const QVariantMap inspected = inspectNewDatabaseFolder(folder);
    if (!inspected.value(u"valid"_s).toBool()) {
        return i18nc("@info", "Choose a folder on this computer or on a mounted share.");
    }
    const QString problem = inspected.value(u"problem"_s).toString();
    if (!problem.isEmpty()) {
        return problem;
    }
    return openAndListDatabase(name, inspected.value(u"path"_s).toString(), true);
}

QString App::openAndListDatabase(const QString &name, const QString &path, bool create)
{
    const QString error = m_library->switchDatabase(path, create);
    if (!error.isEmpty()) {
        return error;
    }
    DatabaseChoice::list(name, path);
    Q_EMIT databasesChanged();
    return {};
}

QString App::openDatabase(const QString &path)
{
    const bool local = path.isEmpty() || cleanedPath(path) == cleanedPath(Paths::defaultDataDir());
    const QString error = m_library->switchDatabase(local ? QString() : path, false);
    if (error.isEmpty()) {
        Config::self()->setActiveDatabase(local ? QString() : cleanedPath(path));
        Config::self()->save();
    }
    Q_EMIT databasesChanged();
    return error;
}

void App::forgetDatabase(const QString &path)
{
    Config *config = Config::self();
    QStringList paths = config->databasePaths();
    QStringList names = config->databaseNames();
    const qsizetype index = paths.indexOf(cleanedPath(path));
    if (index < 0) {
        return;
    }
    if (cleanedPath(m_library->databaseDir()) == paths.at(index)) {
        m_library->switchDatabase(QString());
    }
    if (config->activeDatabase() == paths.at(index)) {
        config->setActiveDatabase(QString());
    }
    paths.removeAt(index);
    if (index < names.size()) {
        names.removeAt(index);
    }
    config->setDatabasePaths(paths);
    config->setDatabaseNames(names);
    config->save();
    Q_EMIT databasesChanged();
}

QVariantMap App::toolbarItem(const QString &name)
{
    QString text;
    QString icon;
    if (previewBarActions.contains(name)) {
        return {};
    }
    if (name == "separator"_L1) {
        text = i18nc("@item a dividing line in the toolbar", "Separator");
        icon = u"distribute-horizontal-x"_s;
    } else if (name == "menu_add"_L1) {
        text = i18nc("@item toolbar button with a menu", "Add (menu)");
        icon = u"list-add"_s;
    } else if (name == "menu_new"_L1) {
        text = i18nc("@item toolbar button with a menu", "New (menu)");
        icon = u"document-new"_s;
    } else if (name == "menu_preview"_L1) {
        text = i18nc("@item toolbar button with a menu", "Preview (menu)");
        icon = u"view-preview"_s;
    } else if (name == "search_field"_L1) {
        text = i18nc("@item the field in the toolbar to type a search into", "Search Field");
        icon = u"search"_s;
    } else if (name == "spacer"_L1) {
        text = i18nc("@item empty room in the toolbar that takes up what is left over", "Flexible Space");
        icon = u"distribute-horizontal-gaps"_s;
    } else if (name == "doc_open"_L1) {
        // Its own text names the application for the selected document.
        text = i18nc("@item toolbar button", "Open File");
        icon = u"document-open-symbolic"_s;
    } else if (const QAction *found = action(name)) {
        text = KLocalizedString::removeAcceleratorMarker(found->text());
        icon = found->icon().name();
    } else {
        return {};
    }
    return {{u"name"_s, name}, {u"text"_s, text}, {u"icon"_s, icon}};
}

QVariantList App::toolbarCandidates()
{
    QVariantList result;
    for (const QString &special : {u"menu_add"_s, u"menu_new"_s, u"menu_preview"_s, u"search_field"_s}) {
        result.append(toolbarItem(special));
    }
    QVariantList actions;
    const QList<QAction *> all = mainCollection()->actions();
    for (const QAction *candidate : all) {
        // Those that have their fixed place in the toolbar already are left out, and the
        // command that goes to the search field, which is offered as the field itself.
        static const QStringList fixed = {u"view_left_sidebar"_s, u"view_right_sidebar"_s, u"focus_search"_s};
        const QString name = candidate->objectName();
        // The actions for single scripts come and go with the scripts; the toolbar takes
        // the lasting ones. Where a view of the panels is, is said at the view itself and
        // changes what the command is called, which is nothing for a button either.
        if (name.isEmpty() || fixed.contains(name) || previewBarActions.contains(name) || candidate->text().isEmpty() || name.startsWith(scriptActionPrefix)
            || name.startsWith("panel_move_"_L1) || name.startsWith("panel_split_"_L1) || name.startsWith("panel_earlier_"_L1) || name.startsWith("panel_later_"_L1)) {
            continue;
        }
        actions.append(toolbarItem(name));
    }
    std::sort(actions.begin(), actions.end(), [](const QVariant &a, const QVariant &b) {
        return QString::localeAwareCompare(a.toMap().value(u"text"_s).toString(), b.toMap().value(u"text"_s).toString()) < 0;
    });
    return result + actions;
}

QStringList App::defaultToolbar() const
{
    return Config::self()->defaultToolbarActionsValue();
}

QUrl App::locationFromInput(const QString &text) const
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }
    const QUrl url = QUrl::fromUserInput(trimmed, QDir::homePath(), QUrl::AssumeLocalFile);
    return url.isValid() && !url.scheme().isEmpty() ? url : QUrl();
}

#include "moc_app.cpp"
