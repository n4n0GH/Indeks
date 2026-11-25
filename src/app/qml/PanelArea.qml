// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// One area of a panel: a bar of tabs, and below it the view of the tab that is chosen. A
// panel has two of these, one above the other; see SidePanel.
//
// Which views the area has is the user's to say (PanelLayout): a tab can be dragged to
// another place among the tabs, into the other area or the other panel, and a right click
// on it offers the same moves. The views themselves are made once, by the window, and put
// themselves into `container` of the area they are in.
ColumnLayout {
    id: root

    /** "left" or "right": the panel this area is part of. */
    required property string side
    /** "top" or "bottom". */
    required property string part

    // What is where; asked anew whenever something moved.
    readonly property list<string> views: App.panels.revision >= 0 ? App.panels.views(side, part) : []
    readonly property string current: App.panels.revision >= 0 ? App.panels.current(side, part) : ""
    /** Where the views of this area are shown. */
    readonly property alias container: container
    /** The tab that is being dragged, of whichever area; see PanelDrag. */
    required property PanelDrag viewDrag
    /** Whether a view dropped on the lower part of this area gets an area of its own below it. */
    property bool splittable: false
    /** How high the bar of tabs is: as high as the bars above the documents and the preview. */
    property real barHeight: 0
    /**
     * An area with one view can do without its bar, if the user says so (the setting
     * hideSingleTabBar). The view can still be moved: with the commands for that, and
     * another view can still be dragged into this area.
     */
    readonly property bool barShown: !(Config.hideSingleTabBar && views.length === 1)

    function show(view: string): void {
        App.panels.setCurrent(view);
        // Two of the views say what the middle of the window is about.
        if (view === "database" || view === "automation") {
            App.sidebarTab = view;
        }
    }

    spacing: 0

    QQC2.ToolBar {
        id: bar

        Layout.fillWidth: true
        Layout.preferredHeight: root.barHeight > 0 ? root.barHeight : implicitHeight
        visible: root.barShown
        position: QQC2.ToolBar.Header
        padding: 0

        // A tab that is dragged over the bar goes between the tabs it is let go between.
        DropArea {
            id: tabDrop

            // Before which of the tabs, as they stand, the one that is dragged would go.
            property int index: 0
            readonly property real tabWidth: width / Math.max(1, root.views.length)

            function follow(x: real): void {
                index = Math.max(0, Math.min(root.views.length, Math.round(x / Math.max(1, tabWidth))));
            }

            parent: bar
            anchors.fill: parent
            z: 10
            keys: [root.viewDrag.key]
            onEntered: drag => follow(drag.x)
            onPositionChanged: drag => follow(drag.x)
            onDropped: drop => {
                const view = root.viewDrag.view;
                const at = index;
                drop.accept();
                // A moment later: the tabs are made anew, the one that is dragged with them.
                Qt.callLater(() => App.panels.place(view, root.side, root.part, at));
            }

            Rectangle {
                visible: tabDrop.containsDrag
                x: Math.max(0, Math.min(parent.width - width, tabDrop.index * tabDrop.tabWidth - width / 2))
                width: 3
                height: parent.height
                color: Kirigami.Theme.highlightColor
            }
        }

        contentItem: RowLayout {
            spacing: 0

            Repeater {
                model: root.views

                // The tabs show their icons alone, which leaves the bar as low as the bars
                // beside it; what each is for says its tooltip, and its name is still there
                // for screen readers.
                delegate: Kirigami.NavigationTabButton {
                    id: tab

                    required property string modelData

                    Layout.fillWidth: true
                    // All of the same width, whatever their names are.
                    Layout.preferredWidth: 1
                    Layout.fillHeight: true
                    // The bar says how high; this is for a bar that was not told.
                    implicitHeight: Kirigami.Units.gridUnit * 2

                    text: App.panels.title(modelData)
                    icon.name: App.panels.icon(modelData)
                    display: QQC2.AbstractButton.IconOnly
                    // Which tab is chosen is not the button's to keep.
                    autoExclusive: false
                    checkable: true
                    checked: modelData === root.current
                    onClicked: {
                        root.show(modelData);
                        checked = Qt.binding(() => tab.modelData === root.current);
                    }

                    Accessible.name: text

                    QQC2.ToolTip.text: App.panels.toolTip(modelData)
                    QQC2.ToolTip.visible: hovered && !tabMenu.visible
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

                    TapHandler {
                        acceptedButtons: Qt.RightButton
                        onTapped: {
                            tabMenu.view = tab.modelData;
                            tabMenu.popup();
                        }
                    }
                    // Dragged to where its view is to be; see PanelDrag.
                    DragHandler {
                        target: null
                        acceptedButtons: Qt.LeftButton
                        onActiveChanged: {
                            if (active) {
                                root.viewDrag.begin(tab.modelData, centroid.scenePosition);
                            } else {
                                root.viewDrag.finish();
                            }
                        }
                        onCentroidChanged: {
                            if (active) {
                                root.viewDrag.moveTo(centroid.scenePosition);
                            }
                        }
                    }
                }
            }
        }
    }

    Item {
        id: container

        Layout.fillWidth: true
        Layout.fillHeight: true

        // A tab that is dragged over the view goes into this area, as its last tab; over
        // the lower part of an area that has none below it, into an area of its own there.
        // What is marked is where the view would be.
        DropArea {
            id: viewDrop

            property bool below: false
            // Not for the only tab of this area: it cannot leave the area empty above it.
            readonly property bool mayGoBelow: root.splittable && root.part === "top" && !(root.views.length === 1 && root.views[0] === root.viewDrag.view)
            readonly property real lowerPart: 0.4

            function follow(y: real): void {
                below = mayGoBelow && y > height * (1 - lowerPart);
            }

            anchors.fill: parent
            z: 100
            keys: [root.viewDrag.key]
            onEntered: drag => follow(drag.y)
            onPositionChanged: drag => follow(drag.y)
            onDropped: drop => {
                const view = root.viewDrag.view;
                const part = below ? "bottom" : root.part;
                drop.accept();
                Qt.callLater(() => App.panels.place(view, root.side, part, -1));
            }

            Rectangle {
                visible: viewDrop.containsDrag
                y: viewDrop.below ? parent.height * (1 - viewDrop.lowerPart) : 0
                width: parent.width
                height: viewDrop.below ? parent.height * viewDrop.lowerPart : parent.height
                color: Qt.alpha(Kirigami.Theme.highlightColor, 0.25)
                border.color: Kirigami.Theme.highlightColor
                border.width: 2
            }
        }
    }

    // Where the view of a tab is to be. The commands are the application's, so that the
    // command bar has them too, there with the name of the view in them.
    QQC2.Menu {
        id: tabMenu

        property string view
        readonly property var moveAction: view.length > 0 ? App.action("panel_move_" + view) : null
        readonly property var splitAction: view.length > 0 ? App.action("panel_split_" + view) : null

        QQC2.MenuItem {
            text: root.side === "left" ? i18nc("@action:inmenu", "Move to Right Panel") : i18nc("@action:inmenu", "Move to Left Panel")
            icon.name: root.side === "left" ? "go-next" : "go-previous"
            onTriggered: tabMenu.moveAction?.trigger()
        }
        QQC2.MenuItem {
            text: root.part === "bottom" ? i18nc("@action:inmenu", "Merge to Top") : i18nc("@action:inmenu", "Split Below")
            icon.name: root.part === "bottom" ? "go-up" : "view-split-top-bottom"
            enabled: tabMenu.splitAction?.enabled ?? false
            onTriggered: tabMenu.splitAction?.trigger()
        }
    }
}
