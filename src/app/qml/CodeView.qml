// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.syntaxhighlighting as Syntax

// Shows and edits text as it is in the file: source code and data with line numbers, syntax
// colours and, for bracketed data such as JSON, folding; prose without the numbers. Lines that
// are too long wrap and continue below their own indentation. The approach is the one of
// Kreator's source editor; CodeLayout does the measuring.
//
// The view does not scroll a tall item behind a window, as a Flickable would. Where it is, is
// kept as the line at its top and how far the view is into that line, and the text is moved to
// match. That way the place holds whatever happens to the text around it: lines wrapping anew
// while the width changes, folding, and, for long texts, lines coming and going. Of a long text
// that is only being read, the TextEdit holds just the lines around the view (see
// CodeLayout.setSource()): scrolling and resizing then cost the same however long the text is.
Item {
    id: root

    property bool readOnly: true
    /** A KSyntaxHighlighting definition; without one the text stays uncoloured. */
    property var syntax
    /** Whether bracketed regions that span lines can be folded. */
    property bool foldable: false
    /** Whether lines get numbers. Prose is shown without, and in the font given. */
    property bool numbered: true
    /**
     * For writing code in a language that goes by indentation: with a width set, a new line
     * begins where the one before it did (one step further in after a colon), and the
     * tabulator key indents by that many spaces.
     */
    property int indentWidth: 0
    property font textFont: Kirigami.Theme.fixedWidthFont
    /** How large the text is shown: a factor on the size of the font. */
    property real zoom: 1
    readonly property font shownFont: zoom === 1 ? textFont
        : textFont.pointSize > 0 ? Qt.font({ family: textFont.family, pointSize: textFont.pointSize * zoom, weight: textFont.weight, italic: textFont.italic })
                                 : Qt.font({ family: textFont.family, pixelSize: Math.max(1, Math.round(textFont.pixelSize * zoom)), weight: textFont.weight, italic: textFont.italic })

    /** The text of the document. While a long text is only read, that is not all of it. */
    readonly property alias text: editor.text
    readonly property alias editor: editor
    readonly property alias lines: codeLayout
    readonly property real foldColumn: codeLayout.hasFoldableRegions ? Kirigami.Units.iconSizes.small + Kirigami.Units.smallSpacing : 0

    // ------------------------------------------------------------------ where the view is

    /** The line at the top of the view, counted in the whole text from 0. */
    property int topLine: 0
    // How far the top of the view is below the top of that line.
    property real into: 0
    // Everything below that asks the layout for positions has to follow relayouts. The revision
    // is made part of the value for that: a bare read of it, as a statement of its own, is
    // dropped when the binding is compiled, and the binding then never updates.
    readonly property real viewTop: codeLayout.revision > 0 ? Math.max(0, codeLayout.lineTop(topLine)) + into : 0
    readonly property real viewHeight: view.height
    readonly property int rows: Math.max(1, Math.ceil(viewHeight / Math.max(1, metrics.lineSpacing)))

    readonly property int currentLine: codeLayout.revision > 0 ? codeLayout.lineAt(editor.cursorPosition) : 0
    readonly property var visibleLines: codeLayout.revision > 0 ? codeLayout.visibleLines(viewTop - 100, viewTop + viewHeight + 100) : []
    readonly property var foldBadges: codeLayout.revision > 0 ? codeLayout.foldBadges(viewTop - 100, viewTop + viewHeight + 100) : []

    // How much text fills the view should it become as wide as the window: the measure for
    // what a long text keeps in the TextEdit around the view.
    readonly property int screenCharacters: {
        const columns = (Window.width > 0 ? Window.width : width) / Math.max(1, metrics.averageCharacterWidth);
        return Math.max(6000, Math.min(60000, Math.round(rows * columns)));
    }
    // Texts up to this length go into the TextEdit whole; they are quick enough that way.
    readonly property int wholeLimit: 50000

    /**
     * Places to mark in the text, as a search within the document found them: where each
     * begins, counted in the whole text and in ascending order, and how long they are.
     */
    property var marks: []
    property int markLength: 0

    signal saveRequested()
    signal escapePressed()
    /** The wheel was turned with Ctrl held: a step larger or smaller is asked for. */
    signal zoomRequested(bool larger)

    /**
     * Shows @p text from its beginning. If @p numberedLines is not empty, only those lines
     * (from 0) get a number, the one at the same place in @p lineNumbers: for text that is a
     * layout of the file's.
     */
    function show(text: string, numberedLines: var, lineNumbers: var): void {
        if (resizing) {
            // Another text takes this one's place in the middle of a change of width.
            codeLayout.endWindow();
            resizing = false;
        }
        allSelected = false;
        pendingReveal = -1;
        glide.stop();
        wheelWanted = wheelDone;
        codeLayout.numberedLines = numberedLines;
        codeLayout.lineNumbers = lineNumbers;
        topLine = 0;
        into = 0;
        if (readOnly && text.length > wholeLimit) {
            // Through the TextEdit itself first, so that it forgets whatever text it had.
            holding = true;
            editor.text = "";
            holding = false;
            codeLayout.setSource(text);
            holdLinesAround(0);
        } else {
            codeLayout.clearSource();
            editor.text = text;
            editor.cursorPosition = 0;
            codeLayout.relayout();
            codeLayout.redraw();
        }
        scrollTo(0);
    }

    // ---------------------------------------------------------------------------- scrolling

    /** Puts the top of the view at @p y, in the coordinates of the text as it is laid out. */
    function scrollTo(y: real): void {
        // Twice at most: if the lines that are laid out end before the place wanted, more are
        // fetched and the rest of the way is gone then.
        for (let round = 0; round < 3; ++round) {
            const first = codeLayout.firstLine === 0 ? 0 : codeLayout.top;
            const last = Math.max(first, codeLayout.bottom + editor.bottomPadding - viewHeight);
            const bounded = Math.max(first, Math.min(y, last));
            const line = codeLayout.lineAtY(Math.max(bounded, codeLayout.top));
            const before = codeLayout.revision;
            const wanted = y - bounded;
            topLine = line;
            into = bounded - Math.max(0, codeLayout.lineTop(line));
            fill();
            if (Math.abs(wanted) < 0.5 || codeLayout.revision === before) {
                return;
            }
            y = viewTop + wanted;
        }
    }

    function scrollBy(delta: real): void {
        if (delta !== 0) {
            scrollTo(viewTop + delta);
        }
    }

    function scrollToStart(): void {
        topLine = 0;
        into = 0;
        holdLinesAround(0);
        scrollTo(0);
    }

    function scrollToEnd(): void {
        const last = codeLayout.lineCount - 1;
        holdLinesAround(last);
        topLine = codeLayout.shownLineNear(last);
        into = 0;
        scrollTo(codeLayout.bottom + editor.bottomPadding - viewHeight);
    }

    /** Brings @p line to the top of the view. */
    function showLine(line: int): void {
        line = Math.max(0, Math.min(line, codeLayout.lineCount - 1));
        holdLinesAround(line);
        topLine = codeLayout.shownLineNear(line);
        into = 0;
        scrollTo(viewTop);
    }

    // Of a long text: makes the TextEdit hold the lines around @p line.
    function holdLinesAround(line: int): void {
        if (!codeLayout.partial) {
            return;
        }
        const wanted = codeLayout.linesAround(line, screenCharacters, 2 * screenCharacters, 1.5 * rows, 2.5 * rows);
        let first = wanted.first;
        let last = wanted.last;
        if (!allSelected && editor.selectionEnd > editor.selectionStart) {
            // What is selected stays in, or the selection would be cut down to what is held.
            // Within reason: a selection of megabytes is given up for it.
            const from = Math.min(first, codeLayout.lineAt(editor.selectionStart));
            const to = Math.max(last, codeLayout.lineAt(editor.selectionEnd));
            if (codeLayout.sourcePosition(to) - codeLayout.sourcePosition(from) < 20 * screenCharacters) {
                first = from;
                last = to;
            }
        }
        if (first === codeLayout.firstLine && last === codeLayout.lastLine) {
            return;
        }
        holding = true;
        codeLayout.showLines(first, last);
        if (allSelected) {
            editor.selectAll();
        }
        holding = false;
    }

    // Of a long text: fetches more lines when the view comes near the end of those held.
    function fill(): void {
        if (!codeLayout.partial || holding || viewHeight <= 0) {
            return;
        }
        const above = viewTop - codeLayout.top;
        const below = codeLayout.bottom - (viewTop + viewHeight);
        if ((codeLayout.firstLine > 0 && above < viewHeight) || (codeLayout.lastLine < codeLayout.lineCount - 1 && below < 1.5 * viewHeight)) {
            holdLinesAround(topLine);
        }
    }

    // The layout changed under the view: folding, wrapping, other lines held.
    function settle(): void {
        if (codeLayout.lineTop(topLine) < 0) {
            // The line at the top was folded away.
            topLine = codeLayout.shownLineNear(topLine);
            into = 0;
        }
        fill();
        // Not below the end of the text, should it have become shorter.
        if (codeLayout.lastLine >= codeLayout.lineCount - 1) {
            const last = Math.max(codeLayout.firstLine === 0 ? 0 : codeLayout.top, codeLayout.bottom + editor.bottomPadding - viewHeight);
            if (viewTop > last + 0.5) {
                scrollTo(last);
            }
        }
        revealPending();
    }

    property bool holding: false

    /** Selects @p length characters at @p position of the whole text and brings them into view. */
    function select(position: int, length: int): void {
        allSelected = false;
        const line = codeLayout.lineOfSource(position);
        holdLinesAround(line);
        const at = codeLayout.documentPosition(position);
        if (at < 0) {
            return;
        }
        codeLayout.reveal(at);
        editor.select(at, at + length);
        // Right after loading, the view may not have its size yet and the text is not wrapped
        // to it. Keep the place in sight through the relayouts that follow, then let go, so
        // that the view does not jump back there whenever it is resized.
        pendingReveal = position;
        revealTimer.restart();
        revealPending();
    }

    property int pendingReveal: -1

    function revealPending(): void {
        if (pendingReveal < 0 || viewHeight <= 0 || editor.width <= 0 || holding) {
            return;
        }
        const position = pendingReveal;
        const at = codeLayout.documentPosition(position);
        if (at < 0) {
            return;
        }
        // The call below changes the layout and comes back here.
        pendingReveal = -1;
        scrollTo(editor.positionToRectangle(at).y - viewHeight / 3);
        pendingReveal = position;
    }

    Timer {
        id: revealTimer

        interval: Kirigami.Units.humanMoment / 4
        onTriggered: root.pendingReveal = -1
    }

    function toggleFold(line: int): void {
        codeLayout.toggleFold(line);
        // The caret must not sit in lines that cannot be seen.
        if (codeLayout.lineTop(codeLayout.lineAt(editor.cursorPosition)) < 0 && codeLayout.documentPosition(codeLayout.sourcePosition(line)) >= 0) {
            editor.cursorPosition = codeLayout.lineStart(line);
        }
    }

    function scheduleLayout(): void {
        layoutTimer.restart();
    }

    function ensureVisible(rect: rect): void {
        if (rect.y < viewTop) {
            scrollTo(rect.y - Kirigami.Units.gridUnit);
        } else if (rect.y + rect.height > viewTop + viewHeight) {
            scrollTo(rect.y + rect.height - viewHeight + Kirigami.Units.gridUnit);
        }
    }

    // ------------------------------------------------------------------------ selecting all

    // All of a long text is selected, also what the TextEdit does not hold: what it holds is
    // shown as selected, and copying takes the whole text.
    // ------------------------------------------------------------------ indentation

    readonly property bool indenting: !readOnly && indentWidth > 0

    // Where the line begins that @p position is in. Read off the text itself: what the
    // layout knows about lines is a keystroke behind while someone types.
    function lineStartAt(position: int): int {
        return editor.text.lastIndexOf("\n", position - 1) + 1;
    }

    // What stands on the caret's line before the caret.
    function lineBeforeCaret(): string {
        return editor.getText(lineStartAt(editor.selectionStart), editor.selectionStart);
    }

    // A new line that begins where this one does, and a step further in after a colon.
    function breakLine(): void {
        const before = lineBeforeCaret();
        let indent = before.match(/^[ \t]*/)[0];
        if (/:\s*(#.*)?$/.test(before)) {
            indent += " ".repeat(indentWidth);
        }
        editor.remove(editor.selectionStart, editor.selectionEnd);
        editor.insert(editor.cursorPosition, "\n" + indent);
    }

    // The tabulator key: spaces up to the next step at the caret, or, with lines selected,
    // all of them a step in or out.
    function shiftLines(inwards: bool): void {
        if (inwards && editor.selectedText.length === 0) {
            const column = lineBeforeCaret().length;
            editor.insert(editor.cursorPosition, " ".repeat(indentWidth - column % indentWidth));
            return;
        }
        // From the last line up, so that the places of the lines above stay what they are.
        const first = lineStartAt(editor.selectionStart);
        let start = lineStartAt(Math.max(editor.selectionStart, editor.selectionEnd - 1));
        while (start >= first) {
            if (inwards) {
                editor.insert(start, " ".repeat(indentWidth));
            } else {
                const lead = editor.getText(start, start + indentWidth).match(/^ */)[0].length;
                editor.remove(start, start + lead);
            }
            if (start === 0) {
                break;
            }
            start = lineStartAt(start - 1);
        }
    }

    // Backspace inside the indentation takes a whole step back. False if the caret is elsewhere.
    function unindentAtCaret(): bool {
        if (editor.selectedText.length > 0) {
            return false;
        }
        const before = lineBeforeCaret();
        if (before.length === 0 || /[^ ]/.test(before)) {
            return false;
        }
        const count = before.length % indentWidth || indentWidth;
        editor.remove(editor.cursorPosition - count, editor.cursorPosition);
        return true;
    }

    property bool allSelected: false

    function selectAll(): void {
        holding = true;
        editor.selectAll();
        holding = false;
        allSelected = codeLayout.partial;
    }

    function copy(): void {
        if (allSelected && codeLayout.partial) {
            codeLayout.copySource();
        } else {
            editor.copy();
        }
    }

    // ------------------------------------------------------------- while the width changes
    //
    // Only for a long text that is in the TextEdit whole, which is one that is being edited.
    // Wrapping all of it anew with every step the border of the panel is dragged takes too
    // long, so from the first step until the width has come to rest only the lines in view
    // and some more after them are in the layout. Afterwards everything is laid out once at
    // the final width. The line at the top of the view stays where it is throughout.
    /** The width is changing; see beginResize(). */
    property bool resizing: false
    // Flipped to make the text be laid out anew at an unchanged width.
    property bool nudged: false

    /** To be called before the width changes, with every step; the first one counts. */
    function beginResize(): void {
        if (resizing || codeLayout.partial || editor.length < 20000 || viewHeight <= 0 || editor.width <= 0) {
            return;
        }
        if (layoutTimer.running) {
            layoutTimer.stop();
            codeLayout.relayout();
        }
        codeLayout.beginWindow(topLine, 2 * screenCharacters, 2 * rows);
        resizing = true;
    }

    /** To be called when the width has come to rest. */
    function endResize(): void {
        if (!resizing) {
            return;
        }
        layoutTimer.stop();
        codeLayout.endWindow();
        nudged = !nudged; // lays everything out at this width
        resizing = false;
        codeLayout.relayout();
    }

    CodeLayout {
        id: codeLayout

        document: editor.textDocument
        decorated: root.readOnly
        foldable: root.foldable
        topOffset: editor.topPadding
        onNumbersChanged: root.scheduleLayout()
        onLayoutChanged: root.settle()
        onFoldableChanged: root.scheduleLayout()
    }

    // Text tends to change several times in a row; measure once afterwards.
    Timer {
        id: layoutTimer

        interval: 0
        onTriggered: codeLayout.relayout()
    }

    // The largest line number there is to show, which is what the column of numbers is as wide as.
    property int largestNumber: 0

    Connections {
        target: codeLayout
        function onLayoutChanged(): void {
            if (root.largestNumber !== codeLayout.largestNumber) {
                root.largestNumber = codeLayout.largestNumber;
            }
        }
    }

    FontMetrics {
        id: metrics

        font: editor.font
    }

    onViewHeightChanged: settle()

    // --------------------------------------------------------------------- wheel and touch

    // A wheel turns in steps; the way is gone smoothly. A touchpad says how far by itself.
    property real wheelWanted: 0
    property real wheelDone: 0
    property real wheelApplied: 0

    onWheelDoneChanged: {
        const step = wheelDone - wheelApplied;
        wheelApplied = wheelDone;
        scrollBy(step);
    }

    NumberAnimation {
        id: glide

        target: root
        property: "wheelDone"
        duration: Kirigami.Units.longDuration
        easing.type: Easing.OutCubic
    }

    WheelHandler {
        target: null
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: event => {
            if (event.modifiers & Qt.ControlModifier) {
                if (event.angleDelta.y !== 0) {
                    root.zoomRequested(event.angleDelta.y > 0);
                }
            } else if (event.pixelDelta.y !== 0) {
                root.scrollBy(-event.pixelDelta.y);
            } else if (event.angleDelta.y !== 0) {
                root.wheelWanted -= event.angleDelta.y / 120 * Qt.styleHints.wheelScrollLines * metrics.lineSpacing;
                glide.stop();
                glide.from = root.wheelDone;
                glide.to = root.wheelWanted;
                glide.start();
            }
        }
    }

    Rectangle {
        id: view

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: bar.left
        clip: true
        Kirigami.Theme.colorSet: Kirigami.Theme.View
        Kirigami.Theme.inherit: false
        color: Kirigami.Theme.backgroundColor

        // A finger on the screen drags the text; with a mouse, dragging selects.
        DragHandler {
            property real last: 0

            target: null
            acceptedDevices: PointerDevice.TouchScreen
            onActiveChanged: last = 0
            onTranslationChanged: {
                root.scrollBy(last - translation.y);
                last = translation.y;
            }
        }

        // Behind the line numbers, as tall as the view however the text below it moves.
        Rectangle {
            id: gutter

            visible: root.numbered
            // By a number that is only touched when it changes. Read off the layout directly,
            // this would be worked out anew, in the middle of being worked out, every time
            // the width it gives the text makes the layout measure again.
            width: root.numbered ? metrics.advanceWidth(String(root.largestNumber)) + Kirigami.Units.largeSpacing * 2 + root.foldColumn : 0
            height: parent.height
            color: Kirigami.Theme.alternateBackgroundColor

            HoverHandler {
                id: gutterHover
            }
            Kirigami.Separator {
                anchors.right: parent.right
                width: 1
                height: parent.height
            }
        }

        // The text and what goes with its lines, moved up by as much as the view is down.
        Item {
            id: sheet

            y: -root.viewTop
            width: parent.width

            // Line numbers, each at the top of its line however many rows that takes.
            Repeater {
                model: root.numbered ? root.visibleLines : []

                delegate: QQC2.Label {
                    required property var modelData

                    // Placed by hand: these come and go with every scroll step, and anchors
                    // complain while one of them is on its way out.
                    x: gutter.width - width - (root.foldColumn > 0 ? Kirigami.Units.smallSpacing + root.foldColumn : Kirigami.Units.largeSpacing)
                    y: modelData.y
                    font: editor.font
                    color: modelData.line === root.currentLine && currentLineMark.visible ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
                    text: modelData.label
                }
            }
            Repeater {
                model: root.numbered ? root.visibleLines.filter(line => line.foldable) : []

                delegate: Item {
                    id: marker

                    required property var modelData

                    x: gutter.width - width - 1
                    y: modelData.y
                    width: root.foldColumn
                    height: metrics.lineSpacing

                    Kirigami.Icon {
                        anchors.centerIn: parent
                        width: Math.min(Kirigami.Units.iconSizes.small, parent.height)
                        height: width
                        source: marker.modelData.folded ? "go-next-symbolic" : "go-down-symbolic"
                        color: Kirigami.Theme.disabledTextColor
                        isMask: true
                        opacity: marker.modelData.folded || markerHover.hovered || gutterHover.hovered ? 1 : 0.35
                    }
                    HoverHandler {
                        id: markerHover

                        cursorShape: Qt.PointingHandCursor
                    }
                    TapHandler {
                        onTapped: root.toggleFold(marker.modelData.line)
                    }

                    QQC2.ToolTip.text: marker.modelData.folded ? i18nc("@info:tooltip", "Unfold") : i18nc("@info:tooltip", "Fold")
                    QQC2.ToolTip.visible: markerHover.hovered
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                }
            }

            // A plain TextEdit: the styled text area does not take purely vertical mouse
            // drags, so selecting lines by dragging down would not work with it.
            TextEdit {
                id: editor

                x: gutter.width
                width: view.width - gutter.width
                height: Math.max(root.viewTop + root.viewHeight, implicitHeight)
                readOnly: root.readOnly
                font: root.shownFont
                color: Kirigami.Theme.textColor
                selectionColor: Kirigami.Theme.highlightColor
                selectedTextColor: Kirigami.Theme.highlightedTextColor
                wrapMode: TextEdit.Wrap
                textFormat: TextEdit.PlainText
                selectByMouse: true
                persistentSelection: true
                tabStopDistance: codeLayout.tabWidth * metrics.advanceWidth(" ")
                leftPadding: root.numbered ? Kirigami.Units.smallSpacing * 2 : Kirigami.Units.largeSpacing
                // A hair's breadth that makes no difference to the eye, but to the layout,
                // which measures anew when the room for the text changes.
                rightPadding: (root.numbered ? Kirigami.Units.smallSpacing * 2 : Kirigami.Units.largeSpacing) + (root.nudged ? 0.01 : 0)
                topPadding: root.numbered ? Kirigami.Units.smallSpacing : Kirigami.Units.largeSpacing
                bottomPadding: topPadding
                activeFocusOnPress: true
                activeFocusOnTab: true

                Accessible.role: Accessible.EditableText
                Accessible.multiLine: true
                Accessible.readOnly: readOnly

                onTextChanged: {
                    if (!root.holding) {
                        root.scheduleLayout();
                    }
                }
                // At once, not a moment later: where the view is goes by where the lines
                // are, and for one frame the text would stand in the wrong place.
                onWidthChanged: {
                    layoutTimer.stop();
                    codeLayout.relayout();
                }
                onFontChanged: root.scheduleLayout()
                // Rows were added or taken away by wrapping: the lines sit elsewhere now.
                onContentHeightChanged: {
                    if (!root.holding) {
                        root.scheduleLayout();
                    }
                }
                onCursorRectangleChanged: {
                    if (!root.readOnly && activeFocus) {
                        root.ensureVisible(cursorRectangle);
                    }
                }
                onSelectedTextChanged: {
                    // Selecting something else ends "everything selected".
                    if (root.allSelected && !root.holding && (selectionStart !== 0 || selectionEnd !== length)) {
                        root.allSelected = false;
                    }
                }

                // With the text in focus, Select All means the text. Left alone, the key goes
                // to the window's action of that name, which selects all documents in the list.
                Keys.onShortcutOverride: event => {
                    if (event.matches(StandardKey.SelectAll) || (event.matches(StandardKey.Copy) && editor.selectedText.length > 0)) {
                        event.accepted = true;
                    }
                }
                Keys.onPressed: event => {
                    const step = metrics.lineSpacing;
                    if (event.matches(StandardKey.Copy) && editor.selectedText.length > 0) {
                        root.copy();
                    } else if (event.matches(StandardKey.SelectAll)) {
                        root.selectAll();
                    } else if (!root.readOnly && event.matches(StandardKey.Save)) {
                        root.saveRequested();
                    } else if (!root.readOnly && event.key === Qt.Key_Escape) {
                        root.escapePressed();
                    } else if (root.indenting && (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && event.modifiers === Qt.NoModifier) {
                        root.breakLine();
                    } else if (root.indenting && event.key === Qt.Key_Tab) {
                        root.shiftLines(true);
                    } else if (root.indenting && event.key === Qt.Key_Backtab) {
                        root.shiftLines(false);
                    } else if (root.indenting && event.key === Qt.Key_Backspace && event.modifiers === Qt.NoModifier && root.unindentAtCaret()) {
                        // done
                    } else if (root.readOnly && event.key === Qt.Key_PageDown) {
                        root.scrollBy(root.viewHeight - step);
                    } else if (root.readOnly && event.key === Qt.Key_PageUp) {
                        root.scrollBy(-(root.viewHeight - step));
                    } else if (root.readOnly && event.key === Qt.Key_Down) {
                        root.scrollBy(step);
                    } else if (root.readOnly && event.key === Qt.Key_Up) {
                        root.scrollBy(-step);
                    } else if (root.readOnly && event.key === Qt.Key_Home) {
                        root.scrollToStart();
                    } else if (root.readOnly && event.key === Qt.Key_End) {
                        root.scrollToEnd();
                    } else {
                        return;
                    }
                    event.accepted = true;
                }

                FindMarks {
                    edit: editor
                    positions: root.marks
                    length: root.markLength
                    // Of what is in view only, counted in the whole text as the marks are.
                    firstPosition: codeLayout.revision > 0 && root.marks.length > 0 ? codeLayout.sourcePosition(Math.max(0, codeLayout.lineAtY(root.viewTop))) : 0
                    lastPosition: {
                        if (codeLayout.revision <= 0 || root.marks.length === 0) {
                            return 0;
                        }
                        const line = codeLayout.lineAtY(root.viewTop + root.viewHeight);
                        return line < 0 ? 2147483647 : codeLayout.sourcePosition(line) + codeLayout.lineLength(line) + 1;
                    }
                    toEdit: position => codeLayout.documentPosition(position)
                    revision: codeLayout.revision
                }

                // The line the caret is in, with all its wrapped rows.
                Rectangle {
                    id: currentLineMark

                    z: -1
                    visible: (root.numbered && editor.activeFocus || !root.readOnly) && y >= 0
                    y: codeLayout.revision > 0 ? codeLayout.lineTop(root.currentLine) : -1
                    width: parent.width
                    height: codeLayout.revision > 0 ? codeLayout.lineHeight(root.currentLine) : 0
                    color: Qt.alpha(Kirigami.Theme.highlightColor, 0.08)
                }

                // What stands in for folded lines; a click brings them back.
                Repeater {
                    model: root.foldBadges

                    delegate: Rectangle {
                        id: badge

                        required property var modelData
                        readonly property rect at: codeLayout.revision > 0 && modelData.position >= 0 ? editor.positionToRectangle(modelData.position) : Qt.rect(0, 0, 0, 0)

                        x: at.x + Kirigami.Units.smallSpacing
                        y: at.y
                        width: badgeLabel.implicitWidth + Kirigami.Units.smallSpacing * 2
                        height: at.height
                        radius: Kirigami.Units.cornerRadius
                        color: badgeHover.hovered ? Qt.alpha(Kirigami.Theme.highlightColor, 0.3) : Kirigami.Theme.alternateBackgroundColor
                        border.width: 1
                        border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, Kirigami.Theme.frameContrast)

                        QQC2.Label {
                            id: badgeLabel

                            anchors.centerIn: parent
                            font: editor.font
                            color: Kirigami.Theme.disabledTextColor
                            text: badge.modelData.text
                        }
                        HoverHandler {
                            id: badgeHover

                            cursorShape: Qt.PointingHandCursor
                        }
                        TapHandler {
                            onTapped: root.toggleFold(badge.modelData.line)
                        }
                    }
                }

                HoverHandler {
                    cursorShape: Qt.IBeamCursor
                }
                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: contextMenu.popup()
                }
            }
        }
    }

    // ---------------------------------------------------------------------- the scroll bar
    //
    // It goes by how far into the text the view is, counted in characters, since how tall
    // the text is cannot be known without laying all of it out.

    // The place in the whole text of what is at @p y.
    function placeOf(y: real): real {
        const line = codeLayout.lineAtY(Math.max(y, codeLayout.top));
        const top = codeLayout.lineTop(line);
        const height = codeLayout.lineHeight(line);
        const share = top >= 0 && height > 0 ? Math.max(0, Math.min(1, (y - top) / height)) : 0;
        return codeLayout.sourcePosition(line) + share * codeLayout.lineLength(line);
    }
    readonly property real placeTop: codeLayout.revision > 0 ? placeOf(viewTop) : 0
    readonly property real placeBottom: codeLayout.revision > 0
        ? (viewTop + viewHeight >= codeLayout.bottom && codeLayout.lastLine >= codeLayout.lineCount - 1 ? codeLayout.length + 1 : placeOf(viewTop + viewHeight)) : 0
    readonly property real barSize: codeLayout.length > 0 ? Math.max(0, Math.min(1, (placeBottom - placeTop) / (codeLayout.length + 1))) : 1
    readonly property real barPosition: {
        const room = codeLayout.length + 1 - (placeBottom - placeTop);
        return room > 0 ? Math.max(0, Math.min(1 - barSize, placeTop / room * (1 - barSize))) : 0;
    }

    QQC2.ScrollBar {
        id: bar

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        orientation: Qt.Vertical
        // Always there: a bar that comes and goes changes the width there is for wrapping.
        policy: QQC2.ScrollBar.AlwaysOn
        // While it is dragged the handle keeps its size, and what it stands for: both
        // follow what is in view, and would change under the pointer otherwise.
        property real heldSize: 1
        property real heldRoom: 0
        property bool following: false

        size: pressed ? heldSize : root.barSize
        minimumSize: Math.min(1, Kirigami.Units.gridUnit * 2 / Math.max(1, height))
        stepSize: 0

        onPressedChanged: {
            if (pressed) {
                heldSize = root.barSize;
                heldRoom = codeLayout.length + 1 - (root.placeBottom - root.placeTop);
            }
        }
        onPositionChanged: {
            // Only where the user moves it to; not what follows from moving the view.
            if (!pressed || following) {
                return;
            }
            following = true;
            const share = size < 1 ? Math.max(0, Math.min(1, position / (1 - size))) : 0;
            if (share >= 0.999) {
                root.scrollToEnd();
            } else {
                const place = share * heldRoom;
                const line = codeLayout.lineOfSource(Math.round(place));
                root.holdLinesAround(line);
                root.topLine = codeLayout.shownLineNear(line);
                root.into = 0;
                const within = codeLayout.lineLength(line) > 0 ? (place - codeLayout.sourcePosition(line)) / codeLayout.lineLength(line) : 0;
                root.scrollTo(root.viewTop + within * codeLayout.lineHeight(root.topLine));
            }
            following = false;
        }

        Binding on position {
            when: !bar.pressed
            value: root.barPosition
            restoreMode: Binding.RestoreNone
        }
    }

    QQC2.Menu {
        id: contextMenu

        QQC2.MenuItem {
            text: i18nc("@action:inmenu", "Undo")
            icon.name: "edit-undo"
            visible: !root.readOnly
            height: visible ? implicitHeight : 0
            enabled: editor.canUndo
            onTriggered: editor.undo()
        }
        QQC2.MenuItem {
            text: i18nc("@action:inmenu", "Redo")
            icon.name: "edit-redo"
            visible: !root.readOnly
            height: visible ? implicitHeight : 0
            enabled: editor.canRedo
            onTriggered: editor.redo()
        }
        QQC2.MenuItem {
            text: i18nc("@action:inmenu", "Cut")
            icon.name: "edit-cut"
            visible: !root.readOnly
            height: visible ? implicitHeight : 0
            enabled: editor.selectedText.length > 0
            onTriggered: editor.cut()
        }
        QQC2.MenuItem {
            text: i18nc("@action:inmenu", "Copy")
            icon.name: "edit-copy"
            enabled: editor.selectedText.length > 0
            onTriggered: root.copy()
        }
        QQC2.MenuItem {
            text: i18nc("@action:inmenu", "Paste")
            icon.name: "edit-paste"
            visible: !root.readOnly
            height: visible ? implicitHeight : 0
            enabled: editor.canPaste
            onTriggered: editor.paste()
        }
        QQC2.MenuItem {
            text: i18nc("@action:inmenu", "Select All")
            icon.name: "edit-select-all"
            onTriggered: root.selectAll()
        }
        QQC2.MenuSeparator {
            visible: codeLayout.hasFoldableRegions
            height: visible ? implicitHeight : 0
        }
        QQC2.MenuItem {
            text: i18nc("@action:inmenu", "Fold All")
            visible: codeLayout.hasFoldableRegions
            height: visible ? implicitHeight : 0
            onTriggered: codeLayout.foldTopLevel(true)
        }
        QQC2.MenuItem {
            text: i18nc("@action:inmenu", "Unfold All")
            visible: codeLayout.hasFoldableRegions
            height: visible ? implicitHeight : 0
            enabled: codeLayout.hasFolds
            onTriggered: codeLayout.unfoldAll()
        }
    }

    Loader {
        active: !!root.syntax && root.syntax.name.length > 0 && root.syntax.name !== "None"
        sourceComponent: Syntax.SyntaxHighlighter {
            textEdit: editor
            definition: root.syntax
        }
    }
}
