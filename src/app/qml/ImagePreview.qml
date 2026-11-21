// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Shows an image, zoomable, with a canvas on top to draw on it.
ColumnLayout {
    id: root

    required property real documentId
    required property url source
    property bool drawing: false
    property int drawTool: InkCanvas.Pen
    property color drawColor: "#da4453" // see DrawToolBar for why this is not a theme colour
    property real drawWidth: 3

    /** 0 fits the image into the view, anything else is the scale relative to its real size. */
    property real zoom: 0
    readonly property real fitScale: image.implicitWidth > 0 && image.implicitHeight > 0
        ? Math.min(1, (flickable.width - Kirigami.Units.gridUnit) / image.implicitWidth, (flickable.height - Kirigami.Units.gridUnit) / image.implicitHeight)
        : 1
    readonly property real effectiveScale: zoom > 0 ? zoom : fitScale

    spacing: 0

    function undoStroke(): void {
        ink.undo();
    }

    function zoomBy(factor: real): void {
        zoom = Math.max(0.05, Math.min(8, effectiveScale * factor));
    }

    // For the bar above the preview; see PreviewToolBar.
    readonly property string status: image.status === Image.Ready
        ? i18nc("@info:status image size in pixels", "%1 × %2 pixels", image.implicitWidth, image.implicitHeight) : ""
    readonly property bool zoomable: image.status === Image.Ready
    readonly property real zoomFactor: effectiveScale
    readonly property bool zoomIsDefault: zoom === 0
    readonly property string zoomResetText: i18nc("@info:tooltip", "Fit the picture into the view")
    function zoomIn(): void {
        zoomBy(1.25);
    }
    function zoomOut(): void {
        zoomBy(1 / 1.25);
    }
    function zoomReset(): void {
        zoom = 0;
    }
    // Notes stuck to the picture; see StickyNotes.
    property var notes: []
    /** The next click on the picture puts a note there. */
    property bool placingNote: false

    function reloadNotes(): void {
        notes = App.library.notes(documentId);
    }

    Component.onCompleted: reloadNotes()

    Connections {
        target: App.library
        function onDocumentChanged(id: real): void {
            if (id === root.documentId) {
                root.reloadNotes();
            }
        }
    }

    // Notes can be stuck to a place here; the bar above the preview offers that, see
    // PreviewToolBar, and sets placingNote for the next click to put one down.
    readonly property bool stickyAvailable: zoomable
    readonly property list<Kirigami.Action> viewActions: [
        Kirigami.Action {
            icon.name: "zoom-original"
            text: i18nc("@action:intoolbar", "Real Size")
            displayHint: Kirigami.DisplayHint.IconOnly
            visible: root.zoomable
            enabled: root.effectiveScale !== 1
            onTriggered: root.zoom = 1
        }
    ]

    QQC2.ScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true

        Flickable {
            id: flickable

            contentWidth: Math.max(width, frame.width + Kirigami.Units.gridUnit)
            contentHeight: Math.max(height, frame.height + Kirigami.Units.gridUnit)
            clip: true
            // Dragging with the mouse draws; scrolling is left to wheel and touch.
            acceptedButtons: Qt.NoButton

            Item {
                id: frame

                anchors.centerIn: parent
                width: Math.round(image.implicitWidth * root.effectiveScale)
                height: Math.round(image.implicitHeight * root.effectiveScale)

                Image {
                    id: image

                    anchors.fill: parent
                    source: root.source
                    asynchronous: true
                    autoTransform: true
                    cache: false
                    mipmap: true
                    smooth: root.effectiveScale < 4
                    fillMode: Image.PreserveAspectFit
                }

                InkCanvas {
                    id: ink

                    anchors.fill: parent
                    visible: image.status === Image.Ready
                    editing: root.drawing
                    tool: root.drawTool
                    penColor: root.drawColor
                    penWidth: root.drawWidth
                    Component.onCompleted: strokes = App.library.ink(root.documentId, 0)
                    onEdited: App.library.setInk(root.documentId, 0, strokes)

                    HoverHandler {
                        enabled: root.drawing
                        cursorShape: Qt.CrossCursor
                    }
                }

                StickyNotes {
                    anchors.fill: parent
                    visible: image.status === Image.Ready
                    documentId: root.documentId
                    page: 0
                    notes: root.notes
                    placing: root.placingNote
                    active: !root.drawing
                    onPlaced: root.placingNote = false
                }
            }

            WheelHandler {
                acceptedModifiers: Qt.ControlModifier
                onWheel: event => root.zoomBy(event.angleDelta.y > 0 ? 1.1 : 1 / 1.1)
            }
        }
    }

    Kirigami.PlaceholderMessage {
        Layout.fillWidth: true
        Layout.margins: Kirigami.Units.gridUnit
        visible: image.status === Image.Error
        icon.name: "dialog-error"
        text: i18nc("@info:placeholder", "This image cannot be shown")
    }
}
