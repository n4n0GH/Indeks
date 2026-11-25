// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick

// Searching within the text a TextEdit shows, for the views that show a document's text in
// one: finds every place `text` stands at, whatever its case, marks them (FindMarks) and
// brings the one asked for into view. A child of the TextEdit.
//
// What a view that can be searched has to offer the preview is the same for all of them:
// see PreviewPane. This is that for a TextEdit, and the places are found with positionsIn()
// by the views that keep their text elsewhere, too.
Item {
    id: root

    /** The TextEdit or TextArea that shows the text. */
    required property Item edit
    /** What scrolls it, for bringing a hit into view. */
    property Flickable flickable
    /** What to find. */
    property string text
    /** Nothing is searched while this is false, e.g. while the text is being written. */
    property bool active: true

    property var positions: []
    readonly property int count: positions.length
    /** There are more places than were counted. */
    property bool more: false
    /** The hit the reader was brought to last; -1 for none. */
    property int index: -1
    // The text that was searched, as it reads on screen.
    property string searched

    // Past this many nobody steps through them one by one.
    readonly property int most: 5000

    /** Where @p needle stands in @p text, whatever its case: @p limit places at most, then one more. */
    function positionsIn(text: string, needle: string, limit: int): var {
        const found = [];
        const wanted = needle.toLowerCase();
        if (wanted.length === 0) {
            return found;
        }
        const lower = text.toLowerCase();
        // Lowering a text can change its length, in a few alphabets; places would be off then.
        const hay = lower.length === text.length ? lower : text;
        let at = hay.indexOf(wanted);
        while (at >= 0 && found.length <= limit) {
            found.push(at);
            at = hay.indexOf(wanted, at + wanted.length);
        }
        return found;
    }

    /** What the list of hits says about the place @p position of @p text: the words around it. */
    function described(text: string, position: int, length: int, label: string): var {
        const start = Math.max(0, position - 40);
        const before = text.slice(start, position).replace(/\s+/g, " ");
        const after = text.slice(position + length, position + length + 80).replace(/\s+/g, " ");
        return {
            label: label,
            // From a whole word on, where the passage does not begin with the text.
            before: start > 0 ? before.replace(/^\S*\s/, "") : before.replace(/^\s+/, ""),
            match: text.slice(position, position + length),
            after: after
        };
    }

    function run(): void {
        index = -1;
        if (!active || text.length === 0) {
            positions = [];
            more = false;
            searched = "";
            return;
        }
        searched = edit.getText(0, edit.length);
        const found = positionsIn(searched, text, most);
        more = found.length > most;
        positions = more ? found.slice(0, most) : found;
    }

    function hit(at: int): var {
        return at >= 0 && at < count ? described(searched, positions[at], text.length, "") : ({});
    }

    function show(at: int): void {
        if (at < 0 || at >= count) {
            return;
        }
        index = at;
        const position = positions[at];
        edit.select(position, position + text.length);
        if (flickable) {
            // A third of the way down, with what comes before it in sight.
            const place = edit.mapToItem(flickable.contentItem, 0, edit.positionToRectangle(position).y).y;
            const last = Math.max(0, flickable.contentHeight - flickable.height);
            if (place < flickable.contentY || place > flickable.contentY + flickable.height * 0.8) {
                flickable.contentY = Math.max(0, Math.min(last, place - flickable.height / 3));
            }
        }
    }

    onTextChanged: run()
    onActiveChanged: run()

    Connections {
        target: root.edit
        enabled: root.active && root.text.length > 0
        // Another text in the same view: another message, the other way of showing one.
        function onTextChanged(): void {
            Qt.callLater(root.run);
        }
    }

    FindMarks {
        edit: root.edit
        positions: root.positions
        length: root.text.length
        // The marks of a whole text of ordinary length are few; of what is in view where
        // the text scrolls.
        firstPosition: root.flickable && root.count > most / 10
            ? root.edit.positionAt(0, root.flickable.contentItem.mapToItem(root.edit, 0, root.flickable.contentY).y) : 0
        lastPosition: root.flickable && root.count > most / 10
            ? root.edit.positionAt(root.edit.width, root.flickable.contentItem.mapToItem(root.edit, 0, root.flickable.contentY + root.flickable.height).y) + 1
            : root.edit.length + 1
        revision: root.edit.contentHeight + root.edit.contentWidth
    }
}
