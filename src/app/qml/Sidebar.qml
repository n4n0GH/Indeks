// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Left sidebar. Every entry is a place to look at: clicking one changes what the file browser
// shows. Documents can be dropped on groups, tags, the inbox and the trash.
QQC2.Pane {
    id: root

    required property Actions actions

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

        QQC2.ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true

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
        id: databaseMenu

        QQC2.MenuItem { action: root.actions.renameDatabase }
        QQC2.MenuItem { action: root.actions.databases }
        QQC2.MenuItem { action: root.actions.addDatabase }
    }

    QQC2.Menu {
        id: entryMenu

        readonly property string kind: App.sidebar.currentKind

        onAboutToShow: inboxItem.checked = kind === "source" && App.library.sourceUsesInbox(App.sidebar.currentId)

        QQC2.MenuItem {
            action: root.actions.applyRule
            visible: entryMenu.kind === "rule"
            height: visible ? implicitHeight : 0
        }
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
            action: root.actions.newRule
            visible: headerMenu.section === "rules"
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
