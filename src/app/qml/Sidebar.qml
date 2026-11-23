// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Left sidebar, which serves more than one purpose, as the panel on the right does. Tabs at
// its top lead to the database, where every entry is a place to look at and clicking one
// changes what the list of documents shows, and to automation, whose two entries put the
// rules or the scripts where the documents are listed otherwise. Documents can be dropped on
// groups, tags, the inbox and the trash.
QQC2.Pane {
    id: root

    required property Actions actions

    // In the order of the tabs.
    readonly property list<string> tabs: ["database", "automation"]
    readonly property int tabIndex: Math.max(0, tabs.indexOf(App.sidebarTab))
    property int ruleCount: 0
    property int scriptCount: 0

    function countAutomation(): void {
        ruleCount = App.library.rules().length;
        scriptCount = App.library.scripts().length;
    }

    Component.onCompleted: countAutomation()

    Connections {
        target: App.library
        function onChanged(): void {
            root.countAutomation();
        }
        function onScriptsChanged(): void {
            root.countAutomation();
        }
        function onDatabaseChanged(): void {
            root.countAutomation();
        }
    }

    padding: 0
    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false

    function dropDocuments(kind: string, key: string, itemId: real, ids: var): void {
        if (kind === "group") {
            App.library.addToGroup(ids, itemId);
        } else if (kind === "tag") {
            const tag = App.library.tags().find(entry => entry.id === itemId);
            if (tag) {
                App.library.addTag(ids, tag.name);
            }
        } else if (key === "place:inbox") {
            App.library.setInbox(ids, true);
        } else if (key === "place:trash") {
            App.library.moveToTrash(ids);
        } else if (key === "place:flagged") {
            App.library.setFlagged(ids, true);
        }
    }

    function acceptsDocuments(kind: string, key: string): bool {
        return kind === "group" || kind === "tag" || key === "place:inbox" || key === "place:trash" || key === "place:flagged";
    }

    contentItem: ColumnLayout {
        spacing: 0

        Kirigami.NavigationTabBar {
            id: tabBar

            Layout.fillWidth: true
            position: QQC2.ToolBar.Header
            actions: [
                Kirigami.Action {
                    text: i18nc("@title:tab the places, groups, tags and locations of the database", "Database")
                    tooltip: i18nc("@info:tooltip", "The documents of the database")
                    icon.name: "folder-documents"
                },
                Kirigami.Action {
                    text: i18nc("@title:tab rules and scripts", "Automation")
                    tooltip: i18nc("@info:tooltip", "Rules and scripts")
                    icon.name: "system-run"
                }
            ]
            // As in the panel on the right: icons alone, which leaves the bar lower; what
            // each is for says its tooltip, and its name is still there for screen readers.
            delegate: Kirigami.NavigationTabButton {
                required property QQC2.Action modelData

                parent: tabBar.contentItem
                action: modelData
                // As the bar's own tabs do: an action is not checkable unless told to be.
                onActionChanged: action.checkable = true
                display: QQC2.AbstractButton.IconOnly
                implicitHeight: Kirigami.Units.gridUnit * 2 + Kirigami.Units.smallSpacing

                Layout.minimumWidth: tabBar.buttonWidth
                Layout.maximumWidth: tabBar.buttonWidth
                Layout.fillHeight: true

                Accessible.name: modelData.text
            }

            // The tab that is shown is the application's to know; the bar follows it and
            // changes it.
            Component.onCompleted: currentIndex = root.tabIndex
            onCurrentIndexChanged: {
                if (currentIndex >= 0 && currentIndex !== root.tabIndex) {
                    App.sidebarTab = root.tabs[currentIndex];
                }
            }

            Connections {
                target: root
                function onTabIndexChanged(): void {
                    tabBar.currentIndex = root.tabIndex;
                }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.tabIndex

            QQC2.ScrollView {
                QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

                ListView {
                    id: list

                    model: App.sidebar
                    clip: true
                    activeFocusOnTab: true
                    keyNavigationEnabled: false

                    Keys.onUpPressed: root.step(-1)
                    Keys.onDownPressed: root.step(1)

                    delegate: QQC2.ItemDelegate {
                        id: entry

                        required property int index
                        required property string key
                        required property string label
                        required property string iconName
                        required property int count
                        required property int depth
                        required property string kind
                        required property real itemId
                        required property bool isHeader
                        required property bool collapsed
                        required property bool offline
                        required property bool isCurrent

                        width: ListView.view.width
                        highlighted: isCurrent || dropArea.containsDrag
                        topPadding: isHeader ? Kirigami.Units.largeSpacing : Kirigami.Units.smallSpacing
                        bottomPadding: Kirigami.Units.smallSpacing
                        leftPadding: Kirigami.Units.largeSpacing + depth * Kirigami.Units.gridUnit

                        Accessible.name: label
                        Accessible.role: isHeader ? Accessible.Heading : Accessible.ListItem

                        contentItem: RowLayout {
                            spacing: Kirigami.Units.smallSpacing

                            Kirigami.Icon {
                                source: entry.isHeader ? (entry.collapsed ? "arrow-right" : "arrow-down") : entry.iconName
                                selected: entry.highlighted
                                opacity: entry.offline ? 0.5 : 1
                                implicitWidth: entry.isHeader ? Kirigami.Units.iconSizes.small : Kirigami.Units.iconSizes.smallMedium
                                implicitHeight: implicitWidth
                            }
                            QQC2.Label {
                                Layout.fillWidth: true
                                text: entry.label
                                elide: Text.ElideRight
                                font.weight: entry.isHeader ? Font.DemiBold : Font.Normal
                                color: entry.highlighted ? Kirigami.Theme.highlightedTextColor : entry.isHeader || entry.offline ? Kirigami.Theme.disabledTextColor : Kirigami.Theme.textColor
                            }
                            Kirigami.Icon {
                                visible: entry.offline
                                source: "network-disconnect"
                                implicitWidth: Kirigami.Units.iconSizes.small
                                implicitHeight: implicitWidth

                                QQC2.ToolTip.text: i18nc("@info:tooltip", "This location is not connected. Its documents can still be found.")
                                QQC2.ToolTip.visible: offlineHover.hovered
                                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                                HoverHandler { id: offlineHover }
                            }
                            QQC2.Label {
                                visible: !entry.isHeader && entry.count > 0
                                text: entry.count
                                color: entry.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.disabledTextColor
                                font: Kirigami.Theme.smallFont
                            }
                        }

                        onClicked: {
                            if (isHeader) {
                                App.sidebar.toggleSection(key.substring(7));
                            } else {
                                App.sidebar.currentKey = key;
                            }
                        }

                        TapHandler {
                            acceptedButtons: Qt.RightButton
                            onTapped: {
                                if (entry.isHeader) {
                                    headerMenu.section = entry.key.substring(7);
                                    headerMenu.popup();
                                } else {
                                    App.sidebar.currentKey = entry.key;
                                    entryMenu.popup();
                                }
                            }
                        }

                        DropArea {
                            id: dropArea

                            anchors.fill: parent
                            enabled: !entry.isHeader && root.acceptsDocuments(entry.kind, entry.key)
                            keys: ["application/x-indeks-documents"]
                            onDropped: drop => {
                                const ids = drop.getDataAsString("application/x-indeks-documents").split(",").map(Number);
                                root.dropDocuments(entry.kind, entry.key, entry.itemId, ids);
                                drop.accept(Qt.LinkAction);
                            }
                        }
                    }
                }
            }

            // Automation: two entries, each a list of its own in the middle of the window.
            QQC2.ScrollView {
                QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

                ListView {
                    id: automationList

                    clip: true
                    activeFocusOnTab: true
                    keyNavigationEnabled: false
                    model: [
                        { page: "rules", label: i18nc("@item sidebar", "Rules"), icon: "run-build-configure", count: root.ruleCount },
                        { page: "scripts", label: i18nc("@item sidebar", "Scripts"), icon: "text-x-python", count: root.scriptCount }
                    ]

                    Keys.onUpPressed: App.automationPage = "rules"
                    Keys.onDownPressed: App.automationPage = "scripts"

                    delegate: QQC2.ItemDelegate {
                        id: page

                        required property var modelData

                        width: ListView.view?.width ?? 0
                        highlighted: App.automationPage === modelData.page
                        topPadding: Kirigami.Units.smallSpacing
                        bottomPadding: Kirigami.Units.smallSpacing
                        leftPadding: Kirigami.Units.largeSpacing
                        onClicked: App.automationPage = modelData.page

                        Accessible.name: modelData.label

                        contentItem: RowLayout {
                            spacing: Kirigami.Units.smallSpacing

                            Kirigami.Icon {
                                source: page.modelData.icon
                                selected: page.highlighted
                                implicitWidth: Kirigami.Units.iconSizes.smallMedium
                                implicitHeight: implicitWidth
                            }
                            QQC2.Label {
                                Layout.fillWidth: true
                                text: page.modelData.label
                                elide: Text.ElideRight
                                color: page.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                            }
                            QQC2.Label {
                                visible: page.modelData.count > 0
                                text: page.modelData.count
                                color: page.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.disabledTextColor
                                font: Kirigami.Theme.smallFont
                            }
                        }

                        TapHandler {
                            acceptedButtons: Qt.RightButton
                            onTapped: {
                                App.automationPage = page.modelData.page;
                                automationMenu.popup();
                            }
                        }
                    }
                }
            }
        }

        // Which database all of the above belongs to; a click lists the others. It keeps
        // its place at the bottom whatever the list above it holds, and moves up when the
        // display of what Indeks is doing slides in below it.
        Kirigami.Separator {
            Layout.fillWidth: true
        }
        QQC2.ItemDelegate {
            Layout.fillWidth: true
            icon.name: "server-database"
            text: App.databaseName
            font.weight: Font.DemiBold
            // As much room below the name as above it; the style leaves less below.
            topPadding: Kirigami.Units.largeSpacing
            bottomPadding: Kirigami.Units.largeSpacing
            onClicked: root.actions.databases.trigger()

            TapHandler {
                acceptedButtons: Qt.RightButton
                onTapped: databaseMenu.popup()
            }

            Accessible.description: i18nc("@info", "The open database. Activate to switch to another one.")
            QQC2.ToolTip.text: i18nc("@info:tooltip", "Switch to another database or add one")
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
        }

        ActivityIndicator {
            Layout.fillWidth: true
            onClicked: root.actions.activity.trigger()
        }
    }

    function step(delta: int): void {
        const row = App.sidebar.step(delta);
        if (row >= 0) {
            list.positionViewAtIndex(row, ListView.Contain);
        }
    }

    QQC2.Menu {
        id: automationMenu

        QQC2.MenuItem {
            action: App.automationPage === "rules" ? root.actions.newRule : root.actions.newScript
        }
    }

    QQC2.Menu {
        id: databaseMenu

        QQC2.MenuItem { action: root.actions.renameDatabase }
        QQC2.MenuItem { action: root.actions.databases }
        QQC2.MenuItem { action: root.actions.addDatabase }
        QQC2.MenuItem { action: root.actions.newDatabase }
    }

    QQC2.Menu {
        id: entryMenu

        readonly property string kind: App.sidebar.currentKind

        onAboutToShow: inboxItem.checked = kind === "source" && App.library.sourceUsesInbox(App.sidebar.currentId)

        QQC2.MenuItem {
            text: i18nc("@action:inmenu", "Update Now")
            icon.name: "view-refresh"
            visible: entryMenu.kind === "source"
            height: visible ? implicitHeight : 0
            onTriggered: App.library.rescanSource(App.sidebar.currentId)
        }
        QQC2.MenuItem {
            id: inboxItem

            // What turns up in the location from now on waits in the inbox to be filed.
            text: i18nc("@action:inmenu", "Put New Files into the Inbox")
            checkable: true
            visible: entryMenu.kind === "source"
            height: visible ? implicitHeight : 0
            onTriggered: App.library.setSourceUsesInbox(App.sidebar.currentId, checked)
        }
        QQC2.MenuItem {
            action: root.actions.newGroup
            text: entryMenu.kind === "group" ? i18nc("@action:inmenu", "New Subgroup…") : i18nc("@action:inmenu", "New Group…")
            visible: entryMenu.kind === "group"
            height: visible ? implicitHeight : 0
        }
        QQC2.MenuItem {
            action: root.actions.editSidebarItem
            text: i18nc("@action:inmenu", "Edit…")
            visible: enabled
            height: visible ? implicitHeight : 0
        }
        QQC2.MenuItem {
            action: root.actions.deleteSidebarItem
            text: entryMenu.kind === "source" ? i18nc("@action:inmenu", "Stop Indexing This Location…") : i18nc("@action:inmenu", "Delete…")
            visible: enabled
            height: visible ? implicitHeight : 0
        }
        QQC2.MenuItem {
            action: root.actions.emptyTrash
            visible: App.sidebar.currentKey === "place:trash"
            height: visible ? implicitHeight : 0
        }
        QQC2.MenuItem {
            action: root.actions.saveSearch
            visible: entryMenu.kind === "place" || entryMenu.kind === "smart"
            height: visible ? implicitHeight : 0
        }
    }

    QQC2.Menu {
        id: headerMenu

        property string section

        QQC2.MenuItem {
            action: root.actions.saveSearch
            visible: headerMenu.section === "searches"
            height: visible ? implicitHeight : 0
        }
        QQC2.MenuItem {
            action: root.actions.newGroup
            visible: headerMenu.section === "groups"
            height: visible ? implicitHeight : 0
        }
        QQC2.MenuItem {
            action: root.actions.newTag
            visible: headerMenu.section === "tags"
            height: visible ? implicitHeight : 0
        }
        QQC2.MenuItem {
            action: root.actions.addFolder
            visible: headerMenu.section === "sources"
            height: visible ? implicitHeight : 0
        }
        QQC2.MenuItem {
            action: root.actions.addRemote
            visible: headerMenu.section === "sources"
            height: visible ? implicitHeight : 0
        }
        QQC2.MenuItem {
            text: i18nc("@action:inmenu", "Collapse or Expand")
            icon.name: "view-list-tree"
            onTriggered: App.sidebar.toggleSection(headerMenu.section)
        }
    }
}
