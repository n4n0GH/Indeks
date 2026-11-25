// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import org.kde.kirigami as Kirigami

// Marks the places a search within the document found, over the text of a TextEdit, whose
// child this is. The one hit the reader is at is shown by the selection of the text; these
// are all of them, as with a marker pen.
//
// Only what is in view is marked: a common word is found thousands of times in a long text.
Item {
    id: root

    /** The TextEdit or TextArea whose text is marked. */
    required property Item edit
    /** Where the hits begin, in ascending order, and how long each is. */
    property var positions: []
    property int length: 0
    /** The stretch of the text that is in view, counted as `positions` are. */
    property int firstPosition: 0
    property int lastPosition: 0
    /** From a position as `positions` count them to one of the TextEdit; -1 where it holds none. */
    property var toEdit: position => position
    /** Whatever changes when the text is laid out anew, so that the marks follow. */
    property real revision: 0

    // More marks than this in one view would only be noise.
    readonly property int most: 500

    readonly property var marks: {
        // Laying the text out anew moves every mark. Made part of a value: a bare read of a
        // property, as a statement of its own, is dropped when the binding is compiled.
        const laidOut = root.revision + root.edit.width + root.edit.height;
        const found = [];
        if (root.length <= 0 || root.positions.length === 0 || root.lastPosition <= root.firstPosition || isNaN(laidOut)) {
            return found;
        }
        // The first hit that ends after the start of what is in view.
        let low = 0;
        let high = root.positions.length;
        while (low < high) {
            const middle = (low + high) >> 1;
            if (root.positions[middle] + root.length <= root.firstPosition) {
                low = middle + 1;
            } else {
                high = middle;
            }
        }
        for (let i = low; i < root.positions.length && root.positions[i] < root.lastPosition && found.length < root.most; ++i) {
            const from = root.toEdit(root.positions[i]);
            const to = root.toEdit(root.positions[i] + root.length);
            if (from < 0 || to < 0) {
                continue;
            }
            const first = root.edit.positionToRectangle(from);
            const last = root.edit.positionToRectangle(to);
            if (Math.abs(first.y - last.y) < 1) {
                found.push(Qt.rect(first.x, first.y, last.x - first.x, first.height));
            } else {
                // Wrapped: to the end of its row, and from the beginning of the row it ends in.
                found.push(Qt.rect(first.x, first.y, root.edit.width - root.edit.rightPadding - first.x, first.height));
                found.push(Qt.rect(root.edit.leftPadding, last.y, last.x - root.edit.leftPadding, last.height));
            }
        }
        return found;
    }

    Repeater {
        model: root.marks

        delegate: Rectangle {
            required property rect modelData

            x: modelData.x
            y: modelData.y
            width: modelData.width
            height: modelData.height
            radius: 2
            color: Qt.alpha(Kirigami.Theme.neutralTextColor, 0.3)
        }
    }
}
