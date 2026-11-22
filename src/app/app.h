// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "activitymodel.h"
#include "documentmodel.h"
#include "library.h"
#include "sidebarmodel.h"

#include <AbstractKirigamiApplication>

#include <QHash>
#include <QKeySequence>
#include <QUrl>

class QQmlEngine;
class QJSEngine;

/**
 * The application object. It owns the library and the models and defines every action Indeks
 * offers. Toolbar, menus and the command bar (Ctrl+Alt+I) all show these same actions, so a
 * function added here is reachable everywhere.
 *
 * Actions that need a dialog or act on the preview are forwarded to the window with request().
 */
class App : public AbstractKirigamiApplication
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(Indeks::Library *library READ library CONSTANT)
    Q_PROPERTY(DocumentModel *documents READ documents CONSTANT)
    Q_PROPERTY(SidebarModel *sidebar READ sidebar CONSTANT)
    Q_PROPERTY(ActivityModel *activity READ activity CONSTANT)
    /** Name of the database that is open. */
    Q_PROPERTY(QString databaseName READ databaseName NOTIFY databasesChanged)
    Q_PROPERTY(bool annotating READ isAnnotating WRITE setAnnotating NOTIFY annotatingChanged)
    /** Whether the form for searching by name, content and properties is shown. */
    Q_PROPERTY(bool advancedSearch READ isAdvancedSearch WRITE setAdvancedSearch NOTIFY advancedSearchChanged)
    /** Whether the preview has a text open for editing. Set by the window. */
    Q_PROPERTY(bool textEditing READ isTextEditing WRITE setTextEditing NOTIFY textEditingChanged)
    /** Whether what the preview shows can be made larger and smaller. Set by the window. */
    Q_PROPERTY(bool previewZoomable READ isPreviewZoomable WRITE setPreviewZoomable NOTIFY previewChanged)
    /**
     * Whether the preview shows something laid out for reading that it can also show as it
     * is in the file (formatted text, a table, indented JSON), and whether it does the
     * latter right now. Set by the window.
     */
    Q_PROPERTY(bool rawViewAvailable READ isRawViewAvailable WRITE setRawViewAvailable NOTIFY previewChanged)
    Q_PROPERTY(bool rawView READ isRawView WRITE setRawView NOTIFY previewChanged)
