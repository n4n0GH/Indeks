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
// preview in the middle, information about the selected document on the right.
StatefulApp.StatefulWindow {
    id: root

    windowName: "Main"
    application: App
    title: App.sidebar.currentLabel

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
        title: App.sidebar.currentLabel

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
                QQC2.SplitView.minimumWidth: Kirigami.Units.gridUnit * 18
                spacing: 0

                // The search form sits above what it finds.
                AdvancedSearchPanel {
                    Layout.fillWidth: true
                    visible: App.advancedSearch
                    actions: appActions
                    onCloseRequested: App.advancedSearch = false
                }
                Kirigami.Separator {
                    Layout.fillWidth: true
                    visible: App.advancedSearch
                }

                QQC2.SplitView {
                    id: centreSplit

                    orientation: Qt.Vertical
                    handle: PanelBorder {}
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    DocumentList {
                        id: documentList

                        actions: appActions
                        QQC2.SplitView.fillHeight: true
                        QQC2.SplitView.minimumHeight: Kirigami.Units.gridUnit * 7
                    }

                    PreviewPane {
                        id: previewPane

                        visible: Config.previewVisible
                        actions: appActions
                        QQC2.SplitView.preferredHeight: Kirigami.Units.gridUnit * 26
                        QQC2.SplitView.minimumHeight: Kirigami.Units.gridUnit * 8
                    }
                }
            }

            SidePanel {
                id: sidePanel

                visible: Config.rightSidebarVisible
                actions: appActions
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

            // Between file list and preview; only as wide as the two are.
            readonly property real splitTop: mainSplit.y + centreColumn.y + centreSplit.y
            readonly property real line: previewPane.y - documentList.y - documentList.height

            visible: previewPane.visible
            horizontalBorder: true
            border: splitTop + documentList.y + documentList.height
            x: mainSplit.x + centreColumn.x
            width: centreColumn.width
            onMoved: position => previewPane.QQC2.SplitView.preferredHeight = Math.max(previewPane.QQC2.SplitView.minimumHeight,
                Math.min(centreSplit.height - documentList.QQC2.SplitView.minimumHeight, splitTop + previewPane.y + previewPane.height - position - line))
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
                searchDialog.ask(0, "", root.currentSearchText());
                break;
            case "new_rule":
                ruleDialog.ask(0);
                break;
            case "scripts_manage":
            case "doc_run_script":
                // Running one means picking one, and the list is where they are.
                scriptsDialog.show();
                break;
            case "new_script":
                scriptEditor.create(null);
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
                toolBar.searchField.forceActiveFocus();
                toolBar.searchField.selectAll();
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
    function currentSearchText(): string {
        const scope = App.sidebar.currentKind === "place" || App.sidebar.currentKind === "smart" || App.sidebar.currentKind === "search" ? App.sidebar.currentQuery : "";
        return [scope, App.documents.effectiveFilter].filter(part => part.trim().length > 0).join(" ");
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
                               // The new document is selected and shown by now; start writing right away.
                               previewPane.startEditing();
                           }
                       });
    }

    function editSidebarItem(kind: string, itemId: real, label: string): void {
        if (kind === "group") {
            nameDialog.ask(i18nc("@title:window", "Rename Group"), i18nc("@info", "New name:"), label, name => App.library.renameGroup(itemId, name));
        } else if (kind === "tag") {
            nameDialog.ask(i18nc("@title:window", "Rename Tag"), i18nc("@info", "New name:"), label, name => App.library.renameTag(itemId, name));
        } else if (kind === "search") {
            searchDialog.ask(itemId, label, App.sidebar.currentQuery);
        } else if (kind === "rule") {
            ruleDialog.ask(itemId);
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
        } else if (kind === "rule") {
            confirmDialog.ask(i18nc("@title:window", "Delete Rule?"),
                              i18nc("@info", "The rule “%1” is deleted. What it did to documents so far stays.", label),
                              i18nc("@action:button", "Delete Rule"), "edit-delete", () => App.library.deleteRule(itemId));
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
    GroupDialog {
        id: groupDialog

        onNewGroupRequested: appActions.newGroup.trigger()
    }
    SearchDialog {
        id: searchDialog
    }
    RuleDialog {
        id: ruleDialog

        // A script written from inside a rule becomes an action of that rule.
        onScriptRequested: scriptEditor.create(file => ruleDialog.addAction({ type: "script", name: file }))
    }
    ScriptsDialog {
        id: scriptsDialog

        onCreateRequested: scriptEditor.create(null)
        onEditRequested: file => scriptEditor.edit(file)
        onOutputRequested: scriptOutput.show()
        onDeleteRequested: (file, name, rules) => confirmDialog.ask(
            i18nc("@title:window", "Move Script to the Trash?"),
            rules.length > 0
                ? i18ncp("@info", "The script “%2” is moved to the trash. The rule %3 runs it and will skip it from then on.",
                         "The script “%2” is moved to the trash. The rules %3 run it and will skip it from then on.", rules.length, name, rules.join(", "))
                : i18nc("@info", "The script “%1” is moved to the trash. What it did to documents so far stays.", name),
            i18nc("@action:button", "Move to Trash"), "edit-delete",
            () => App.library.deleteScript(file))
    }
    ScriptEditor {
        id: scriptEditor
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
