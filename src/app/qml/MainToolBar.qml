// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The toolbar across the top of the window: panel toggles at both ends, the search field, a
// menu with everything, and in between the buttons the user chose (see ToolbarSettingsPage).
QQC2.ToolBar {
    id: root

    required property Actions actions
    readonly property alias searchField: searchField

    // What Config.toolbarActions names, as action objects.
    property list<Kirigami.Action> chosen
    property list<Kirigami.Action> separators

    function rebuild(): void {
        const outdated = Array.from(separators);
        const dividers = [];
        const list = [];
        for (const name of Config.toolbarActions) {
            let action = null;
            if (name === "separator") {
                action = separatorComponent.createObject(root);
                dividers.push(action);
            } else if (name === "menu_add") {
                action = addMenu;
            } else if (name === "menu_new") {
                action = newMenu;
            } else {
                action = actions.named(name);
            }
            if (action && !list.includes(action)) {
                list.push(action);
            }
        }
        chosen = list;
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
                    newMenuSeparator,
                    root.actions.newGroup,
                    root.actions.newTag,
                    root.actions.newRule,
                    newMenuSeparator2,
                    root.actions.newTodo
                ]
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

        Kirigami.SearchField {
            id: searchField

            Layout.preferredWidth: Kirigami.Units.gridUnit * 16
            Layout.maximumWidth: Kirigami.Units.gridUnit * 24
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

            QQC2.ToolTip.text: i18nc("@info:tooltip", "Words find file names, content and notes. Filters narrow down: tag:name, kind:pdf, ext:md, added:7d, is:annotated … The button to the right has a form for all of them.")
            QQC2.ToolTip.visible: hovered && !activeFocus
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
        }

        QQC2.ToolButton {
            action: root.actions.advancedSearch
            display: QQC2.AbstractButton.IconOnly
            QQC2.ToolTip.text: i18nc("@info:tooltip", "Filters: narrow the list down by name, content, type, date, size, tags and more")
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
        }

        QQC2.ToolButton {
            action: root.actions.saveSearch
            display: QQC2.AbstractButton.IconOnly
            QQC2.ToolTip.text: text
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
        }

        QQC2.ToolButton {
            action: root.actions.preview
            display: QQC2.AbstractButton.IconOnly
            QQC2.ToolTip.text: text
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
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
                    QQC2.MenuItem { action: root.actions.rawView }
                    QQC2.MenuSeparator {}
                    QQC2.MenuItem { action: root.actions.zoomIn }
                    QQC2.MenuItem { action: root.actions.zoomOut }
                    QQC2.MenuItem { action: root.actions.zoomReset }
                }
                QQC2.Menu {
                    title: i18nc("@title:menu", "Sidebar")
                    QQC2.MenuItem { action: root.actions.editSidebarItem }
                    QQC2.MenuItem { action: root.actions.deleteSidebarItem }
                    QQC2.MenuItem { action: root.actions.applyRule }
                }
                QQC2.Menu {
                    title: i18nc("@title:menu", "Scripts")
                    QQC2.MenuItem { action: root.actions.scripts }
                    QQC2.MenuItem { action: root.actions.newScript }
                    QQC2.MenuItem { action: root.actions.runScript }
                    QQC2.MenuSeparator {}
                    QQC2.MenuItem { action: root.actions.scriptOutput }
                    QQC2.MenuItem { action: root.actions.stopScripts }
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