public:
    // No default argument on purpose: a singleton type that can be default-constructed makes
    // the QML engine build its own second instance instead of asking create() for this one.
    explicit App(QObject *parent);
    ~App() override;

    static App *instance();
    static App *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    Indeks::Library *library() const
    {
        return m_library;
    }
    DocumentModel *documents() const
    {
        return m_documents;
    }
    SidebarModel *sidebar() const
    {
        return m_sidebar;
    }
    ActivityModel *activity() const
    {
        return m_activity;
    }
    bool isAnnotating() const
    {
        return m_annotating;
    }
    void setAnnotating(bool annotating);
    bool isAdvancedSearch() const
    {
        return m_advancedSearch;
    }
    void setAdvancedSearch(bool shown);
    bool isTextEditing() const
    {
        return m_textEditing;
    }
    void setTextEditing(bool editing);
    bool isPreviewZoomable() const
    {
        return m_previewZoomable;
    }
    void setPreviewZoomable(bool zoomable);
    bool isRawViewAvailable() const
    {
        return m_rawViewAvailable;
    }
    void setRawViewAvailable(bool available);
    bool isRawView() const
    {
        return m_rawView;
    }
    void setRawView(bool raw);

    /**
     * Makes folders indexed locations, e.g. dropped ones or those sent from Dolphin, and
     * shows the last of them. Single files are not taken; see Library::addFolders().
     */
    Q_INVOKABLE void addFolders(const QList<QUrl> &urls);
    /** Handles the command line of this or a second instance. */
    void handleArguments(const QStringList &arguments, const QString &workingDirectory);

    Q_INVOKABLE void openExternally(qint64 documentId);
    /** Lets the user pick the application to open the documents with. */
    Q_INVOKABLE void openWith(const QVariantList &documentIds);
    Q_INVOKABLE void showInFileManager(qint64 documentId);
    Q_INVOKABLE void copyText(const QString &text);
    /**
     * The file of an icon in the size the sidebar and the menus show it in, which is a plain
     * line drawing. For showing that same drawing large: asked for at a large size, the icon
     * theme has pictures in colour for some icons and not for others. Empty if there is none.
     */
    Q_INVOKABLE QUrl lineIcon(const QString &name) const;
    /** Passes the settings on to the indexer and saves them. */
    Q_INVOKABLE void applySettings();
    QString databaseName() const;
    /**
     * The databases Indeks knows: the local one first, then those that were added. Maps with
     * `name`, `path`, `active`, `builtin`, `reachable`, `network` and `problem` (why it cannot
     * be opened right now; empty if it can).
     */
    Q_INVOKABLE QVariantList databases() const;
    /**
     * What adding the database whose index is @p file would do. A database is added by its
     * index file and in no other way: the file is what shows that a database is there.
     * `path` (the folder of the database), `valid` (there is something to say about it),
     * `network`, `known` (already in the list) and `problem`.
     */
    Q_INVOKABLE QVariantMap inspectDatabaseFile(const QString &file) const;
    /** Adds the database whose index is @p file and opens it. Returns what went wrong, if anything. */
    Q_INVOKABLE QString addDatabase(const QString &name, const QString &file);
    /**
     * What starting a new database in @p folder would do: `path`, `valid`, `empty` (nothing
     * is in the folder; the user is warned if something is), `network`, `known` and
     * `problem`. A folder that holds a database already is a problem here: that one is added.
     */
    Q_INVOKABLE QVariantMap inspectNewDatabaseFolder(const QString &folder) const;
    /** Starts a database in @p folder and opens it. Returns what went wrong, if anything. */
    Q_INVOKABLE QString createDatabase(const QString &name, const QString &folder);
    /** Opens one of the known databases; an empty @p path is the local one. Returns what went wrong, if anything. */
    Q_INVOKABLE QString openDatabase(const QString &path);
    /**
     * Gives a database another name; an empty @p path is the local one. The name is how this
     * computer lists the database and nothing more: the folder keeps its name, and so does
     * the database on other computers. Returns false if there is no such database or no name.
     */
    Q_INVOKABLE bool renameDatabase(const QString &path, const QString &name);
    /** Takes a database off the list. Its folder and everything in it stay as they are. */
    Q_INVOKABLE void forgetDatabase(const QString &path);
    /** A local path for what a file or folder dialog or the user's typing gave. */
    Q_INVOKABLE QString localPath(const QString &input) const;

    /**
     * Name, text and icon of something the toolbar can show: an action, "separator", or one of
     * the menu buttons "menu_add" and "menu_new". Empty if there is no such thing.
     */
    Q_INVOKABLE QVariantMap toolbarItem(const QString &name);
    /** Everything that can be put into the toolbar, the menu buttons first. */
    Q_INVOKABLE QVariantList toolbarCandidates();
    Q_INVOKABLE QStringList defaultToolbar() const;
    /** Turns what the user typed as a location into a URL; invalid if it makes no sense. */
    Q_INVOKABLE QUrl locationFromInput(const QString &text) const;
    /** Brings a document into view in the file browser and selects it. */
    Q_INVOKABLE void showDocument(qint64 documentId);
    /** Opens the manual in the application for PDF documents. */
    Q_INVOKABLE void openManual();
    /** Opens the panel on the right at "info" about the document or at the "todo" list. */
    Q_INVOKABLE void showSidePanel(const QString &tab);
    /** Selects an entry of the sidebar, e.g. "place:inbox". */
    Q_INVOKABLE void goTo(const QString &sidebarKey);
    /** Runs one of the scripts of the database on the selected documents. */
    Q_INVOKABLE void runScript(const QString &file);
    /** Shows the folder the scripts are kept in, in the file manager. */
    Q_INVOKABLE void openScriptsFolder();

Q_SIGNALS:
    /** The window is asked to do something that needs a dialog or the preview. */
    void request(const QString &what);
    void passiveMessage(const QString &text);
    /** A script ran and there is something to say about it; the output has the details. */
    void scriptMessage(const QString &text);
    void raiseWindow();
    void annotatingChanged();
    void advancedSearchChanged();
    void databasesChanged();
    void textEditingChanged();
    void previewChanged();

protected:
    void setupActions() override;

private:
    QAction *addAction(const QString &name, const QString &text, const QString &icon, const QKeySequence &shortcut, const std::function<void()> &handler);
    QAction *addRequest(const QString &name, const QString &text, const QString &icon, const QKeySequence &shortcut = QKeySequence());
    QAction *addToggle(const QString &name, const QString &text, const QString &icon, const QKeySequence &shortcut, bool checked, const std::function<void(bool)> &handler);
    void updateActions();
    QVariantList selection() const;
    QString localDatabaseName() const;
    /** Opens the database in @p path, starting it if @p create, and puts it on the list. */
    QString openAndListDatabase(const QString &name, const QString &path, bool create);
    /** Keeps one action per script, so that each can be run from the command bar. */
    void syncScriptActions();
    void reportScript(const QVariantMap &report);

    Indeks::Library *m_library = nullptr;
    DocumentModel *m_documents = nullptr;
    SidebarModel *m_sidebar = nullptr;
    ActivityModel *m_activity = nullptr;
    /** What the Edit action does with the selected document. */
    enum class EditMode {
        None,
        Text,
        Draw,
    };
    void syncEditAction();

    bool m_annotating = false;
    bool m_advancedSearch = false;
    bool m_textEditing = false;
    bool m_previewZoomable = false;
    bool m_rawViewAvailable = false;
    bool m_rawView = false;
    EditMode m_editMode = EditMode::None;
    /** When a script run for a rule was last complained about, by script. */
    QHash<QString, qint64> m_scriptComplaints;
};
