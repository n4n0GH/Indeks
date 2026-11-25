// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.syntaxhighlighting as Syntax

// Shows plain text, Markdown, HTML and tables of values, and lets the user edit them. Saving
// goes through the library, which writes to a copy unless Indeks owns the file.
//
// What a file is laid out as for reading (Markdown and HTML rendered, CSV and TSV as a table,
// JSON indented) can also be shown as it is in the file: `raw`.
ColumnLayout {
    id: root

    required property real documentId
    required property var info
    property list<string> searchTerms
    /** How large text is shown; the same for all documents. A change is asked for with zoomRequested(). */
    property real textZoom: 1

    signal zoomRequested(real zoom)

    // A JSON Lines file stays one even if a record quotes enough markup to pass for a web page.
    readonly property bool jsonLines: /\.(jsonl|ndjson)$/i.test(info.name ?? "")
    readonly property bool html: !jsonLines && (info.mime === "text/html" || info.mime === "application/xhtml+xml")
    // What a file that only claims to be text is written as, going by what is in it:
    // "markdown", "table" or nothing; see Library::guessedTextFormat(). Such a file is shown
    // as what it is, and as it is in the file on request, like one that says so by its name.
    property string guessed: App.library.guessedTextFormat(documentId)
    readonly property bool markdown: info.mime === "text/markdown" || guessed === "markdown"
    /** Formatted text, which is shown as it is meant to look. */
    readonly property bool rich: html || markdown
    /** Values in rows and columns, which are shown as a table. */
    readonly property bool tabular: /\.(csv|tsv|tab)$/i.test(info.name ?? "") || ["text/csv", "text/tab-separated-values", "text/x-comma-separated-values"].includes(info.mime ?? "")
        || guessed === "table"
    property bool editing: false
    /** Shows the file as it is instead of what it is laid out as. */
    property bool raw: false
    // Compared with what was loaded or last saved. A "text changed" signal would not do:
    // it also fires when the highlighter recolours the text.
    property string savedText
    readonly property string currentText: code ? codeView.text : area.text
    readonly property bool modified: editing && currentText !== savedText

    // Which of the three views is up. The table, for files of values that are only looked
    // at. The code view, for text that is shown as it is in the file: source code and data
    // with line numbers and colours, prose without. The text area, for what is rendered:
    // HTML, and Markdown while it is not being edited.
    readonly property bool table: tabular && !raw && !editing
    readonly property bool code: !table && (!rich || raw || (markdown && editing))
    readonly property bool rendered: !table && !code

    // The colouring rules for this kind of file. Plain prose has none ("None"), and running
    // the highlighter with that leaves the text invisible, so it is only used for real ones.
    readonly property var syntax: guessed === "markdown" ? Syntax.Repository.definitionForName("Markdown") : Syntax.Repository.definitionForFileName(info.name ?? "")
    readonly property bool highlighted: code && syntax.name.length > 0 && syntax.name !== "None"

    // JSON Lines, and JSON written without line breaks, are laid out for reading: a member per
    // line, indented. That is a view only; editing works on the text as it is in the file.
    property bool allRecords: false
    property var structure: ({})
    // How many records a file has that was too large to count before showing it; -1 until
    // the count is in.
    property int countedRecords: -1
    readonly property int recordTotal: structured ? (structure.total >= 0 ? structure.total : countedRecords) : 0

    // ------------------------------------------------------------- for the bar above the preview

    readonly property bool rawAvailable: !editing && (rich || tabular || formattable)
    readonly property string rawText: rich ? i18nc("@action:intoolbar show formatted text as its source", "Show Unformatted")
                                           : i18nc("@action:intoolbar show a table or laid out data as the text in the file", "View Raw")

    readonly property bool zoomable: true
    readonly property real zoomFactor: textZoom
    readonly property bool zoomIsDefault: textZoom === 1
    readonly property string zoomResetText: i18nc("@info:tooltip", "Back to the normal size")

    function zoomStep(larger: bool): void {
        // In tenths, and through 100 % exactly.
        const next = Math.round(textZoom * (larger ? 1.1 : 1 / 1.1) * 100) / 100;
        zoomRequested(Math.abs(next - 1) < 0.04 ? 1 : Math.max(0.5, Math.min(4, next)));
    }
    function zoomIn(): void {
        zoomStep(true);
    }
    function zoomOut(): void {
        zoomStep(false);
    }
    function zoomReset(): void {
        zoomRequested(1);
    }

    readonly property string status: {
        if (editing) {
            return info.owned ? i18nc("@info", "Editing a document stored in Indeks") : i18nc("@info", "Editing a copy — the original file stays as it is");
        }
        if (table) {
            const data = tableLoader.item?.table;
            if (!data || data.loading) {
                return i18nc("@info:status", "Reading the table…");
            }
            const rows = i18ncp("@info part of: N rows, M columns", "%1 row", "%1 rows", data.rows);
            const columns = i18ncp("@info part of: N rows, M columns", "%1 column", "%1 columns", data.columns);
            const size = data.partial ? i18nc("@info a table that is only shown in part: rows, columns", "The first %1, %2", rows, columns)
                                      : i18nc("@info size of a table: rows, columns", "%1, %2", rows, columns);
            // What the reader did to the table: it is not the file as it stands any more.
            const changes = [];
            if (data.hiddenColumns > 0) {
                changes.push(i18ncp("@info columns of a table that are not shown", "%1 hidden", "%1 hidden", data.hiddenColumns));
            }
            if (data.sorting) {
                changes.push(i18nc("@info:status", "sorting…"));
            } else if (data.sorted) {
                changes.push(data.sortColumn >= 0 ? i18nc("@info %1 is the name of a column", "sorted by %1", data.columnName(data.sortColumn))
                                                  : i18nc("@info", "sorted"));
            }
            return changes.length > 0 ? i18nc("@info size of a table, and what was done to it", "%1 (%2)", size, changes.join(", ")) : size;
        }
        if (structured) {
            if (!structure.lines) {
                return i18nc("@info", "Laid out for reading; the file has no line breaks");
            }
            if (recordTotal < 0) {
                // Still being counted.
                return i18ncp("@info", "The first record", "The first %1 records", structure.shown);
            }
            if (structure.shown < recordTotal) {
                return i18nc("@info", "The first %1 of %2 records", structure.shown.toLocaleString(Qt.locale(), "f", 0), recordTotal.toLocaleString(Qt.locale(), "f", 0));
            }
            return i18ncp("@info", "%1 record", "%1 records", recordTotal);
        }
        if ((info.size ?? 0) > App.library.maxTextSize()) {
            return i18nc("@info", "The beginning of a large file");
        }
        if (raw) {
            return i18nc("@info", "Shown as it is in the file");
        }
        // Said where the file does not say so itself: the reader may wonder why a text
        // file looks the way it does.
        return rendered && guessed === "markdown" ? i18nc("@info", "Shown as Markdown, which is what its text looks like") : "";
    }

    readonly property list<Kirigami.Action> viewActions: [
        // While writing
        Kirigami.Action {
            visible: root.editing && root.rendered
            icon.name: "format-text-bold"
            text: i18nc("@action:intoolbar", "Bold")
            displayHint: Kirigami.DisplayHint.IconOnly
            checkable: true
            checked: area.cursorSelection.font.bold
            onTriggered: {
                area.cursorSelection.font.bold = !area.cursorSelection.font.bold;
                checked = Qt.binding(() => area.cursorSelection.font.bold);
            }
        },
        Kirigami.Action {
            visible: root.editing && root.rendered
            icon.name: "format-text-italic"
            text: i18nc("@action:intoolbar", "Italic")
            displayHint: Kirigami.DisplayHint.IconOnly
            checkable: true
            checked: area.cursorSelection.font.italic
            onTriggered: {
                area.cursorSelection.font.italic = !area.cursorSelection.font.italic;
                checked = Qt.binding(() => area.cursorSelection.font.italic);
            }
        },
        Kirigami.Action {
            visible: root.editing && root.rendered
            icon.name: "format-text-underline"
            text: i18nc("@action:intoolbar", "Underline")
            displayHint: Kirigami.DisplayHint.IconOnly
            checkable: true
            checked: area.cursorSelection.font.underline
            onTriggered: {
                area.cursorSelection.font.underline = !area.cursorSelection.font.underline;
                checked = Qt.binding(() => area.cursorSelection.font.underline);
            }
        },
        Kirigami.Action {
            visible: root.editing
            text: i18nc("@action:intoolbar", "Save")
            icon.name: "document-save"
            enabled: root.modified
            onTriggered: root.save()
        },
        Kirigami.Action {
            visible: root.editing
            text: i18nc("@action:intoolbar", "Discard Changes")
            icon.name: "document-revert"
            enabled: root.modified
            onTriggered: root.discardChanges()
        },
        Kirigami.Action {
            visible: root.editing
            text: i18nc("@action:intoolbar", "Done")
            icon.name: "dialog-ok-apply"
            onTriggered: root.finishEditing()
        },
        // While reading
        Kirigami.Action {
            visible: !root.editing && root.structured && root.structure.lines && root.structure.more && !root.allRecords
            text: i18nc("@action:intoolbar", "Show More")
            icon.name: "view-more-symbolic"
            tooltip: i18nc("@info:tooltip", "Format as much of the file as the preview takes; this can take a moment")
            onTriggered: {
                root.allRecords = true;
                root.load();
            }
        },
        Kirigami.Action {
            visible: !root.editing && root.structured && codeView.lines.hasFoldableRegions
            text: codeView.lines.hasFolds ? i18nc("@action:intoolbar", "Unfold All") : i18nc("@action:intoolbar", "Fold All")
            icon.name: codeView.lines.hasFolds ? "expand-all-symbolic" : "collapse-all-symbolic"
            displayHint: Kirigami.DisplayHint.IconOnly
            onTriggered: {
                if (codeView.lines.hasFolds) {
                    codeView.lines.unfoldAll();
                } else {
                    codeView.lines.foldTopLevel(true);
                }
            }
        },
        Kirigami.Action {
            id: headerRowAction

            readonly property TableModel data: tableLoader.item?.table ?? null

            visible: root.table && !!data && !data.loading && data.rows + (data.headerRow ? 1 : 0) > 0
            text: i18nc("@action:intoolbar whether the first row of a table holds the names of the columns", "Column Names")
            icon.name: "edit-table-insert-row-above"
            tooltip: i18nc("@info:tooltip", "Whether the first row of the file names the columns")
            checkable: true
            checked: data?.headerRow ?? false
            onTriggered: {
                data.headerRow = !data.headerRow;
                checked = Qt.binding(() => headerRowAction.data?.headerRow ?? false);
            }
        }
    ]

    Connections {
        target: App.library
        function onRecordsCounted(id: real, total: int): void {
            if (id === root.documentId) {
                root.countedRecords = total;
            }
        }
    }
    readonly property bool structured: structure.text !== undefined
    // Whether this document can be shown formatted, found out when it is first loaded.
    property bool formattable: false
    // What is formatted at first; large files take noticeably longer than they are worth.
    readonly property int recordLimit: 500

    spacing: 0

    // Everything there is to show, with line breaks as the views count them: one character
    // each, however the file writes them. Places in the text go by this.
    property string sourceText

    // Shows @p text from its beginning. The line arguments are CodeView.show()'s.
    function present(text: string, numberedLines: var, lineNumbers: var): void {
        sourceText = text.includes("\r") ? text.replace(/\r\n?/g, "\n") : text;
        if (code) {
            codeView.show(sourceText, numberedLines, lineNumbers);
        } else {
            area.text = sourceText;
            if (proseFlick) {
                proseFlick.contentY = 0;
            }
        }
    }

    readonly property Flickable proseFlick: proseScroll.contentItem as Flickable

    function selectText(position: int, length: int): void {
        if (code) {
            codeView.select(position, length);
        } else {
            area.select(position, position + length);
        }
    }

    function load(): void {
        structure = code && !editing && !raw ? App.library.formattedText(documentId, allRecords ? 0 : recordLimit) : {};
        countedRecords = -1;
        if (structured) {
            formattable = true;
            if (structure.total < 0) {
                App.library.countRecords(documentId);
            }
        }
        if (table) {
            // The table reads the file by itself; the two text views let go of theirs.
            area.text = "";
            codeView.show("", [], []);
            sourceText = "";
        } else if (code) {
            area.text = "";
            if (structured) {
                present(structure.text, structure.recordLines, structure.sourceLines);
            } else {
                present(App.library.readText(documentId), [], []);
            }
        } else {
            codeView.show("", [], []);
            area.textFormat = html ? TextEdit.RichText : TextEdit.MarkdownText;
            present(App.library.readText(documentId), [], []);
        }
        savedText = currentText;
        stage.settle();
        if (!editing && !table) {
            showFirstHit();
        }
        findInCode();
    }

    // The other way of showing the same file. A moment later: which view is up follows from
    // `raw` too, and has to have followed before the text is handed to one of them.
    onRawChanged: Qt.callLater(root.loadUnlessEditing)

    function loadUnlessEditing(): void {
        if (!editing) {
            load();
        }
    }

    // ------------------------------------------------------------- searching within the text
    //
    // What the preview asks of a view that can be searched; see PreviewPane. The table and
    // the rendered text do it themselves (DataTable, TextFind); text that is shown as it is
    // in the file is searched here, in all of it, of which the view may hold only a part.

    property string findText
    /** Not while the text is being written: the places would be off with every key. */
    readonly property bool findable: !editing
    // Where the text was found, as places in `sourceText`.
    property var codeHits: []
    property bool codeMore: false
    property int codeIndex: -1
    readonly property int findCount: table ? (tableLoader.item?.findCount ?? 0) : code ? codeHits.length : proseFind.count
    readonly property bool findMore: table ? (tableLoader.item?.findMore ?? false) : code ? codeMore : proseFind.more
    readonly property int findIndex: table ? (tableLoader.item?.findIndex ?? -1) : code ? codeIndex : proseFind.index

    function findInCode(): void {
        codeIndex = -1;
        if (!code || editing || findText.length === 0) {
            codeHits = [];
            codeMore = false;
            return;
        }
        const found = proseFind.positionsIn(sourceText, findText, proseFind.most);
        codeMore = found.length > proseFind.most;
        codeHits = codeMore ? found.slice(0, proseFind.most) : found;
    }

    function findHit(at: int): var {
        if (table) {
            return tableLoader.item?.findHit(at) ?? ({});
        }
        if (!code) {
            return proseFind.hit(at);
        }
        if (at < 0 || at >= codeHits.length) {
            return ({});
        }
        // Lines are named where they are numbered, and as the file numbers them.
        const position = codeHits[at];
        const label = highlighted && !structured ? i18nc("@info a line of a text, by its number", "Line %1", codeView.lines.lineOfSource(position) + 1) : "";
        return proseFind.described(sourceText, position, findText.length, label);
    }

    function showHit(at: int): void {
        if (table) {
            tableLoader.item?.showHit(at);
        } else if (!code) {
            proseFind.show(at);
        } else if (at >= 0 && at < codeHits.length) {
            codeIndex = at;
            codeView.select(codeHits[at], findText.length);
        }
    }

    onFindTextChanged: findInCode()
    onCodeChanged: findInCode()
    onEditingChanged: findInCode()

    /** One of the commands for a table, by the name of its action; see DataTable. */
    function tableCommand(what: string): void {
        if (table && tableLoader.item) {
            tableLoader.item.command(what);
        }
    }

    // Selects the first search word found so that the reader lands where the hit is.
    function showFirstHit(): void {
        if (searchTerms.length > 0) {
            // What is rendered reads differently from its source; that is searched as shown.
            const plain = (rendered ? area.getText(0, area.length) : sourceText).toLowerCase();
            for (const term of searchTerms) {
                const position = plain.indexOf(term.toLowerCase());
                if (position >= 0) {
                    selectText(position, term.length);
                    return;
                }
            }
        }
        area.cursorPosition = 0;
    }

    function startEditing(): void {
        if (editing) {
            return;
        }
        if ((info.size ?? 0) > App.library.maxTextSize()) {
            // Only the beginning is loaded; saving that would cut the document short.
            applicationWindow().showPassiveNotification(i18nc("@info", "This document is too large to be edited in Indeks."));
            return;
        }
        // While a long text is only read, the view holds part of it; for editing it has
        // to hold all. And the rendered and the formatted views give way to the text as it
        // is in the file.
        const line = code ? codeView.topLine : -1;
        const keepPlace = code && !structured;
        editing = true;
        // HTML as it looks is written where it stands; everything else is written as text.
        if (code) {
            load();
            if (keepPlace && line > 0) {
                codeView.showLine(line);
            }
        }
        if (code) {
            codeView.editor.forceActiveFocus();
        } else {
            area.forceActiveFocus();
        }
    }

    function save(): bool {
        const content = rendered ? area.getFormattedText(0, area.length) : currentText;
        if (!App.library.saveText(documentId, content)) {
            return false;
        }
        savedText = currentText;
        // What was written may be something else than what was there.
        guessed = App.library.guessedTextFormat(documentId);
        return true;
    }

    function finishEditing(): void {
        if (modified && !save()) {
            return;
        }
        const line = codeView.topLine;
        const wasCode = code;
        editing = false;
        if (wasCode) {
            load();
            // The same text as before, unless it is laid out for reading again now.
            if (code && !structured && line > 0) {
                codeView.showLine(line);
            }
        }
    }

    function discardChanges(): void {
        editing = false;
        load();
    }

    // Called before another document takes this one's place: nothing typed gets lost.
    function aboutToClose(): void {
        if (editing && modified) {
            save();
        }
    }

    Component.onCompleted: load()

    // Both views wrap the text to their width, and wrapping a long text anew for every step
    // a panel border or the window is dragged takes too long. The code view has its own way
    // of keeping up (see CodeView.beginResize()); it is told here when the width begins to
    // change and when it has come to rest. Long rendered text cannot do that: it keeps its
    // width meanwhile, cut off or with empty space beside it, and is wrapped once at the end.
    Item {
        id: stage

        readonly property bool longRendered: root.rendered && area.length > 100000
        property real codeWidth: 0
        property real renderedWidth: 0

        function settle(): void {
            settling.stop();
            codeWidth = width;
            renderedWidth = width;
            codeView.endResize();
        }

        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true

        onWidthChanged: {
            if (root.code) {
                // In this order: the view prepares, then it gets its new width.
                codeView.beginResize();
                codeWidth = width;
                renderedWidth = width;
                if (codeView.resizing) {
                    settling.restart();
                }
            } else if (longRendered && renderedWidth > 0) {
                settling.restart();
            } else {
                settle();
            }
        }
        onLongRenderedChanged: settle()

        Timer {
            id: settling

            interval: Kirigami.Units.humanMoment / 12
            onTriggered: stage.settle()
        }

        // Behind the text while there is more room than it takes up.
        Rectangle {
            anchors.fill: parent
            Kirigami.Theme.colorSet: Kirigami.Theme.View
            Kirigami.Theme.inherit: false
            color: Kirigami.Theme.backgroundColor
        }

        CodeView {
            id: codeView

            width: stage.codeWidth
            height: parent.height
            visible: root.code
            readOnly: !root.editing
            syntax: root.syntax
            numbered: root.highlighted
            textFont: root.highlighted ? Kirigami.Theme.fixedWidthFont : Kirigami.Theme.defaultFont
            zoom: root.textZoom
            onZoomRequested: larger => root.zoomStep(larger)
            // Folding goes by brackets outside of strings, which only data formats promise to be
            // meaningful.
            foldable: root.structured || /json/i.test(root.syntax.name)
            marks: root.code && !root.editing ? root.codeHits : []
            markLength: root.findText.length
            onSaveRequested: root.save()
            onEscapePressed: root.finishEditing()
        }

        QQC2.ScrollView {
            id: proseScroll

            width: stage.renderedWidth
            height: parent.height
            visible: root.rendered

            QQC2.TextArea {
                id: area

                readOnly: !root.editing
                padding: Kirigami.Units.largeSpacing
                wrapMode: TextEdit.Wrap
                selectByMouse: true
                persistentSelection: true
                baseUrl: root.info.contentUrl ?? ""
                font.family: Kirigami.Theme.defaultFont.family
                font.pointSize: Kirigami.Theme.defaultFont.pointSize * root.textZoom
                background: Rectangle {
                    color: Kirigami.Theme.backgroundColor
                }
                Kirigami.Theme.colorSet: Kirigami.Theme.View
                Kirigami.Theme.inherit: false

                onLinkActivated: link => Qt.openUrlExternally(link)

                TextFind {
                    id: proseFind

                    edit: area
                    flickable: root.proseFlick
                    text: root.findText
                    active: root.rendered && !root.editing
                }

                WheelHandler {
                    acceptedModifiers: Qt.ControlModifier
                    onWheel: event => {
                        if (event.angleDelta.y !== 0) {
                            root.zoomStep(event.angleDelta.y > 0);
                        }
                    }
                }

                // With the text in focus, Select All means the text, not the documents in
                // the list, which is what the window's action of that name selects.
                Keys.onShortcutOverride: event => {
                    if (event.matches(StandardKey.SelectAll) || (event.matches(StandardKey.Copy) && area.selectedText.length > 0)) {
                        event.accepted = true;
                    }
                }
                Keys.onPressed: event => {
                    if (root.editing && event.matches(StandardKey.Save)) {
                        root.save();
                        event.accepted = true;
                    } else if (root.editing && event.key === Qt.Key_Escape) {
                        root.finishEditing();
                        event.accepted = true;
                    } else if (event.matches(StandardKey.SelectAll)) {
                        area.selectAll();
                        event.accepted = true;
                    } else if (event.matches(StandardKey.Copy) && area.selectedText.length > 0) {
                        area.copy();
                        event.accepted = true;
                    }
                }
            }
        }

        Loader {
            id: tableLoader

            anchors.fill: parent
            active: root.table
            visible: active

            sourceComponent: DataTable {
                path: root.info.contentPath ?? ""
                fileName: root.info.name ?? ""
                zoom: root.textZoom
                searchTerms: root.searchTerms
                findText: root.findText
                onZoomRequested: larger => root.zoomStep(larger)
            }
        }
    }
}
