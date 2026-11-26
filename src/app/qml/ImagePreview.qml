// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Shapes
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

    // For the bar above the preview: the text that was recognised on a scan can be shown in
    // its place. That is another viewer, which PreviewPane puts here; this only says so.
    property bool rawAvailable: false
    readonly property bool raw: false
    property string rawText

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
    // ------------------------------------------------------------- the text in the picture
    //
    // Where text was recognised in the picture, it can be selected, copied and searched,
    // although it is a picture: the index knows where each word stands (RecognizedText).

    /** Search terms to mark, coming from the search that found the picture. */
    property list<string> searchTerms

    // Searching within the picture; see PreviewPane for what a viewer offers.
    property string findText
    readonly property bool findable: scanText.available
    readonly property int findCount: findText.length > 0 ? scanText.findCount : 0
    readonly property bool findMore: false
    property int findIndex: -1
    onFindTextChanged: findIndex = -1

    function findHit(at: int): var {
        if (at < 0 || at >= findCount) {
            return ({});
        }
        const found = scanText.hit(at);
        return {
            label: "",
            before: found.before,
            match: found.match,
            after: found.after
        };
    }

    function showHit(at: int): void {
        if (at < 0 || at >= findCount) {
            return;
        }
        findIndex = at;
        scanText.setCurrent(at);
        // Down to a third above the place, where the picture is higher than the view.
        const down = frame.y + scanText.hit(at).top * frame.height - flickable.height / 3;
        flickable.contentY = Math.max(0, Math.min(flickable.contentHeight - flickable.height, down));
    }

    function copySelection(): void {
        if (!scanText.empty) {
            App.copyText(scanText.text);
        }
    }
    function selectAll(): void {
        scanText.selectPage(0);
    }

    RecognizedText {
        id: scanText

        documentId: root.documentId
        findText: root.findText.length > 0 ? root.findText : root.searchTerms.length > 0 ? root.searchTerms[0] : ""
        // A picture whose text was recognised before Indeks kept where the words stand
        // is read again now, in the background.
        Component.onCompleted: App.library.requestRecognizedLayout(root.documentId)
    }
    /** Whether there are words to select in the picture. */
    readonly property bool selectable: scanText.revision >= 0 && scanText.hasWords(0) && !drawing && !placingNote

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
            icon.name: "edit-copy"
            text: i18nc("@action:intoolbar", "Copy Selected Text")
            displayHint: Kirigami.DisplayHint.IconOnly
            visible: root.zoomable && scanText.available
            enabled: !scanText.empty
            onTriggered: root.copySelection()
        },
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

                // A place as shares of the width and the height of the picture.
                function inShares(place: point): point {
                    return Qt.point(place.x / Math.max(1, width), place.y / Math.max(1, height));
                }

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

                // What was found in the picture, the place that was gone to, and what is
                // selected. The places of recognised words are shares of the picture.
                Shape {
                    anchors.fill: parent
                    visible: image.status === Image.Ready && scanText.available

                    ShapePath {
                        strokeWidth: -1
                        fillColor: Qt.alpha(Kirigami.Theme.neutralTextColor, 0.4)
                        scale: Qt.size(frame.width, frame.height)
                        PathMultiline {
                            paths: {
                                scanText.findCount; // re-evaluate when the search changes
                                return scanText.marks(0);
                            }
                        }
                    }
                    ShapePath {
                        strokeWidth: -1
                        fillColor: Qt.alpha(Kirigami.Theme.highlightColor, 0.5)
                        scale: Qt.size(frame.width, frame.height)
                        PathMultiline {
                            paths: root.findIndex >= 0 ? scanText.currentMarks : []
                        }
                    }
                    ShapePath {
                        strokeWidth: -1
                        fillColor: Qt.alpha(Kirigami.Theme.highlightColor, 0.4)
                        scale: Qt.size(frame.width, frame.height)
                        PathMultiline {
                            paths: scanText.geometry
                        }
                    }
                }

                // Dragging selects the words between where the mouse went down and where it
                // is, a double click a word, a click nothing; as on the pages of a PDF.
                DragHandler {
                    enabled: root.selectable
                    target: null
                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad | PointerDevice.Stylus
                    acceptedButtons: Qt.LeftButton
                    cursorShape: Qt.IBeamCursor
                    onActiveChanged: if (active) {
                        flickable.forceActiveFocus();
                    }
                    onCentroidChanged: if (active) {
                        scanText.selectBetween(0, frame.inShares(centroid.pressPosition), frame.inShares(centroid.position));
                    }
                }
                TapHandler {
                    enabled: root.selectable
                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad | PointerDevice.Stylus
                    acceptedButtons: Qt.LeftButton
                    gesturePolicy: TapHandler.DragThreshold
                    onSingleTapped: {
                        flickable.forceActiveFocus();
                        scanText.clear();
                    }
                    onDoubleTapped: eventPoint => scanText.selectWordAt(0, frame.inShares(eventPoint.position))
                }
                HoverHandler {
                    enabled: root.selectable
                    cursorShape: Qt.IBeamCursor
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

            // While the picture has the keyboard, selecting everything means its text, not
            // every document in the list.
            Keys.onShortcutOverride: event => {
                if (scanText.available && event.matches(StandardKey.SelectAll)) {
                    event.accepted = true;
                }
            }
            Keys.onPressed: event => {
                if (!scanText.available) {
                    return;
                }
                if (event.matches(StandardKey.Copy)) {
                    root.copySelection();
                    event.accepted = true;
                } else if (event.matches(StandardKey.SelectAll)) {
                    root.selectAll();
                    event.accepted = true;
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
