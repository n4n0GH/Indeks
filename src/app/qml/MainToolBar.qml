// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The toolbar across the top of the window: the buttons for the sidebar and the side panel at
// its ends, the menu with everything, and in between what the user chose (see
// ToolbarSettingsPage): buttons, the search field, and a flexible space that sends what
// follows it to the far end.
QQC2.ToolBar {
    id: root

    required property Actions actions
    /** The search field the toolbar made last. It can outlive its place in the toolbar. */
    property Kirigami.SearchField searchField: null

    /**
     * Puts the cursor into the search field. False if there is none to be seen: it was taken
     * off the toolbar, or the window is too narrow for it. That the field exists does not
     * say so, since the toolbar keeps what it once made.
     */
    function focusSearch(): bool {
        const placed = chosen.includes(searchAction) || chosenAtEnd.includes(searchAction);
        if (!placed || !searchField || !searchField.visible) {
            return false;
        }
        searchField.forceActiveFocus();
        searchField.selectAll();
        return true;
    }

    // What Config.toolbarActions names, as action objects: what stands at the near end, and
    // what the flexible space sends to the far end.
    property list<Kirigami.Action> chosen
    property list<Kirigami.Action> chosenAtEnd
    property list<Kirigami.Action> separators

    function rebuild(): void {
        const outdated = Array.from(separators);
        const dividers = [];
        const start = [];
        const end = [];
        let list = start;
        for (const name of Config.toolbarActions) {
            let action = null;
            if (name === "spacer") {
                // Only the first one: there is one far end.
                list = end;
                continue;
            } else if (name === "separator") {
                action = separatorComponent.createObject(root);
                dividers.push(action);
            } else if (name === "menu_add") {
                action = addMenu;
            } else if (name === "menu_new") {
                action = newMenu;
            } else if (name === "menu_preview") {
                action = previewMenu;
            } else if (name === "search_field") {
                action = searchAction;
            } else {
                action = actions.named(name);
            }
            if (action && !start.includes(action) && !end.includes(action)) {
                list.push(action);
            }
        }
        chosen = start;
        chosenAtEnd = end;
        separators = dividers;
        // Only now that the toolbar has let go of them.
        for (const old of outdated) {
            old.destroy();
        }
    }

    Component.onCompleted: rebuild()

    Connections {
        target: Config
        function onToolbarActionsChanged(): void {
            root.rebuild();
        }
    }

    Component {
        id: separatorComponent

        Kirigami.Action {
            separator: true
        }
    }

    // The search field, as something the toolbar places like a button.
    Kirigami.Action {
        id: searchAction

        text: i18nc("@action the search field, where the toolbar lists it by name", "Search Field")
        icon.name: "search"
        // Stays in view when the window is narrow and buttons move into the menu.
        displayHint: Kirigami.DisplayHint.KeepVisible
        displayComponent: Kirigami.SearchField {
            id: searchField

            implicitWidth: Kirigami.Units.gridUnit * 16
            // The toolbar makes this anew whenever its buttons change; what is being
            // searched for is still the same then.
            Component.onCompleted: {
                text = App.documents.filter;
                root.searchField = searchField;
            }
            Component.onDestruction: {
                if (root.searchField === searchField) {
                    root.searchField = null;
                }
            }
            placeholderText: i18nc("@info:placeholder", "Search in %1…", App.sidebar.currentLabel)
            // The search starts by itself a moment after the last key. A single letter is
            // found in nearly every document and takes the longest to look up, so that one
            // waits for a real pause, or for Enter.
            autoAccept: false
            onAccepted: {
                typingPause.stop();
                App.documents.filter = text;
            }
            onTextChanged: {
                if (text.length === 0) {
                    // Clearing the field takes effect at once.
                    typingPause.stop();
                    App.documents.filter = "";
                } else if (text !== App.documents.filter) {
                    typingPause.interval = text.trim().length < 2 ? Kirigami.Units.humanMoment / 2 : Kirigami.Units.humanMoment / 8;
                    typingPause.restart();
                }
            }

            Timer {
                id: typingPause

                onTriggered: App.documents.filter = searchField.text
            }

            // The search can also be reset from elsewhere, e.g. when jumping to a document.
            Connections {
                target: App.documents
                function onFilterChanged(): void {
                    if (searchField.text !== App.documents.filter) {
                        searchField.text = App.documents.filter;
                    }
                }
            }

            QQC2.ToolTip.text: i18nc("@info:tooltip", "Words find file names, content and notes. Filters narrow down: tag:name, kind:pdf, ext:md, added:7d, is:annotated … The filter button has a form for all of them.")
            QQC2.ToolTip.visible: hovered && !activeFocus
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
        }
    }

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: toolbarMenu.popup()
    }

    QQC2.Menu {
        id: toolbarMenu

        QQC2.MenuItem {
            action: root.actions.configureToolbar
        }
    }

    contentItem: RowLayout {
        spacing: Kirigami.Units.mediumSpacing

        QQC2.ToolButton {
            action: root.actions.leftSidebar
            display: QQC2.AbstractButton.IconOnly
            QQC2.ToolTip.text: text
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
        }

        Kirigami.ActionToolBar {
            Layout.fillWidth: true
            actions: root.chosen
            display: Config.toolbarIconsOnly ? QQC2.AbstractButton.IconOnly : QQC2.AbstractButton.TextBesideIcon

            Kirigami.Action {
                id: addMenu

                text: i18nc("@action:intoolbar", "Add")
                icon.name: "list-add"
                children: [root.actions.addFolder, root.actions.addRemote, addMenuSeparator, root.actions.addDatabase, root.actions.newDatabase]
            }
            Kirigami.Action {
                id: newMenu

                text: i18nc("@action:intoolbar", "New")
                icon.name: "document-new"
                children: [
                    root.actions.newText,
                    root.actions.newMarkdown,
                    root.actions.newRichText,
                    root.actions.newFromClipboard,
                    newMenuSeparator,
                    root.actions.newGroup,
                    root.actions.newTag,
                    root.actions.newRule,
                    root.actions.newScript,
                    newMenuSeparator2,
                    root.actions.newTodo
                ]
            }
            // Whether there is a preview, and where: one choice of three, in a menu like
            // those of Add and New. The icon of the button shows which one holds.
            Kirigami.Action {
                id: previewMenu

                text: i18nc("@action:intoolbar", "Preview")
                tooltip: i18nc("@info:tooltip", "Whether the selected document is shown, and where")
                icon.name: !Config.previewVisible ? "view-preview" : Config.previewBeside ? "view-split-left-right" : "view-split-top-bottom"
                children: [root.actions.previewNone, root.actions.previewBelow, root.actions.previewBesideDocuments]
            }
            Kirigami.Action {
                id: newMenuSeparator2

                separator: true
            }
            Kirigami.Action {
                id: newMenuSeparator

                separator: true
            }
            Kirigami.Action {
                id: addMenuSeparator

                separator: true
            }
        }

        Kirigami.ActionToolBar {
            // What the flexible space sent here. As wide as it needs to be; what does not fit
            // a narrow window goes into its menu, the search field last.
            Layout.fillWidth: false
            Layout.preferredWidth: implicitWidth
            visible: root.chosenAtEnd.length > 0
            alignment: Qt.AlignRight
            actions: root.chosenAtEnd
            display: Config.toolbarIconsOnly ? QQC2.AbstractButton.IconOnly : QQC2.AbstractButton.TextBesideIcon
        }

        QQC2.ToolButton {
            action: root.actions.rightSidebar
            display: QQC2.AbstractButton.IconOnly
            QQC2.ToolTip.text: text
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
        }

        QQC2.ToolButton {
            id: menuButton

            icon.name: "application-menu"
            text: i18nc("@action:button", "Menu")
            display: QQC2.AbstractButton.IconOnly
            down: mainMenu.visible
            onClicked: mainMenu.popup(menuButton, 0, menuButton.height)

            QQC2.ToolTip.text: text
            QQC2.ToolTip.visible: hovered && !mainMenu.visible
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

            QQC2.Menu {
                id: mainMenu

                QQC2.MenuItem { action: root.actions.databases }
                QQC2.MenuItem { action: root.actions.renameDatabase }
                QQC2.MenuItem { action: root.actions.rescan }
                QQC2.MenuItem { action: root.actions.selectAll }
                QQC2.MenuItem { action: root.actions.focusSearch }
                QQC2.MenuItem { action: root.actions.advancedSearch }
                QQC2.MenuItem { action: root.actions.listColumns }
                QQC2.MenuSeparator {}
                QQC2.Menu {
                    title: i18nc("@title:menu", "Go")
                    QQC2.MenuItem { action: root.actions.goInbox }
                    QQC2.MenuItem { action: root.actions.goAll }
                    QQC2.MenuItem { action: root.actions.goFlagged }
                    QQC2.MenuItem { action: root.actions.goTrash }
                    QQC2.MenuSeparator {}
                    QQC2.MenuItem { action: root.actions.goRules }
                    QQC2.MenuItem { action: root.actions.scripts }
                }
                QQC2.Menu {
                    title: i18nc("@title:menu", "Document")
                    QQC2.MenuItem { action: root.actions.openWith }
                    QQC2.MenuItem { action: root.actions.showInFileManager }
                    QQC2.MenuItem { action: root.actions.copyLocation }
                    QQC2.MenuItem { action: root.actions.rename }
                    QQC2.MenuSeparator {}
                    QQC2.MenuItem { action: root.actions.ocr }
                    QQC2.MenuItem { action: root.actions.reindex }
                    QQC2.MenuItem { action: root.actions.runScript }
                    QQC2.MenuItem { action: root.actions.clearDrawing }
                    QQC2.MenuItem { action: root.actions.revert }
                    QQC2.MenuSeparator {}
                    QQC2.MenuItem { action: root.actions.toInbox }
                    QQC2.MenuItem { action: root.actions.fromInbox }
                    QQC2.MenuItem { action: root.actions.fromGroup }
                    QQC2.MenuItem { action: root.actions.restore }
                }
                QQC2.Menu {
                    title: i18nc("@title:menu", "Preview")
                    QQC2.MenuItem { action: root.actions.previewNone }
                    QQC2.MenuItem { action: root.actions.previewBelow }
                    QQC2.MenuItem { action: root.actions.previewBesideDocuments }
                    QQC2.MenuSeparator {}
                    QQC2.MenuItem { action: root.actions.rawView }
                    QQC2.MenuSeparator {}
                    QQC2.MenuItem { action: root.actions.find }
                    QQC2.MenuItem { action: root.actions.findNext }
                    QQC2.MenuItem { action: root.actions.findPrevious }
                    QQC2.MenuSeparator {}
                    QQC2.MenuItem { action: root.actions.zoomIn }
                    QQC2.MenuItem { action: root.actions.zoomOut }
                    QQC2.MenuItem { action: root.actions.zoomReset }
                }
                QQC2.Menu {
                    title: i18nc("@title:menu", "Sidebar")
                    QQC2.MenuItem { action: root.actions.editSidebarItem }
                    QQC2.MenuItem { action: root.actions.deleteSidebarItem }
                    QQC2.MenuSeparator {}
                    QQC2.MenuItem { action: root.actions.leftSidebar }
                    QQC2.MenuItem { action: root.actions.rightSidebar }
                    QQC2.MenuItem { action: root.actions.resetPanels }
                }
                QQC2.Menu {
                    title: i18nc("@title:menu rules and scripts", "Automation")
                    QQC2.MenuItem { action: root.actions.goRules }
                    QQC2.MenuItem { action: root.actions.newRule }
                    QQC2.MenuItem { action: root.actions.applyRule }
                    QQC2.MenuSeparator {}
                    QQC2.MenuItem { action: root.actions.scripts }
                    QQC2.MenuItem { action: root.actions.newScript }
                    QQC2.MenuItem { action: root.actions.runScript }
                    QQC2.MenuItem { action: root.actions.scriptOutput }
                    QQC2.MenuItem { action: root.actions.stopScripts }
                    QQC2.MenuSeparator {}
                    QQC2.MenuItem { action: root.actions.automationDelete }
                }
                QQC2.MenuItem { action: root.actions.emptyTrash }
                QQC2.MenuSeparator {}
                QQC2.MenuItem { action: root.actions.commandBar }
                QQC2.MenuItem { action: root.actions.configureShortcuts }
                QQC2.MenuItem { action: root.actions.configureToolbar }
                QQC2.MenuItem { action: root.actions.configure }
                QQC2.MenuSeparator {}
                QQC2.MenuItem { action: root.actions.manual }
                QQC2.MenuItem { action: root.actions.about }
                QQC2.MenuItem { action: root.actions.quit }
            }
        }
    }
}
