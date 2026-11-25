// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The preview below the file browser. Picks a viewer that fits the selected document and
// fetches the content first if it lives on a remote server.
QQC2.Pane {
    id: root

    required property Actions actions

    readonly property real documentId: App.documents.selectionCount === 1 ? App.documents.currentId : 0

    // What the viewer currently shows. Changed only together with the viewer itself.
    property real shownId: 0
    property var shownInfo: ({})
    property string shownKind: "none"
    property url shownSource
    // The document's own content the view was made from. For an Office document that is not
    // what is shown: the viewer gets a PDF rendition of it.
    property url shownContent
    property string message: ""
    property int pdfPage: 0

    // Drawing tool settings, kept while moving between documents.
    property int drawTool: InkCanvas.Pen
    property color drawColor: "#da4453" // see DrawToolBar for why this is not a theme colour
    property real drawWidth: 3

    readonly property bool drawable: shownKind === "pdf" || shownKind === "image" || shownKind === "office"

    // How large text is shown. Unlike the zoom of a picture or a page, which starts out
    // fitted to the view for each of them, this is the reader's and stays.
    property real textZoom: Config.previewTextZoom
    onTextZoomChanged: {
        if (Config.previewTextZoom !== textZoom) {
            Config.previewTextZoom = textZoom;
            Config.save();
        }
    }

    // What the actions of the same names do; see PreviewToolBar for what a viewer brings along.
    function zoomIn(): void {
        if (viewer.item?.zoomable) {
            viewer.item.zoomIn();
        }
    }
    function zoomOut(): void {
        if (viewer.item?.zoomable) {
            viewer.item.zoomOut();
        }
    }
    function zoomReset(): void {
        if (viewer.item?.zoomable) {
            viewer.item.zoomReset();
        }
    }
    /** Brings the place a sticky note is at into view, where the viewer has pages to turn to. */
    function showNote(note: var): void {
        if (viewer.item?.showNote) {
            viewer.item.showNote(note);
        }
    }
    /** One of the commands for a table in the preview, by the name of its action. */
    function tableCommand(what: string): void {
        if (viewer.item?.tableCommand) {
            viewer.item.tableCommand(what);
        }
    }
    function toggleRaw(): void {
        if (viewer.item?.rawAvailable) {
            viewer.item.raw = !viewer.item.raw;
        }
    }

    // ---------------------------------------------------------- searching within the document
    //
    // The panel that asks for it (FindPane) sets `findText`; the viewer finds, marks and
    // brings into view. A viewer that can be searched offers, by these names:
    //
    //   findable   whether it can be searched right now
    //   findText   what to find; set from here
    //   findCount  how many places it found, findMore if there are more than it counted
    //   findIndex  the hit it brought the reader to last, -1 for none
    //   findHit(i) what to say about hit i: `label` (its page, line or cell), and the text
    //              around it as `before`, `match`, `after`
    //   showHit(i) brings hit i into view
    //
    // Viewers without these (pictures, canvases) cannot be searched.

    /** What to find in the document that is shown; empty for nothing. */
    property string findText
    readonly property bool findAvailable: viewer.status === Loader.Ready && (viewer.item?.findable ?? false)
    readonly property int findCount: findAvailable && findText.length > 0 ? viewer.item.findCount : 0
    readonly property bool findMore: findAvailable && (viewer.item.findMore ?? false)
    readonly property int findIndex: findAvailable ? viewer.item.findIndex : -1
    /** Why the document cannot be searched, where it is one that could be otherwise. */
    readonly property bool findPaused: viewer.status === Loader.Ready && (viewer.item?.editing ?? false)

    function findHit(at: int): var {
        return findAvailable ? viewer.item.findHit(at) : ({});
    }
    function findGo(at: int): void {
        if (findAvailable && at >= 0 && at < findCount) {
            viewer.item.showHit(at);
        }
    }
    // Round and round: after the last hit comes the first.
    function findNext(): void {
        if (findCount > 0) {
            findGo((findIndex + 1) % findCount);
        }
    }
    function findPrevious(): void {
        if (findCount > 0) {
            findGo(findIndex <= 0 ? findCount - 1 : findIndex - 1);
        }
    }
    // What is typed is found as it is typed: the reader is brought to the first hit of
    // every new search.
    function findFirst(): void {
        if (findCount > 0 && findIndex < 0) {
            findGo(0);
        }
    }
    onFindCountChanged: Qt.callLater(findFirst)
    onFindIndexChanged: Qt.callLater(findFirst)

    Binding {
        target: viewer.item
        property: "findText"
        value: root.findText
        when: root.findAvailable
    }

    padding: 0
    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false

    function kindFor(info: var, available: bool): string {
        if (!available) {
            return "unavailable";
        }
        if (info.container ?? false) {
            return "mailbox";
        }
        if ((info.office ?? false) && App.library.officeViewAvailable) {
            return "office";
        }
        switch (info.kind) {
        case 1:
            return "text";
        case 2:
            return info.mime === "text/html" || info.mime === "application/xhtml+xml" ? "text" : "extracted";
        case 3:
            return "pdf";
        case 4:
            return "image";
        case 5:
            return "email";
        case 6:
            return "canvas";
        case 7:
            return "website";
        }
        return "generic";
    }

    function closeViewer(): void {
        const item = viewer.item;
        if (item && item.aboutToClose) {
            item.aboutToClose();
        }
        if (item && (shownKind === "pdf" || shownKind === "office")) {
            pdfPage = item.currentPage;
        }
        viewer.sourceComponent = null;
    }

    function show(kind: string, info: var, source: url, text: string): void {
        const sameDocument = info.id === shownId && kind === shownKind;
        closeViewer();
        if (!sameDocument) {
            pdfPage = 0;
        }
        shownId = info.id ?? 0;
        shownInfo = info;
        shownKind = kind;
        shownSource = source;
        shownContent = info.contentUrl ?? "";
        message = text;
        const components = {
            "text": textComponent,
            "extracted": extractedComponent,
            "unavailable": extractedComponent,
            "pdf": pdfComponent,
            "office": pdfComponent,
            "image": imageComponent,
            "email": emailComponent,
            "canvas": canvasComponent
        };
        viewer.sourceComponent = components[kind] ?? null;
        editIfWanted();
    }

    function reload(): void {
        if (documentId <= 0) {
            show(App.documents.selectionCount > 1 ? "multiple" : "none", {}, "", "");
            return;
        }
        show("loading", App.library.documentInfo(documentId), "", "");
        App.library.requestContent(documentId);
    }

    // Shows a document whose content is at hand. Office documents take a detour: Indeks asks
    // for a PDF rendition and shows that when it arrives.
    function present(info: var): void {
        const kind = kindFor(info, true);
        if (kind !== "office") {
            show(kind, info, info.contentUrl, "");
            return;
        }
        if (shownKind === "office" && shownId === info.id) {
            // Already on screen. If the rendition turns out to be the same, as it is when the
            // content only moved into Indeks' copy, the view stays as it is.
            shownInfo = info;
            shownContent = info.contentUrl;
        } else {
            show("converting", info, "", "");
        }
        App.library.requestRendition(info.id);
    }

    function startEditing(): void {
        if (viewer.item && viewer.item.startEditing) {
            viewer.item.startEditing();
        }
    }

    // A document that is to be written in as soon as it is shown: one that was just made.
    // In a large database the list it is selected in takes a moment to come, and the
    // preview shows it only then.
    property real editWanted: 0

    function editOnceShown(id: real): void {
        editWanted = id;
        editIfWanted();
    }

    function editIfWanted(): void {
        if (editWanted > 0 && shownId === editWanted && viewer.status === Loader.Ready && viewer.item?.startEditing) {
            editWanted = 0;
            viewer.item.startEditing();
        }
    }

    function finishEditing(): void {
        if (viewer.item && viewer.item.finishEditing) {
            viewer.item.finishEditing();
        }
    }

    // Lets the Edit action show whether a text is being edited.
    Binding {
        target: App
        property: "textEditing"
        value: viewer.item?.editing ?? false
    }

    // Lets the actions for the preview know what there is to act on.
    Binding {
        target: App
        property: "previewZoomable"
        value: viewer.item?.zoomable ?? false
    }
    Binding {
        target: App
        property: "rawViewAvailable"
        value: viewer.item?.rawAvailable ?? false
    }
    Binding {
        target: App
        property: "tableShown"
        value: viewer.item?.table ?? false
    }
    Binding {
        target: App
        property: "rawView"
        value: viewer.item?.raw ?? false
    }

    onDocumentIdChanged: {
        if (documentId > 0 && documentId !== editWanted) {
            // Another document was selected in the meantime: that one is not for writing in.
            editWanted = 0;
        }
        reload();
    }
    Component.onCompleted: reload()

    Connections {
        target: App.documents
        function onSelectionChanged(): void {
            if (root.documentId <= 0) {
                root.reload();
            }
        }
    }

    Connections {
        target: App.library
        function onContentReady(id: real, path: string): void {
            if (id === root.documentId) {
                // Ask again: now that the content is at hand the info carries its location.
                root.present(App.library.documentInfo(id));
            }
        }
        function onRenditionReady(id: real, pdf: url): void {
            if (id !== root.documentId) {
                return;
            }
            const info = App.library.documentInfo(id);
            if (root.shownKind === "office" && root.shownId === id && root.shownSource.toString() === pdf.toString()) {
                root.shownInfo = info;
            } else {
                root.show("office", info, pdf, "");
            }
        }
        function onRenditionFailed(id: real, text: string): void {
            if (id === root.documentId) {
                root.show("extracted", App.library.documentInfo(id), "", text);
            }
        }
        function onContentFailed(id: real, text: string): void {
            if (id === root.documentId) {
                root.show("unavailable", App.library.documentInfo(id), "", text);
            }
        }
        function onDocumentChanged(id: real): void {
            if (id !== root.shownId || id !== root.documentId) {
                return;
            }
            const info = App.library.documentInfo(id);
            const item = viewer.item;
            if (item && item.editing) {
                // The editor holds the newest text; do not pull it away under the user.
                root.shownInfo = info;
                return;
            }
            const available = (info.contentPath ?? "").length > 0;
            const kind = root.kindFor(info, available);
            const moved = available && info.contentUrl.toString() !== root.shownContent.toString();
            if (kind === "office" && (root.shownKind === "office" || root.shownKind === "converting")) {
                if (moved) {
                    root.present(info);
                } else {
                    root.shownInfo = info;
                }
            } else if (kind === "office" && available) {
                root.present(info);
            } else if (kind !== root.shownKind || moved) {
                // The content moved, typically into Indeks' own copy when editing or drawing began.
                root.show(kind, info, info.contentUrl, "");
            } else {
                root.shownInfo = info;
            }
        }
    }

    Component {
        id: textComponent

        TextPreview {
            documentId: root.shownId
            info: root.shownInfo
            searchTerms: App.documents.searchTerms
            textZoom: root.textZoom
            onZoomRequested: zoom => root.textZoom = zoom
        }
    }

    Component {
        id: pdfComponent

        PdfPreview {
            documentId: root.shownId
            source: root.shownSource
            searchTerms: App.documents.searchTerms
            drawing: App.annotating
            drawTool: root.drawTool
            drawColor: root.drawColor
            drawWidth: root.drawWidth
            initialPage: root.pdfPage
        }
    }

    Component {
        id: imageComponent

        ImagePreview {
            documentId: root.shownId
            source: root.shownSource
            drawing: App.annotating
            drawTool: root.drawTool
            drawColor: root.drawColor
            drawWidth: root.drawWidth
        }
    }

    Component {
        id: emailComponent

        EmailPreview {
            documentId: root.shownId
            textZoom: root.textZoom
            onZoomRequested: zoom => root.textZoom = zoom
        }
    }

    Component {
        id: canvasComponent

        CanvasPreview {
            documentId: root.shownId
        }
    }

    // For documents Indeks cannot render itself, and for files that are out of reach:
    // the text that was read when the document was indexed.
    Component {
        id: extractedComponent

        ColumnLayout {
            id: extracted

            readonly property string indexedText: App.library.indexedText(root.shownId)

            // Searching within the text; see above for what the preview asks for.
            property alias findText: extractedFind.text
            readonly property bool findable: indexedText.length > 0
            readonly property int findCount: extractedFind.count
            readonly property bool findMore: extractedFind.more
            readonly property int findIndex: extractedFind.index
            function findHit(at: int): var {
                return extractedFind.hit(at);
            }
            function showHit(at: int): void {
                extractedFind.show(at);
            }

            // For the bar above the preview; see PreviewToolBar.
            readonly property string status: indexedText.length > 0 ? i18nc("@info:status", "The text Indeks found in the document") : ""
            readonly property bool zoomable: indexedText.length > 0
            readonly property real zoomFactor: root.textZoom
            readonly property bool zoomIsDefault: root.textZoom === 1
            readonly property string zoomResetText: i18nc("@info:tooltip", "Back to the normal size")
            function zoomStep(larger: bool): void {
                const next = Math.round(root.textZoom * (larger ? 1.1 : 1 / 1.1) * 100) / 100;
                root.textZoom = Math.abs(next - 1) < 0.04 ? 1 : Math.max(0.5, Math.min(4, next));
            }
            function zoomIn(): void {
                zoomStep(true);
            }
            function zoomOut(): void {
                zoomStep(false);
            }
            function zoomReset(): void {
                root.textZoom = 1;
            }

            spacing: 0

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                position: Kirigami.InlineMessage.Position.Header
                visible: true
                type: root.shownKind === "unavailable" || root.message.length > 0 ? Kirigami.MessageType.Warning : Kirigami.MessageType.Information
                text: {
                    if (root.shownKind === "unavailable") {
                        return extracted.indexedText.length > 0
                            ? i18nc("@info", "%1 Below is the text Indeks stored when it indexed the document.", root.message)
                            : root.message;
                    }
                    if (root.message.length > 0) {
                        // An Office document LibreOffice could not make a rendition of.
                        return i18nc("@info", "%1 Below is the text of the document without its formatting.", root.message);
                    }
                    if ((root.shownInfo.office ?? false) && !App.library.officeViewAvailable) {
                        return i18nc("@info", "This is the text of the document without its formatting. With LibreOffice installed, Indeks shows it as it looks.");
                    }
                    return i18nc("@info", "This is the text of the document without its formatting. Open it in its own application to see it in full.");
                }
                actions: [
                    Kirigami.Action {
                        fromQAction: App.action("doc_open")
                        visible: root.shownKind !== "unavailable"
                    }
                ]
            }

            QQC2.ScrollView {
                id: extractedScroll

                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: extracted.indexedText.length > 0

                QQC2.TextArea {
                    id: extractedText

                    readOnly: true
                    wrapMode: TextEdit.Wrap
                    selectByMouse: true
                    textFormat: TextEdit.PlainText
                    text: extracted.indexedText
                    font.family: Kirigami.Theme.defaultFont.family
                    font.pointSize: Kirigami.Theme.defaultFont.pointSize * root.textZoom
                    background: Rectangle {
                        color: Kirigami.Theme.backgroundColor
                    }

                    TextFind {
                        id: extractedFind

                        edit: extractedText
                        flickable: extractedScroll.contentItem as Flickable
                    }

                    WheelHandler {
                        acceptedModifiers: Qt.ControlModifier
                        onWheel: event => {
                            if (event.angleDelta.y !== 0) {
                                extracted.zoomStep(event.angleDelta.y > 0);
                            }
                        }
                    }
                }
            }

            Kirigami.PlaceholderMessage {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: Kirigami.Units.gridUnit
                visible: extracted.indexedText.length === 0
                icon.name: root.shownInfo.icon ?? "unknown"
                text: root.shownInfo.name ?? ""
                explanation: i18nc("@info:placeholder", "No text of this document is stored in the index.")
            }
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

        // Always there, whatever is shown and even when nothing is.
        PreviewToolBar {
            Layout.fillWidth: true
            actions: root.actions
            view: viewer.status === Loader.Ready ? viewer.item : null
        }

        DrawToolBar {
            Layout.fillWidth: true
            visible: App.annotating && root.drawable
            tool: root.drawTool
            penColor: root.drawColor
            penWidth: root.drawWidth
            onToolChanged: root.drawTool = tool
            onPenColorChanged: root.drawColor = penColor
            onPenWidthChanged: root.drawWidth = penWidth
            onUndoRequested: if (viewer.item && viewer.item.undoStroke) {
                viewer.item.undoStroke();
            }
            onDoneRequested: App.annotating = false
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            position: Kirigami.InlineMessage.Position.Header
            visible: (root.shownInfo.hasLocalCopy ?? false) && root.shownKind !== "none" && root.shownKind !== "multiple" && root.shownKind !== "loading"
            type: Kirigami.MessageType.Information
            text: i18nc("@info", "You are looking at Indeks' own copy of this document. The original file is unchanged.")
            actions: [
                Kirigami.Action {
                    fromQAction: App.action("doc_revert")
                    text: i18nc("@action:button", "Discard Copy")
                }
            ]
        }

        Loader {
            id: viewer

            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: status === Loader.Ready
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: viewer.status !== Loader.Ready

            Kirigami.PlaceholderMessage {
                anchors.centerIn: parent
                width: parent.width - Kirigami.Units.gridUnit * 4
                visible: root.shownKind === "none" || root.shownKind === "multiple"
                icon.name: root.shownKind === "multiple" ? "document-multiple" : "view-preview"
                text: root.shownKind === "multiple"
                    ? i18ncp("@info:placeholder", "%1 document selected", "%1 documents selected", App.documents.selectionCount)
                    : i18nc("@info:placeholder", "Select a document to see it here")
            }

            Kirigami.PlaceholderMessage {
                anchors.centerIn: parent
                width: parent.width - Kirigami.Units.gridUnit * 4
                visible: root.shownKind === "generic"
                icon.name: root.shownInfo.icon ?? "unknown"
                text: root.shownInfo.name ?? ""
                explanation: i18nc("@info:placeholder", "Indeks cannot show documents of the type “%1”.", root.shownInfo.mimeComment ?? "")
                helpfulAction: root.actions.open
            }

            // A website is its address: there is nothing of the page itself to show.
            ColumnLayout {
                id: website

                readonly property string address: root.shownInfo.webAddress ?? ""

                anchors.centerIn: parent
                width: parent.width - Kirigami.Units.gridUnit * 4
                visible: root.shownKind === "website"
                spacing: Kirigami.Units.largeSpacing

                Kirigami.Icon {
                    Layout.alignment: Qt.AlignHCenter
                    implicitWidth: Kirigami.Units.iconSizes.huge
                    implicitHeight: implicitWidth
                    source: root.shownInfo.icon ?? "globe"
                }
                Kirigami.Heading {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    level: 2
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                    text: root.shownInfo.name ?? ""
                }
                // The address as it is, to be read before it is opened, and to be selected.
                Kirigami.SelectableLabel {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WrapAnywhere
                    textFormat: Text.PlainText
                    text: website.address.length > 0 ? website.address : i18nc("@info:placeholder", "This file holds no address of a website.")
                }
                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: Kirigami.Units.smallSpacing

                    QQC2.Button {
                        action: root.actions.open
                    }
                    QQC2.Button {
                        visible: website.address.length > 0
                        action: root.actions.copyAddress
                    }
                }
            }

            // A mailbox is what is in it; that is looked at message by message.
            Kirigami.PlaceholderMessage {
                id: mailboxMessage

                readonly property int messages: root.shownInfo.parts ?? 0
                readonly property bool open: root.shownKind === "mailbox" && App.documents.count >= 0 && App.documents.isExpanded(root.shownId)

                anchors.centerIn: parent
                width: parent.width - Kirigami.Units.gridUnit * 4
                visible: root.shownKind === "mailbox"
                icon.name: root.shownInfo.icon ?? "mail-folder-inbox"
                text: messages > 0 ? i18ncp("@info:placeholder", "A mailbox with %1 message", "A mailbox with %1 messages", messages)
                                   : i18nc("@info:placeholder", "A mailbox")
                explanation: messages > 0
                    ? i18nc("@info:placeholder", "Open it in the list, with the arrow before its name, to see its messages. Each of them is found by searching, and can be tagged, grouped and annotated like any document.")
                    : i18nc("@info:placeholder", "Its messages are listed once Indeks has read it.")
                helpfulAction: Kirigami.Action {
                    enabled: mailboxMessage.messages > 0
                    visible: enabled
                    text: mailboxMessage.open ? i18nc("@action:button", "Hide Messages") : i18nc("@action:button", "Show Messages")
                    icon.name: mailboxMessage.open ? "go-up" : "go-down"
                    onTriggered: App.documents.setExpanded(root.shownId, !mailboxMessage.open)
                }
            }

            Kirigami.LoadingPlaceholder {
                anchors.centerIn: parent
                visible: root.shownKind === "loading"
                text: i18nc("@info:placeholder", "Fetching %1…", root.shownInfo.name ?? "")
            }

            Kirigami.LoadingPlaceholder {
                anchors.centerIn: parent
                visible: root.shownKind === "converting"
                text: i18nc("@info:placeholder", "Preparing %1 for viewing…", root.shownInfo.name ?? "")
            }
        }
    }
}
