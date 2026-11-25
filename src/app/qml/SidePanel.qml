// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// One of the two panels beside the middle of the window: the sidebar on the left, the side
// panel on the right. Both are made the same way: an upper area and, if the user gave it
// any views, a lower one, each with its tabs and the view of the tab that is chosen
// (PanelArea). The border between the two is the user's to move.
//
// Which view is in which area of which panel is kept by PanelLayout. Below the areas of the
// panel that has the database stand the name of the open database and what Indeks is doing.
QQC2.Pane {
    id: root

    required property Actions actions
    /** "left" or "right". */
    required property string side
    /** The tab that is being dragged; see PanelDrag. */
    required property PanelDrag viewDrag

    /** A panel without views has nothing to show. */
    readonly property bool hasViews: upper.views.length > 0
    // The database and what is being done to it belong together, wherever the user put
    // the view of the database.
    readonly property bool hasDatabase: App.panels.revision >= 0 && App.panels.sideOf("database") === side

    // How much of the height the lower area takes; kept from one start to the next.
    property real lowerShare: side === "left" ? Config.leftPanelLower : Config.rightPanelLower

    /** Where the views of an area of this panel put themselves; @p part is "top" or "bottom". */
    function container(part: string): Item {
        return part === "bottom" ? lower.container : upper.container;
    }

    function keepShare(): void {
        if (side === "left") {
            Config.leftPanelLower = lowerShare;
        } else {
            Config.rightPanelLower = lowerShare;
        }
        Config.save();
    }

    padding: 0
    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false

    contentItem: ColumnLayout {
        spacing: 0

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            QQC2.SplitView {
                id: split

                anchors.fill: parent
                orientation: Qt.Vertical
                handle: PanelBorder {}

                PanelArea {
                    id: upper

                    side: root.side
                    part: "top"
                    viewDrag: root.viewDrag
                    splittable: lower.views.length === 0
                    QQC2.SplitView.fillHeight: true
                    QQC2.SplitView.minimumHeight: Kirigami.Units.gridUnit * 6
                }
                PanelArea {
                    id: lower

                    side: root.side
                    part: "bottom"
                    viewDrag: root.viewDrag
                    visible: views.length > 0
                    QQC2.SplitView.preferredHeight: split.height * root.lowerShare
                    QQC2.SplitView.minimumHeight: Kirigami.Units.gridUnit * 6
                }
            }

            // The border between the two areas, to be taken hold of; see ResizeGrip.
            ResizeGrip {
                readonly property real least: lower.QQC2.SplitView.minimumHeight

                visible: lower.visible
                horizontalBorder: true
                border: lower.y
                onMoved: position => {
                    const height = Math.max(least, Math.min(split.height - least, split.height - position));
                    root.lowerShare = split.height > 0 ? height / split.height : root.lowerShare;
                }
                onDraggingChanged: {
                    if (!dragging) {
                        root.keepShare();
                    }
                }
            }
        }

        // Which database all of this belongs to; a click lists the others. It keeps its
        // place at the bottom whatever is above it, and moves up when the display of what
        // Indeks is doing slides in below it.
        Kirigami.Separator {
            Layout.fillWidth: true
            visible: root.hasDatabase
        }
        QQC2.ItemDelegate {
            id: databaseButton

            Layout.fillWidth: true
            visible: root.hasDatabase
            icon.name: "server-database"
            text: App.databaseName
            font.weight: Font.DemiBold
            // As much room below the name as above it; the style leaves less below.
            topPadding: Kirigami.Units.largeSpacing
            bottomPadding: Kirigami.Units.largeSpacing
            onClicked: root.actions.databases.trigger()

            // The name, and at the far end how much room the database takes on the disk.
            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Icon {
                    source: databaseButton.icon.name
                    implicitWidth: Kirigami.Units.iconSizes.smallMedium
                    implicitHeight: implicitWidth
                }
                QQC2.Label {
                    Layout.fillWidth: true
                    text: databaseButton.text
                    font: databaseButton.font
                    elide: Text.ElideRight
                }
                QQC2.Label {
                    visible: text.length > 0
                    text: App.library.databaseSizeText
                    font: Kirigami.Theme.smallFont
                    color: Kirigami.Theme.disabledTextColor

                    Accessible.name: i18nc("@info size of the database on the disk", "Size on disk: %1", text)
                }
            }

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
            // Not laid out at all in the other panel: it would be told to slide in there too.
            visible: root.hasDatabase && reveal > 0
            onClicked: root.actions.activity.trigger()
        }
    }

    QQC2.Menu {
        id: databaseMenu

        QQC2.MenuItem { action: root.actions.renameDatabase }
        QQC2.MenuItem { action: root.actions.databases }
        QQC2.MenuItem { action: root.actions.addDatabase }
        QQC2.MenuItem { action: root.actions.newDatabase }
    }
}
