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

    // ---------------------------------------------------------- searching within the document
    // What the preview asks of a view that can be searched; see PreviewPane. The search is
    // Qt's own, which goes through the pages one after the other: the hits come in as it goes.

    property string findText
    readonly property bool findable: pdf.status === PdfDocument.Ready
    readonly property int findCount: findText.length > 0 ? searchModel.count + scanText.findCount : 0
    readonly property bool findMore: false
    property int findIndex: -1
    onFindTextChanged: findIndex = -1

    // What the search knows about each hit; see QPdfSearchModel::Role.
    readonly property int pageRole: Qt.UserRole
    readonly property int indexOnPageRole: Qt.UserRole + 1
    readonly property int locationRole: Qt.UserRole + 2
    readonly property int contextBeforeRole: Qt.UserRole + 3
    readonly property int contextAfterRole: Qt.UserRole + 4

    function findHit(at: int): var {
        if (at < 0 || at >= findCount) {
            return ({});
        }
        // Qt hands the words around a hit over with a sign for every line break in them.
        if (at >= searchModel.count) {
            // A place on a scanned page; see RecognizedText.
            const found = scanText.hit(at - searchModel.count);
            return {
                label: i18nc("@info a page of a document, by its number", "Page %1", found.page + 1),
                before: found.before,
                match: found.match,
                after: found.after
            };
        }
        const hit = searchModel.index(at, 0);
        const before = (searchModel.data(hit, contextBeforeRole) ?? "").replace(/[\s\u23ce]+/g, " ");
        return {
            label: i18nc("@info a page of a document, by its number", "Page %1", searchModel.data(hit, pageRole) + 1),
            // From a whole word on, where the passage is cut short.
            before: before.length > 40 ? before.slice(before.length - 40).replace(/^\S*\s/, "") : before,
            match: findText,
            after: (searchModel.data(hit, contextAfterRole) ?? "").replace(/[\s\u23ce]+/g, " ").slice(0, 80)
        };
    }

    function showHit(at: int): void {
        if (at < 0 || at >= findCount) {
            return;
        }
        findIndex = at;
        let page = 0;
        let down = 0; // how far down its page the place is, in the size the page is shown at
        if (at >= searchModel.count) {
            // On a scanned page: where the recognised words stand, in shares of the page.
            const found = scanText.hit(at - searchModel.count);
            scanText.setCurrent(at - searchModel.count);
            page = found.page;
            const size = pdf.pagePointSize(page);
            down = size.width > 0 ? found.top * paperWidth * size.height / size.width : 0;
        } else {
            scanText.setCurrent(-1);
            const hit = searchModel.index(at, 0);
            page = searchModel.data(hit, pageRole);
            // Which of the hits is the one the reader is at: the page, and its number on the page.
            searchModel.currentPage = page;
            searchModel.currentResult = searchModel.data(hit, indexOnPageRole);
            const place = searchModel.data(hit, locationRole);
            const size = pdf.pagePointSize(page);
            down = size.width > 0 ? place.y * paperWidth / size.width : 0;
        }
        // The page to the top of the view, then down to a third above the hit.
        pages.positionViewAtIndex(page, ListView.Beginning);
        const last = pages.contentHeight - pages.height + pages.originY + pages.bottomMargin;
        pages.contentY = Math.max(pages.contentY, Math.min(last, pages.contentY + down - pages.height / 3));
    }

    // How wide a page is shown; the same for all of them.
    readonly property real paperWidth: Math.floor(Math.max(Kirigami.Units.gridUnit * 6, (pages.width - Kirigami.Units.gridUnit * 2) * zoom))

    readonly property int currentPage: Math.max(0, pages.indexAt(pages.width / 2, pages.contentY + pages.height / 2))
    readonly property int pageCount: pdf.pageCount
    property real zoom: 1.0

    spacing: 0

    // For the bar above the preview: the text that was recognised on a scan can be shown in
    // its place. That is another viewer, which PreviewPane puts here; this only says so.
    property bool rawAvailable: false
    readonly property bool raw: false
    property string rawText

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
        } else if (!scanText.empty) {
            App.copyText(scanText.text);
        }
    }
    /** Selects all the text of the page that is in view. */
    function selectAll(): void {
        if (isScan(currentPage)) {
            textSelection.clear();
            scanText.selectPage(currentPage);
        } else {
            scanText.clear();
            textSelection.selectPage(currentPage);
        }
    }
    /**
     * Whether a page is a picture of text whose words were recognised: then it is those
     * that are selected and found on it (RecognizedText), and not the text of the PDF, of
     * which the page has none.
     */
    function isScan(page: int): bool {
        return scanText.revision >= 0 && scanText.hasWords(page);
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
            enabled: !textSelection.empty || !scanText.empty
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

    // The words recognised on scanned pages, and where they stand. A scan whose text was
    // recognised before Indeks kept that is read again now, in the background; its words
    // arrive when that is done.
    RecognizedText {
        id: scanText

        documentId: root.documentId
        // Pages that have text of their own are the PDF's. Asked only of the pages words
        // were recognised on, and so not at all of a PDF that is no scan.
        ownTextPages: scanText.pageCount > 0 && textSelection.source.toString().length > 0 ? textSelection.pagesWithText(scanText.pageCount) : []
        findText: searchModel.searchString
        Component.onCompleted: App.library.requestRecognizedLayout(root.documentId)
    }

    PdfSearchModel {
        id: searchModel

        document: pdf
        // What is searched for within this document; while nothing is, the first word the
        // list of documents was searched for.
        searchString: root.findText.length > 0 ? root.findText : root.searchTerms.length > 0 ? root.searchTerms[0] : ""
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
                // A place as shares of the width and the height of the page, which is what
                // the places of recognised words are.
                function inShares(place: point): point {
                    return Qt.point(place.x / Math.max(1, paper.width), place.y / Math.max(1, paper.height));
                }
                readonly property bool scan: root.isScan(index)

                width: pages.width
                height: paper.height

                Rectangle {
                    id: paper

                    anchors.horizontalCenter: parent.horizontalCenter
                    width: root.paperWidth
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
                        // The hit the reader was brought to.
                        ShapePath {
                            strokeWidth: -1
                            fillColor: Qt.alpha(Kirigami.Theme.highlightColor, 0.5)
                            scale: Qt.size(pageItem.pageScale, pageItem.pageScale)
                            PathMultiline {
                                paths: root.findIndex >= 0 && root.findIndex < searchModel.count && searchModel.currentPage === pageItem.index
                                    ? searchModel.currentResultBoundingPolygons : []
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

                    // The same three for a scanned page: what was found on it, the place
                    // that was gone to, and what is selected. The places of recognised
                    // words are shares of the page, hence the other scale.
                    Shape {
                        anchors.fill: parent
                        visible: image.status === Image.Ready && pageItem.scan

                        ShapePath {
                            strokeWidth: -1
                            fillColor: Qt.alpha(Kirigami.Theme.neutralTextColor, 0.4)
                            scale: Qt.size(paper.width, paper.height)
                            PathMultiline {
                                paths: {
                                    scanText.findCount; // re-evaluate when the search changes
                                    return pageItem.scan ? scanText.marks(pageItem.index) : [];
                                }
                            }
                        }
                        ShapePath {
                            strokeWidth: -1
                            fillColor: Qt.alpha(Kirigami.Theme.highlightColor, 0.5)
                            scale: Qt.size(paper.width, paper.height)
                            PathMultiline {
                                paths: root.findIndex >= searchModel.count && scanText.currentPage === pageItem.index ? scanText.currentMarks : []
                            }
                        }
                        ShapePath {
                            strokeWidth: -1
                            fillColor: Qt.alpha(Kirigami.Theme.highlightColor, 0.4)
                            scale: Qt.size(paper.width, paper.height)
                            PathMultiline {
                                paths: scanText.page === pageItem.index ? scanText.geometry : []
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
                            if (pageItem.scan) {
                                textSelection.clear();
                                scanText.selectBetween(pageItem.index, pageItem.inShares(centroid.pressPosition), pageItem.inShares(centroid.position));
                            } else {
                                scanText.clear();
                                textSelection.selectBetween(pageItem.index, pageItem.inPoints(centroid.pressPosition), pageItem.inPoints(centroid.position));
                            }
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
                            scanText.clear();
                        }
                        onDoubleTapped: eventPoint => {
                            if (pageItem.scan) {
                                scanText.selectWordAt(pageItem.index, pageItem.inShares(eventPoint.position));
                            } else {
                                textSelection.selectWordAt(pageItem.index, pageItem.inPoints(eventPoint.position));
                            }
                        }
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
