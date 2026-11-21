// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Tools for drawing on an image or PDF page.
QQC2.ToolBar {
    id: root

    property int tool: InkCanvas.Pen
    property color penColor: penColors[0]
    property real penWidth: 3

    signal undoRequested()
    signal doneRequested()

    // Ink is part of the document, not of the interface: a red line must stay red whatever
    // colour scheme is active and whether or not the window has focus. Hence fixed colours here,
    // the one place where Indeks does not ask the theme.
    readonly property list<color> penColors: ["#da4453", "#f67400", "#27ae60", "#3daee9", "#9b59b6", "#232629"]
    readonly property list<string> penColorNames: [
        i18nc("@info:tooltip pen colour", "Red"),
        i18nc("@info:tooltip pen colour", "Orange"),
        i18nc("@info:tooltip pen colour", "Green"),
        i18nc("@info:tooltip pen colour", "Blue"),
        i18nc("@info:tooltip pen colour", "Purple"),
        i18nc("@info:tooltip pen colour", "Black")
    ]

    position: QQC2.ToolBar.Header

    contentItem: RowLayout {
        spacing: Kirigami.Units.mediumSpacing

        QQC2.ButtonGroup { id: toolGroup }

        QQC2.ToolButton {
            text: i18nc("@action:intoolbar drawing tool", "Pen")
            icon.name: "draw-freehand"
            checkable: true
            checked: root.tool === InkCanvas.Pen
            QQC2.ButtonGroup.group: toolGroup
            onClicked: root.tool = InkCanvas.Pen
        }
        QQC2.ToolButton {
            text: i18nc("@action:intoolbar drawing tool", "Highlighter")
            icon.name: "draw-highlight"
            checkable: true
            checked: root.tool === InkCanvas.Highlighter
            QQC2.ButtonGroup.group: toolGroup
            onClicked: root.tool = InkCanvas.Highlighter
        }
        QQC2.ToolButton {
            text: i18nc("@action:intoolbar drawing tool", "Eraser")
            icon.name: "draw-eraser"
            checkable: true
            checked: root.tool === InkCanvas.Eraser
            QQC2.ButtonGroup.group: toolGroup
            onClicked: root.tool = InkCanvas.Eraser
        }

        QQC2.ToolSeparator {}

        Repeater {
            model: root.penColors

            QQC2.ToolButton {
                id: swatch

                required property int index
                required property color modelData

                enabled: root.tool !== InkCanvas.Eraser
                checkable: true
                checked: Qt.colorEqual(root.penColor, modelData)
                autoExclusive: true
                onClicked: root.penColor = modelData

                Accessible.name: root.penColorNames[index]
                QQC2.ToolTip.text: root.penColorNames[index]
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

                contentItem: Item {
                    implicitWidth: Kirigami.Units.iconSizes.smallMedium
                    implicitHeight: Kirigami.Units.iconSizes.smallMedium

                    Rectangle {
                        anchors.centerIn: parent
                        width: Kirigami.Units.iconSizes.small
                        height: width
                        radius: width / 2
                        color: swatch.modelData
                        opacity: swatch.enabled ? 1 : 0.4
                        border.width: 1
                        border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, Kirigami.Theme.frameContrast)
                    }
                }
            }
        }

        QQC2.ToolSeparator {}

        QQC2.Label {
            text: i18nc("@label:slider", "Width:")
        }
        QQC2.Slider {
            Layout.preferredWidth: Kirigami.Units.gridUnit * 6
            enabled: root.tool !== InkCanvas.Eraser
            from: 1
            to: 12
            stepSize: 1
            value: root.penWidth
            onMoved: root.penWidth = value
            Accessible.name: i18nc("@label:slider", "Pen width")
        }

        Item {
            Layout.fillWidth: true
        }

        QQC2.ToolButton {
            text: i18nc("@action:intoolbar", "Undo Stroke")
            icon.name: "edit-undo"
            onClicked: root.undoRequested()
        }
        QQC2.ToolButton {
            text: i18nc("@action:intoolbar", "Done")
            icon.name: "dialog-ok-apply"
            onClicked: root.doneRequested()
        }
    }
}
