// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Dialogs as Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.settings as KirigamiSettings
import org.kde.kirigamiaddons.statefulapp as StatefulApp

// The main window: toolbar on top, the sidebar with places on the left, file browser and
// preview in the middle, information about the selected document on the right. While the
// sidebar shows its Automation tab, the middle lists rules or scripts instead of documents,
// and the one that is selected is changed where the preview is otherwise.
StatefulApp.StatefulWindow {
    id: root

    windowName: "Main"
    application: App

    /** What the middle of the window is about, for its title. */
    readonly property string placeLabel: !App.automation ? App.sidebar.currentLabel
        : App.automationPage === "rules" ? i18nc("@title:window", "Rules") : i18nc("@title:window", "Scripts")

    title: placeLabel

    // A rule or a script that was changed and not saved is asked about before the window
    // goes, however it was told to go.
    property bool closeConfirmed: false
    onClosing: close => {
        if (!closeConfirmed && automationPane.dirty) {
            close.accepted = false;
            root.quitWhenSaved();
        }
    }

    function quitWhenSaved(): void {
        if (automationPane.dirty) {
            // What the question is about is to be seen behind it.
            App.sidebarTab = "automation";
        }
        automationPane.guardedAll(() => {
            root.closeConfirmed = true;
            Qt.quit();
        });
    }

    minimumWidth: Kirigami.Units.gridUnit * 40
    minimumHeight: Kirigami.Units.gridUnit * 26
    width: Kirigami.Units.gridUnit * 84
    height: Kirigami.Units.gridUnit * 50

    pageStack.globalToolBar.style: Kirigami.ApplicationHeaderStyle.None
    pageStack.columnView.columnResizeMode: Kirigami.ColumnView.SingleColumn

    Actions {
        id: appActions
    }

    pageStack.initialPage: Kirigami.Page {
        id: mainPage

        padding: 0
        title: root.placeLabel

        header: MainToolBar {
            id: toolBar

            actions: appActions
        }

        QQC2.SplitView {
            id: mainSplit

            anchors.fill: parent
            orientation: Qt.Horizontal
            handle: PanelBorder {}

            Sidebar {
                id: sidebar

                visible: Config.leftSidebarVisible
                actions: appActions
                QQC2.SplitView.preferredWidth: Kirigami.Units.gridUnit * 14
                QQC2.SplitView.minimumWidth: Kirigami.Units.gridUnit * 9
                QQC2.SplitView.maximumWidth: Kirigami.Units.gridUnit * 28
            }

            ColumnLayout {
                id: centreColumn

                QQC2.SplitView.fillWidth: true
                // Room for list and preview side by side, where they stand so.
                QQC2.SplitView.minimumWidth: Kirigami.Units.gridUnit * (centreSplit.beside && paneStack.visible ? 19 : 18)
                spacing: 0

                // The search form sits above what it finds.
                AdvancedSearchPanel {
                    id: filterBar

                    Layout.fillWidth: true
                    visible: App.advancedSearch && !App.automation
                    actions: appActions
                    onCloseRequested: App.advancedSearch = false
                }
                Kirigami.Separator {
                    Layout.fillWidth: true
                    visible: App.advancedSearch && !App.automation
                }

                QQC2.SplitView {
                    id: centreSplit

                    /** The preview stands to the right of the list, not below it. */
                    readonly property bool beside: Config.previewBeside

                    // Of the sizes below, the split view looks at those of the direction it
                    // lies in; list and preview carry both, and keep what was dragged in one
                    // direction while they stand in the other.
                    orientation: beside ? Qt.Horizontal : Qt.Vertical
                    handle: PanelBorder {}
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    // Two things share each half, one at a time: the documents and their
                    // preview, or the rules and scripts and where they are changed. The
                    // sizes belong to the halves, so that the border stays where it was
                    // dragged to whichever is shown.
                    StackLayout {
                        id: listStack

                        currentIndex: App.automation ? 1 : 0
                        QQC2.SplitView.fillHeight: true
                        QQC2.SplitView.fillWidth: true
                        QQC2.SplitView.minimumHeight: Kirigami.Units.gridUnit * 7
                        QQC2.SplitView.minimumWidth: Kirigami.Units.gridUnit * 8

                        DocumentList {
                            id: documentList

                            actions: appActions
                        }
                        AutomationList {
                            id: automationList

                            actions: appActions
                            onRuleRequested: id => automationPane.editRule(id)
                            onScriptRequested: file => automationPane.editScript(file)
                            onActivated: root.showEditor()
                        }
                    }

                    StackLayout {
                        id: paneStack

                        currentIndex: App.automation ? 1 : 0
                        visible: Config.previewVisible
                        QQC2.SplitView.preferredHeight: Kirigami.Units.gridUnit * 26
                        QQC2.SplitView.minimumHeight: Kirigami.Units.gridUnit * 8
                        // Half of the room, until the border is dragged.
                        QQC2.SplitView.preferredWidth: Math.round(centreSplit.width / 2)
                        QQC2.SplitView.minimumWidth: Kirigami.Units.gridUnit * 10

                        PreviewPane {
                            id: previewPane

                            actions: appActions
                            findText: sidePanel.findText
                        }
                        AutomationPane {
                            id: automationPane

                            actions: appActions
                        }
                    }
                }
            }

            SidePanel {
                id: sidePanel

                visible: Config.rightSidebarVisible
                actions: appActions
                preview: previewPane
                onNoteActivated: note => previewPane.showNote(note)
                onTodoEditRequested: id => todoDialog.edit(id)
                QQC2.SplitView.preferredWidth: Kirigami.Units.gridUnit * 16
                QQC2.SplitView.minimumWidth: Kirigami.Units.gridUnit * 11
                QQC2.SplitView.maximumWidth: Kirigami.Units.gridUnit * 30
            }
        }

        // Zones to grab the borders between the panels by; see ResizeGrip.
        ResizeGrip {
            id: sidebarGrip

            visible: sidebar.visible
            border: mainSplit.x + sidebar.x + sidebar.width
            onMoved: position => sidebar.QQC2.SplitView.preferredWidth = Math.max(sidebar.QQC2.SplitView.minimumWidth,
                Math.min(sidebar.QQC2.SplitView.maximumWidth, position - mainSplit.x - sidebar.x))
        }
        ResizeGrip {
            id: infoGrip

            // The line between the two panels, which this begins at.
            readonly property real line: sidePanel.x - centreColumn.x - centreColumn.width

            visible: sidePanel.visible
            border: mainSplit.x + centreColumn.x + centreColumn.width
            onMoved: position => sidePanel.QQC2.SplitView.preferredWidth = Math.max(sidePanel.QQC2.SplitView.minimumWidth,
                Math.min(sidePanel.QQC2.SplitView.maximumWidth, mainSplit.x + sidePanel.x + sidePanel.width - position - line))
        }
        ResizeGrip {
            id: previewGrip

            // Between file list and preview, whichever way the two stand; only as long as
            // they are.
            readonly property bool beside: centreSplit.beside
            readonly property real splitTop: mainSplit.y + centreColumn.y + centreSplit.y
            readonly property real splitLeft: mainSplit.x + centreColumn.x + centreSplit.x
            readonly property real line: beside ? paneStack.x - listStack.x - listStack.width : paneStack.y - listStack.y - listStack.height

            visible: paneStack.visible
            horizontalBorder: !beside
            border: beside ? splitLeft + listStack.x + listStack.width : splitTop + listStack.y + listStack.height
            x: beside ? border : splitLeft
            y: beside ? splitTop : border
            width: beside ? reach : centreSplit.width
            height: beside ? centreSplit.height : reach
            onMoved: position => {
                if (beside) {
                    paneStack.QQC2.SplitView.preferredWidth = Math.max(paneStack.QQC2.SplitView.minimumWidth,
                        Math.min(centreSplit.width - listStack.QQC2.SplitView.minimumWidth, splitLeft + paneStack.x + paneStack.width - position - line));
                } else {
                    paneStack.QQC2.SplitView.preferredHeight = Math.max(paneStack.QQC2.SplitView.minimumHeight,
                        Math.min(centreSplit.height - listStack.QQC2.SplitView.minimumHeight, splitTop + paneStack.y + paneStack.height - position - line));
                }
            }
        }
    }

    // Requests for everything that needs a dialog or the preview arrive here, whether they
    // were triggered from the toolbar, a menu, a shortcut or the command bar.
    Connections {
        target: App

        function onRequest(what: string): void {
            const ids = App.documents.selectedIds;
            const currentId = App.documents.currentId;
            const kind = App.sidebar.currentKind;
            const itemId = App.sidebar.currentId;
            const label = App.sidebar.currentLabel;

            switch (what) {
            case "add_folder":
                folderDialog.open();
                break;
            case "add_remote":
                locationDialog.ask();
                break;
            case "new_text":
                root.askForNewDocument("text", i18nc("@title:window", "New Plain Text Document"));
                break;
            case "new_markdown":
                root.askForNewDocument("markdown", i18nc("@title:window", "New Markdown Document"));
                break;
            case "new_richtext":
                root.askForNewDocument("html", i18nc("@title:window", "New Formatted Document"));
                break;
            case "new_clipboard":
                clipboardDialog.ask();
                break;
            case "doc_rename":
                nameDialog.ask(i18nc("@title:window", "Rename Document"), i18nc("@info", "New name:"), App.library.documentInfo(currentId).name,
                               name => App.library.renameDocument(currentId, name));
                break;
            case "doc_tag":
                tagDialog.ask(ids);
                break;
            case "doc_group":
                groupDialog.ask(ids);
                break;
            case "doc_note":
                noteDialog.ask(currentId, App.library.documentInfo(currentId).name);
                break;
            case "doc_links":
                linkDialog.ask(currentId);
                break;
            case "new_todo":
                todoDialog.ask(0);
                break;
            case "doc_todo":
                todoDialog.ask(currentId);
                break;
            case "doc_edit":
                if (!Config.previewVisible) {
                    appActions.preview.trigger();
                }
                previewPane.startEditing();
                break;
            case "doc_edit_done":
                previewPane.finishEditing();
                break;
            case "view_zoom_in":
                previewPane.zoomIn();
                break;
            case "view_zoom_out":
                previewPane.zoomOut();
                break;
            case "view_zoom_reset":
                previewPane.zoomReset();
                break;
            case "view_raw":
                previewPane.toggleRaw();
                break;
            case "doc_find":
                // The panel was just brought up: its field takes the keys once it is there.
                Qt.callLater(sidePanel.focusFind);
                break;
            case "find_next":
                root.findStep(true);
                break;
            case "find_previous":
                root.findStep(false);
                break;
            case "doc_clear_drawing":
                confirmDialog.ask(i18nc("@title:window", "Remove All Drawings?"),
                                  i18nc("@info", "Everything drawn on “%1” will be removed. The document itself is not affected.", App.library.documentInfo(currentId).name),
                                  i18nc("@action:button", "Remove Drawings"), "edit-clear",
                                  () => {
                                      App.library.clearInk(currentId);
                                      previewPane.reload();
                                  });
                break;
            case "empty_trash":
                confirmDialog.ask(i18nc("@title:window", "Empty Trash?"),
                                  i18nc("@info", "The documents in the trash are removed from Indeks together with their tags, notes and drawings. Files that Indeks only indexed stay where they are; documents created in Indeks move to the system's trash."),
                                  i18nc("@action:button", "Empty Trash"), "trash-empty",
                                  () => App.library.emptyTrash());
                break;
            case "new_group":
                nameDialog.ask(kind === "group" ? i18nc("@title:window", "New Group in “%1”", label) : i18nc("@title:window", "New Group"),
                               i18nc("@info", "Name of the group:"), "",
                               name => {
                                   const id = App.library.createGroup(name, kind === "group" ? itemId : 0);
                                   if (id > 0 && !groupDialog.visible) {
                                       App.goTo("group:" + id);
                                   }
                               });
                break;
            case "new_tag":
                nameDialog.ask(i18nc("@title:window", "New Tag"), i18nc("@info", "Name of the tag:"), "",
                               name => {
                                   const id = App.library.createTag(name);
                                   if (id > 0) {
                                       App.goTo("tag:" + id);
                                   }
                               });
                break;
            case "save_search":
                searchDialog.askNew(root.currentSearchPieces(), filterBar.visible ? filterBar.filters : []);
                break;
            case "go_rules":
                App.automationPage = "rules";
                App.sidebarTab = "automation";
                break;
            case "scripts_manage":
                App.automationPage = "scripts";
                App.sidebarTab = "automation";
                break;
            case "new_rule":
                App.automationPage = "rules";
                App.sidebarTab = "automation";
                root.showEditor();
                automationPane.editRule(0);
                break;
            case "new_script":
                App.automationPage = "scripts";
                App.sidebarTab = "automation";
                root.showEditor();
                automationPane.editScript("");
                break;
            case "automation_delete":
                root.deleteAutomationItem();
                break;
            case "file_quit":
                root.quitWhenSaved();
                break;
            case "doc_run_script":
                runScriptDialog.show();
                break;
            case "scripts_output":
                scriptOutput.show();
                break;
            case "edit_sidebar_item":
                root.editSidebarItem(kind, itemId, label);
                break;
            case "delete_sidebar_item":
                root.deleteSidebarItem(kind, itemId, label);
                break;
            case "focus_search":
                if (!toolBar.focusSearch()) {
                    // A toolbar without the search field: the filter bar finds as well.
                    App.advancedSearch = true;
                }
                break;
            case "view_activity":
                activityDialog.open();
                break;
            case "view_columns":
                documentList.showColumnMenu();
                break;
            case "db_manage":
                databaseDialog.ask();
                break;
            case "db_add":
                addDatabaseDialog.ask();
                break;
            case "db_new":
                newDatabaseDialog.ask();
                break;
            case "db_rename":
                // The one that is open.
                root.askForDatabaseName(App.databaseName, App.library.databaseDir);
                break;
            case "options_configure":
                configurationView.open();
                break;
            case "options_configure_toolbars":
                configurationView.open("toolbar");
                break;
            case "open_kcommand_bar":
                commandBar.show();
                break;
            }
        }

        function onPassiveMessage(text: string): void {
            root.showPassiveNotification(text);
        }

        function onScriptMessage(text: string): void {
            // With the way to what the script printed and where it failed.
            root.showPassiveNotification(text, "long", i18nc("@action:button", "Show Output"), () => scriptOutput.show());
        }

        function onRaiseWindow(): void {
            root.show();
            root.raise();
            root.requestActivate();
        }
    }

    // The search to save: what the current place stands for plus what is typed in the search field.
    // To the next or the previous place the search within the document found. Without that
    // search in view there is nothing to step through: it is brought up instead.
    function findStep(forward: bool): void {
        if (sidePanel.findText.length === 0) {
            App.action("doc_find").trigger();
        } else if (forward) {
            previewPane.findNext();
        } else {
            previewPane.findPrevious();
        }
    }

    // What narrows the list down besides the filter bar, as pieces of the query language:
    // the place that is selected in the sidebar, and what is typed in the search field.
    function currentSearchPieces(): list<string> {
        const scope = App.sidebar.currentKind === "place" || App.sidebar.currentKind === "smart" || App.sidebar.currentKind === "search" ? App.sidebar.currentQuery : "";
        return [scope, App.documents.filter].filter(part => part.trim().length > 0);
    }

    // The name is this computer's word for the database; its folder stays what it is.
    function askForDatabaseName(name: string, path: string): void {
        nameDialog.ask(i18nc("@title:window", "Rename Database"),
                       i18nc("@info", "New name for “%1”. The folder of the database keeps its name.", name), name,
                       newName => App.renameDatabase(path, newName));
    }

    function askForNewDocument(format: string, title: string): void {
        nameDialog.ask(title, i18nc("@info", "Name of the document. It is stored in Indeks and starts out in the inbox."), i18nc("default name of a new document", "Untitled"),
                       name => {
                           const id = App.library.createDocument(name, format, "");
                           if (id > 0) {
                               if (!Config.previewVisible) {
                                   appActions.preview.trigger();
                               }
                               // Start writing right away, as soon as the new document is shown.
                               previewPane.editOnceShown(id);
                           }
                       });
    }

    function editSidebarItem(kind: string, itemId: real, label: string): void {
        if (kind === "group") {
            nameDialog.ask(i18nc("@title:window", "Rename Group"), i18nc("@info", "New name:"), label, name => App.library.renameGroup(itemId, name));
        } else if (kind === "tag") {
            nameDialog.ask(i18nc("@title:window", "Rename Tag"), i18nc("@info", "New name:"), label, name => App.library.renameTag(itemId, name));
        } else if (kind === "search") {
            searchDialog.edit(itemId, label, App.sidebar.currentQuery);
        }
    }

    // The pane in which rules and scripts are changed is where the preview is, and hidden
    // with it.
    function showEditor(): void {
        if (!Config.previewVisible) {
            appActions.preview.trigger();
        }
    }

    // The rule or the script that is selected in its list.
    function deleteAutomationItem(): void {
        if (App.automationPage === "rules") {
            const rule = App.library.rule(App.currentRule);
            if (rule.id === undefined) {
                return;
            }
            confirmDialog.ask(i18nc("@title:window", "Delete Rule?"),
                              i18nc("@info", "The rule “%1” is deleted. What it did to documents so far stays.", rule.name),
                              i18nc("@action:button", "Delete Rule"), "edit-delete", () => App.library.deleteRule(rule.id));
        } else {
            const script = App.library.scripts().find(entry => entry.file === App.currentScript);
            if (!script) {
                return;
            }
            const rules = script.rules;
            confirmDialog.ask(
                i18nc("@title:window", "Move Script to the Trash?"),
                rules.length > 0
                    ? i18ncp("@info", "The script “%2” is moved to the trash. The rule %3 runs it and will skip it from then on.",
                             "The script “%2” is moved to the trash. The rules %3 run it and will skip it from then on.", rules.length, script.name, rules.join(", "))
                    : i18nc("@info", "The script “%1” is moved to the trash. What it did to documents so far stays.", script.name),
                i18nc("@action:button", "Move to Trash"), "edit-delete",
                () => App.library.deleteScript(script.file));
        }
    }

    function deleteSidebarItem(kind: string, itemId: real, label: string): void {
        if (kind === "group") {
            confirmDialog.ask(i18nc("@title:window", "Delete Group?"),
                              i18nc("@info", "The group “%1” and its subgroups are deleted. The documents in them stay in Indeks.", label),
                              i18nc("@action:button", "Delete Group"), "edit-delete", () => App.library.deleteGroup(itemId));
        } else if (kind === "tag") {
            confirmDialog.ask(i18nc("@title:window", "Delete Tag?"),
                              i18nc("@info", "The tag “%1” is removed from all documents.", label),
                              i18nc("@action:button", "Delete Tag"), "edit-delete", () => App.library.deleteTag(itemId));
        } else if (kind === "search") {
            confirmDialog.ask(i18nc("@title:window", "Delete Saved Search?"),
                              i18nc("@info", "The saved search “%1” is deleted. No documents are affected.", label),
                              i18nc("@action:button", "Delete Search"), "edit-delete", () => App.library.deleteSearch(itemId));
        } else if (kind === "source") {
            confirmDialog.ask(i18nc("@title:window", "Stop Indexing This Location?"),
                              i18nc("@info", "“%1” will no longer be indexed. Its documents leave Indeks, except those you tagged, grouped or annotated. The files themselves are not touched.", label),
                              i18nc("@action:button", "Stop Indexing"), "list-remove", () => App.library.removeSource(itemId));
        }
    }

    Dialogs.FolderDialog {
        id: folderDialog

        title: i18nc("@title:window", "Choose a Folder to Index")
        onAccepted: {
            const id = App.library.addSource(selectedFolder, "", true);
            if (id > 0) {
                App.goTo("source:" + id);
            }
        }
    }

    ActivityDialog {
        id: activityDialog
    }
    DatabaseDialog {
        id: databaseDialog

        onAddRequested: {
            databaseDialog.close();
            addDatabaseDialog.ask();
        }
        onNewRequested: {
            databaseDialog.close();
            newDatabaseDialog.ask();
        }
        onRenameRequested: (name, path) => root.askForDatabaseName(name, path)
        onForgetRequested: (name, path) => confirmDialog.ask(
            i18nc("@title:window", "Remove Database from the List?"),
            i18nc("@info", "Indeks will no longer list “%1”. The folder %2 and everything in it stay as they are, and the database can be added again at any time.", name, path),
            i18nc("@action:button", "Remove from List"), "list-remove",
            () => App.forgetDatabase(path))
    }
    AddDatabaseDialog {
        id: addDatabaseDialog
    }
    NewDatabaseDialog {
        id: newDatabaseDialog
    }
    NameDialog {
        id: nameDialog
    }
    ConfirmDialog {
        id: confirmDialog
    }
    TagDialog {
        id: tagDialog
    }
    NoteDialog {
        id: noteDialog
    }
    TodoDialog {
        id: todoDialog

        // What was just written down is shown where it now stands.
        onAdded: App.showSidePanel("todo")
    }
    LinkDialog {
        id: linkDialog
    }
    ClipboardDialog {
        id: clipboardDialog
    }
    GroupDialog {
        id: groupDialog

        onNewGroupRequested: appActions.newGroup.trigger()
    }
    SearchDialog {
        id: searchDialog
    }
    RunScriptDialog {
        id: runScriptDialog

        actions: appActions
    }
    ScriptOutputDialog {
        id: scriptOutput
    }
    LocationDialog {
        id: locationDialog
    }
    CommandBar {
        id: commandBar
    }

    KirigamiSettings.ConfigurationView {
        id: configurationView

        window: root
        modules: [
            KirigamiSettings.ConfigurationModule {
                moduleId: "locations"
                text: i18nc("@action:button", "Indexed Locations")
                icon.name: "folder-saved-search"
                page: () => Qt.createComponent("io.github.n4n0gh.indeks", "LocationsSettingsPage")
            },
            KirigamiSettings.ConfigurationModule {
                moduleId: "indexing"
                text: i18nc("@action:button", "Indexing")
                icon.name: "search"
                page: () => Qt.createComponent("io.github.n4n0gh.indeks", "IndexingSettingsPage")
            },
            KirigamiSettings.ConfigurationModule {
                moduleId: "ocr"
                text: i18nc("@action:button", "Text Recognition")
                icon.name: "document-scan"
                page: () => Qt.createComponent("io.github.n4n0gh.indeks", "OcrSettingsPage")
            },
            KirigamiSettings.ConfigurationModule {
                moduleId: "todos"
                text: i18nc("@action:button", "To-Do List")
                icon.name: "view-task"
                page: () => Qt.createComponent("io.github.n4n0gh.indeks", "TodoSettingsPage")
            },
            KirigamiSettings.ConfigurationModule {
                moduleId: "toolbar"
                text: i18nc("@action:button", "Toolbar")
                icon.name: "configure-toolbars"
                page: () => Qt.createComponent("io.github.n4n0gh.indeks", "ToolbarSettingsPage")
            },
            KirigamiSettings.ConfigurationModule {
                moduleId: "about"
                text: i18nc("@action:button", "About Indeks")
                icon.name: "help-about"
                page: () => Qt.createComponent("org.kde.kirigamiaddons.formcard", "AboutPage")
                category: i18nc("@title:group", "About")
            }
        ]
    }
}
