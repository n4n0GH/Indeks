// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Pdf
import QtQuick.Shapes
import org.kde.kirigami as Kirigami

// Shows a PDF document page by page. Text can be selected and copied, search hits are
// highlighted, and in drawing mode every page carries a canvas to paint on.
ColumnLayout {
    id: root

    required property real documentId
    required property url source
    property list<string> searchTerms
    property bool drawing: false
    property int drawTool: InkCanvas.Pen
    property color drawColor: "#da4453" // see DrawToolBar for why this is not a theme colour
    property real drawWidth: 3
    /** Page to show first, e.g. the one the user was on before the document was reloaded. */
    property int initialPage: 0

    readonly property int currentPage: Math.max(0, pages.indexAt(pages.width / 2, pages.contentY + pages.height / 2))
    readonly property int pageCount: pdf.pageCount
    property real zoom: 1.0

    spacing: 0

    function undoStroke(): void {
        const item = pages.itemAtIndex(lastDrawnPage >= 0 ? lastDrawnPage : currentPage);
        if (item) {
            item.undo();
        }
    }
    property int lastDrawnPage: -1

    function copySelection(): void {
        if (!textSelection.empty) {
            App.copyText(textSelection.text);
        }
    }
    /** Selects all the text of the page that is in view. */
    function selectAll(): void {
        textSelection.selectPage(currentPage);
    }

    // Notes stuck to the pages; see StickyNotes.
    property var notes: []
    /** The next click on a page puts a note there. */
    property bool placingNote: false

    function reloadNotes(): void {
        notes = App.library.notes(documentId);
    }

    /** Brings the page a note is stuck to into view. */
    function showNote(note: var): void {
        if (note.page >= 0 && note.page < pageCount) {
            pages.positionViewAtIndex(note.page, ListView.Beginning);
        }
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

    // For the bar above the preview; see PreviewToolBar. Pages are as wide as the view at 100 %.
    readonly property bool ready: pdf.status === PdfDocument.Ready
    readonly property string status: ready ? i18nc("@info:status", "Page %1 of %2", currentPage + 1, pageCount) : ""
    readonly property bool zoomable: ready
    readonly property real zoomFactor: zoom
    readonly property bool zoomIsDefault: zoom === 1
    readonly property string zoomResetText: i18nc("@info:tooltip", "Fit the pages to the width of the view")
    function zoomIn(): void {
        zoom = Math.min(4, zoom * 1.25);
    }
    function zoomOut(): void {
        zoom = Math.max(0.25, zoom / 1.25);
    }
    function zoomReset(): void {
        zoom = 1;
    }
    // Notes can be stuck to a place here; the bar above the preview offers that, see
    // PreviewToolBar, and sets placingNote for the next click to put one down.
    readonly property bool stickyAvailable: ready
    readonly property list<Kirigami.Action> viewActions: [
        Kirigami.Action {
            icon.name: "edit-copy"
            text: i18nc("@action:intoolbar", "Copy Selected Text")
            displayHint: Kirigami.DisplayHint.IconOnly
            visible: root.ready
            enabled: !textSelection.empty
            onTriggered: root.copySelection()
        }
    ]

    PdfDocument {
        id: pdf

        source: root.source
        onStatusChanged: {
            if (pdf.status === PdfDocument.Ready && root.initialPage > 0) {
                Qt.callLater(() => pages.positionViewAtIndex(Math.min(root.initialPage, pdf.pageCount - 1), ListView.Beginning));
            }
        }
    }

    // What is selected of the text, with the mouse: see the pages below.
    PdfText {
        id: textSelection

        source: pdf.status === PdfDocument.Ready ? root.source : ""
    }

    PdfSearchModel {
        id: searchModel

        document: pdf
        searchString: root.searchTerms.length > 0 ? root.searchTerms[0] : ""
    }

    QQC2.ScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: pdf.status === PdfDocument.Ready
        QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

        ListView {
            id: pages

            model: pdf.status === PdfDocument.Ready ? pdf.pageCount : 0
            spacing: Kirigami.Units.largeSpacing
            topMargin: Kirigami.Units.largeSpacing
            bottomMargin: Kirigami.Units.largeSpacing
            clip: true
            cacheBuffer: height
            // Dragging with the mouse selects text or draws; scrolling is left to wheel and touch.
            acceptedButtons: Qt.NoButton
            delegate: Item {
                id: pageItem

                required property int index
                readonly property size pointSize: pdf.pagePointSize(index)
                readonly property real pageScale: pointSize.width > 0 && paper.width > 0 ? paper.width / pointSize.width : 1

                function undo(): void {
                    ink.undo();
                }
                // A place on the page as the PDF counts: in points from its top left corner.
                function inPoints(place: point): point {
                    return Qt.point(place.x / pageScale, place.y / pageScale);
                }

                width: pages.width
                height: paper.height

                Rectangle {
                    id: paper

                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.floor(Math.max(Kirigami.Units.gridUnit * 6, (pages.width - Kirigami.Units.gridUnit * 2) * root.zoom))
                    height: Math.round(width * pageItem.pointSize.height / pageItem.pointSize.width)
                    // The page itself is content, not interface: paper is white under every colour scheme.
                    color: "white"

                    PdfPageImage {
                        id: image

                        anchors.fill: parent
                        document: pdf
                        currentFrame: pageItem.index
                        asynchronous: true
                        fillMode: Image.PreserveAspectFit
                        sourceSize.width: paper.width * Screen.devicePixelRatio
                        sourceSize.height: 0
                    }

                    QQC2.BusyIndicator {
                        anchors.centerIn: parent
                        running: image.status === Image.Loading
                        visible: running
                    }

                    // Search hits and the text selection, both in page coordinates (points).
                    Shape {
                        anchors.fill: parent
                        visible: image.status === Image.Ready

                        ShapePath {
                            strokeWidth: -1
                            fillColor: Qt.alpha(Kirigami.Theme.neutralTextColor, 0.4)
                            scale: Qt.size(pageItem.pageScale, pageItem.pageScale)
                            PathMultiline {
                                paths: {
                                    searchModel.searchString; // re-evaluate when the search changes
                                    return searchModel.boundingPolygonsOnPage(pageItem.index);
                                }
                            }
                        }
                        ShapePath {
                            strokeWidth: -1
                            fillColor: Qt.alpha(Kirigami.Theme.highlightColor, 0.4)
                            scale: Qt.size(pageItem.pageScale, pageItem.pageScale)
                            PathMultiline {
                                paths: textSelection.page === pageItem.index ? textSelection.geometry : []
                            }
                        }
                    }

                    // Dragging selects text, from wherever the drag starts to wherever it is:
                    // in the margin, between two lines or below the last one, the text that
                    // is nearest counts. A double click selects a word, a click nothing.
                    DragHandler {
                        id: selectDrag

                        enabled: !root.drawing && !root.placingNote
                        target: null
                        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad | PointerDevice.Stylus
                        acceptedButtons: Qt.LeftButton
                        cursorShape: Qt.IBeamCursor
                        onActiveChanged: if (active) {
                            pages.forceActiveFocus();
                        }
                        onCentroidChanged: if (active) {
                            textSelection.selectBetween(pageItem.index, pageItem.inPoints(centroid.pressPosition), pageItem.inPoints(centroid.position));
                        }
                    }
                    TapHandler {
                        enabled: !root.drawing && !root.placingNote
                        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad | PointerDevice.Stylus
                        acceptedButtons: Qt.LeftButton
                        gesturePolicy: TapHandler.DragThreshold
                        onSingleTapped: {
                            pages.forceActiveFocus();
                            textSelection.clear();
                        }
                        onDoubleTapped: eventPoint => textSelection.selectWordAt(pageItem.index, pageItem.inPoints(eventPoint.position))
                    }
                    // Where there is text to select, the pointer says so.
                    HoverHandler {
                        enabled: !root.drawing && !root.placingNote
                        cursorShape: Qt.IBeamCursor
                    }

                    InkCanvas {
                        id: ink

                        anchors.fill: parent
                        editing: root.drawing
                        tool: root.drawTool
                        penColor: root.drawColor
                        penWidth: root.drawWidth
                        Component.onCompleted: strokes = App.library.ink(root.documentId, pageItem.index)
                        onEdited: {
                            root.lastDrawnPage = pageItem.index;
                            App.library.setInk(root.documentId, pageItem.index, strokes);
                        }

                        HoverHandler {
                            enabled: root.drawing
                            cursorShape: Qt.CrossCursor
                        }
                    }

                    StickyNotes {
                        anchors.fill: parent
                        documentId: root.documentId
                        page: pageItem.index
                        notes: root.notes
                        placing: root.placingNote
                        active: !root.drawing
                        onPlaced: root.placingNote = false
                    }
                }
            }

            // While the pages have the keyboard, selecting everything means their text, not
            // every document in the list.
            Keys.onShortcutOverride: event => {
                if (event.matches(StandardKey.SelectAll)) {
                    event.accepted = true;
                }
            }
            Keys.onPressed: event => {
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
                onWheel: event => root.zoom = Math.max(0.25, Math.min(4, root.zoom * (event.angleDelta.y > 0 ? 1.1 : 1 / 1.1)))
            }
        }
    }

    Kirigami.PlaceholderMessage {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.margins: Kirigami.Units.gridUnit
        visible: pdf.status === PdfDocument.Error
        icon.name: "dialog-error"
        text: i18nc("@info:placeholder", "This PDF cannot be shown")
        explanation: pdf.error
    }

    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: pdf.status === PdfDocument.Loading || pdf.status === PdfDocument.Null

        QQC2.BusyIndicator {
            anchors.centerIn: parent
            running: parent.visible
        }
    }
}
