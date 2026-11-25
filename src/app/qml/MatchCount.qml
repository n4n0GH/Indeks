// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

// Says how many documents the filters of a stack find at the moment, below the stack: for
// where filters say what something is for, a rule or a saved search, and no list beside them
// shows what they find.
//
// Looked up a moment after the filters change; not at all with a word of one or two letters
// among them, which takes seconds in a large index.
QQC2.Label {
    id: root

    required property FilterStack filterStack
    /** Nothing is counted while this is false: for an editor that is not in view. */
    property bool active: true

    property int count: -1
    readonly property bool countable: filterStack.query.trim().length > 0
        && !filterStack.filters.some(filter => (filter.type === "words" || filter.type === "content")
                                         && (filter.data?.text ?? "").split(/\s+/).some(word => word.replace(/"/g, "").length < 3))
    /** The number of documents found; -1 while it is not known. */
    readonly property int matches: countable ? count : -1

    visible: count >= 0 && countable
    font: Kirigami.Theme.smallFont
    color: Kirigami.Theme.disabledTextColor
    text: i18ncp("@info", "%1 document matches at the moment.", "%1 documents match at the moment.", Math.max(0, count))

    Connections {
        target: root.filterStack
        function onQueryChanged(): void {
            root.count = -1;
            counter.restart();
        }
    }
    Timer {
        id: counter

        interval: Kirigami.Units.humanMoment / 8
        onTriggered: root.count = root.countable && root.active ? App.library.count(root.filterStack.query) : -1
    }
}
