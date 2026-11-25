// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The database, as a view of a panel: every entry is a place to look at, and clicking one
// changes what the list of documents shows. Documents can be dropped on groups, tags, the
// inbox and the trash. The sections that have a heading (smart groups, saved searches,
// groups, tags, locations) stand in the order the user drags them into by their headings.
//
// One of the views that the user puts into whichever area of whichever panel; see
// PanelLayout and SidePanel.
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

    // --------------------------------------------------------- putting the sections in order
    //
    // A heading that is dragged takes its section along. What it is dragged over says where
    // the section goes: before a section that stands above it, behind one that stands below.

    readonly property string sectionKey: "application/x-indeks-sidebar-section"
    /** The section whose heading is being dragged; empty while none is. */
    property string draggedSection
    property string draggedLabel
    /** The section the dragged one would be put beside, and on which side. */
    property string targetSection
    readonly property bool targetAfter: draggedSection.length > 0 && targetSection.length > 0
        && App.sidebar.sectionIndex(targetSection) > App.sidebar.sectionIndex(draggedSection)

    function dragSection(section: string, label: string, scenePoint: point): void {
        draggedSection = section;
        draggedLabel = label;
        moveSectionDrag(scenePoint);
    }

    function moveSectionDrag(scenePoint: point): void {
        const place = root.mapFromItem(null, scenePoint.x, scenePoint.y);
        sectionChip.x = place.x + Kirigami.Units.largeSpacing;
        sectionChip.y = place.y - sectionChip.height / 2;
    }

    function dropSection(): void {
        if (draggedSection.length === 0) {
            return;
        }
        const section = draggedSection;
        const target = targetSection;
        const after = targetAfter;
        sectionChip.Drag.drop();
        draggedSection = "";
        targetSection = "";
        if (target.length > 0 && target !== section) {
            // A moment later: the rows are made anew, the one that is dragged with them.
            Qt.callLater(() => App.sidebar.moveSection(section, target, after));
        }
    }

    // What follows the pointer, and what the rows see coming.
    Rectangle {
        id: sectionChip

        z: 100
        visible: root.draggedSection.length > 0
        width: chipLabel.implicitWidth + Kirigami.Units.largeSpacing * 2
        height: chipLabel.implicitHeight + Kirigami.Units.smallSpacing * 2
        radius: Kirigami.Units.cornerRadius
        color: Kirigami.Theme.backgroundColor
        border.color: Kirigami.Theme.highlightColor
        opacity: 0.9

        Drag.active: visible
        Drag.keys: [root.sectionKey]
        Drag.hotSpot.x: -Kirigami.Units.largeSpacing
        Drag.hotSpot.y: height / 2

        QQC2.Label {
            id: chipLabel

            anchors.centerIn: parent
            text: root.draggedLabel
            font.weight: Font.DemiBold
        }
    }

    function step(delta: int): void {
        App.sidebarTab = "database";
        const row = App.sidebar.step(delta);
        if (row >= 0) {
            list.positionViewAtIndex(row, ListView.Contain);
        }
    }

    contentItem: QQC2.ScrollView {
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
                required property string section
                required property bool sectionEnd
                required property bool collapsed
                required property bool offline
                required property bool isCurrent

                width: ListView.view.width
                // The entry that is selected is the one whose documents are listed: not while
                // the middle of the window lists rules or scripts.
                highlighted: (isCurrent && !App.automation) || dropArea.containsDrag
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
                        // Back to the documents, should the window be about rules or scripts.
                        App.sidebarTab = "database";
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
                            App.sidebarTab = "database";
                            App.sidebar.currentKey = entry.key;
                            entryMenu.popup();
                        }
                    }
                }

                // A heading is dragged to where its section is to be.
                DragHandler {
                    enabled: entry.isHeader
                    target: null
                    acceptedButtons: Qt.LeftButton
                    grabPermissions: PointerHandler.CanTakeOverFromAnything
                    onActiveChanged: {
                        if (active) {
                            root.dragSection(entry.section, entry.label, centroid.scenePosition);
                        } else {
                            root.dropSection();
                        }
                    }
                    onCentroidChanged: {
                        if (active) {
                            root.moveSectionDrag(centroid.scenePosition);
                        }
                    }
                }
                // Every row of a section that has a heading is a place to drop one on.
                DropArea {
                    anchors.fill: parent
                    enabled: root.draggedSection.length > 0 && entry.section !== "library" && entry.section !== root.draggedSection
                    keys: [root.sectionKey]
                    onEntered: root.targetSection = entry.section
                    // Said again with every move: the row that was left may say so after
                    // the next one was entered.
                    onPositionChanged: root.targetSection = entry.section
                    onExited: {
                        if (root.targetSection === entry.section) {
                            root.targetSection = "";
                        }
                    }
                }
                // Where the section would go: above the heading of the one it is put before,
                // below the last row of the one it is put behind.
                Rectangle {
                    visible: root.targetSection === entry.section && (root.targetAfter ? entry.sectionEnd : entry.isHeader)
                    y: root.targetAfter ? parent.height - height : 0
                    width: parent.width
                    height: 3
                    color: Kirigami.Theme.highlightColor
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
