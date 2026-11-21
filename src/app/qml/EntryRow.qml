// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// One line of a list the user puts together entry by entry, like the conditions and the actions
// of a rule: what the entry is, and buttons to change it, move it and take it off again.
//
// For a list that can be reordered, this has to be a child of the view's delegate rather than
// the delegate itself: while it is dragged, the handle takes it out of its place.
QQC2.Control {
    id: root

    /** Position in the list. The drag handle reads it too, by this very name. */
    property int index: 0
    /** How many entries the list has. */
    property int count: 0
    property string label
    property string iconName
    /** Whether the entry can be changed where it is. */
    property bool editable: false
    /** The view to reorder by dragging; none if the order of the entries does not matter. */
    property ListView reorderView: null
    readonly property bool reorderable: reorderView !== null

    signal editRequested()
    signal removeRequested()
    signal moveRequested(int from, int to)

    hoverEnabled: true
    leftPadding: Kirigami.Units.smallSpacing
    rightPadding: Kirigami.Units.smallSpacing
    topPadding: Math.round(Kirigami.Units.smallSpacing / 2)
    bottomPadding: Math.round(Kirigami.Units.smallSpacing / 2)

    Accessible.role: Accessible.ListItem
    Accessible.name: label

    component RowButton: QQC2.ToolButton {
        display: QQC2.AbstractButton.IconOnly

        QQC2.ToolTip.text: text
        QQC2.ToolTip.visible: hovered
        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
    }

    // Lifted off the list while it is carried.
    background: Rectangle {
        visible: handle.dragActive
        color: Kirigami.Theme.backgroundColor
        border.color: Kirigami.Theme.highlightColor
        radius: Kirigami.Units.cornerRadius
    }

    contentItem: RowLayout {
        spacing: Kirigami.Units.smallSpacing

        Kirigami.ListItemDragHandle {
            id: handle

            visible: root.reorderable
            listItem: root
            listView: root.reorderView
            onMoveRequested: (from, to) => root.moveRequested(from, to)
        }
        Kirigami.Icon {
            Layout.leftMargin: root.reorderable ? 0 : Kirigami.Units.smallSpacing
            implicitWidth: Kirigami.Units.iconSizes.small
            implicitHeight: Kirigami.Units.iconSizes.small
            source: root.iconName
        }
        QQC2.Label {
            Layout.fillWidth: true
            text: root.label
            elide: Text.ElideRight
            textFormat: Text.PlainText
        }
        RowButton {
            visible: root.editable
            text: i18nc("@action:button", "Change…")
            icon.name: "document-edit"
            onClicked: root.editRequested()
        }
        RowButton {
            visible: root.reorderable
            enabled: root.index > 0
            text: i18nc("@action:button", "Move Up")
            icon.name: "go-up"
            onClicked: root.moveRequested(root.index, root.index - 1)
        }
        RowButton {
            visible: root.reorderable
            enabled: root.index < root.count - 1
            text: i18nc("@action:button", "Move Down")
            icon.name: "go-down"
            onClicked: root.moveRequested(root.index, root.index + 1)
        }
        RowButton {
            text: i18nc("@action:button", "Remove")
            icon.name: "edit-delete-remove"
            onClicked: root.removeRequested()
        }
    }
}
