// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

// The place of one of the two panels in the row across the window (a SplitView), and how
// the panel comes and goes there: it slides in from its edge of the window and out again,
// the way the drawers of Kirigami do, and the middle of the window takes the room that is
// left at every step.
//
// The panel keeps its width on the way and moves with the edge of this place that faces
// the middle. So nothing in the panel is laid out anew while it slides; as much of it
// shows as there is room for. What is laid out at every step is the middle, which is what
// makes the two move as one.
Item {
    id: root

    /** "left" or "right": the edge of the window the panel is at. */
    required property string side
    /** Whether the panel is to be seen. */
    property bool open: true
    /** How wide the user has the panel, and how narrow and wide it may be made. */
    property real panelWidth: Kirigami.Units.gridUnit * 14
    property real leastWidth: Kirigami.Units.gridUnit * 9
    property real mostWidth: Kirigami.Units.gridUnit * 28

    /** How much of the panel is in view: 1 when it is all there, 0 when it is gone. */
    property real shown: open ? 1 : 0
    /** On its way in or out. */
    readonly property bool sliding: shown > 0 && shown < 1
    /** How wide the panel is when it is all there: the width it keeps while it slides. */
    property real restWidth: panelWidth
    // The window comes up with the panels where they were left, without a slide.
    property bool settled: false

    /** The panel. */
    default property alias panel: holder.data

    visible: shown > 0
    clip: sliding
    QQC2.SplitView.preferredWidth: sliding ? Math.round(restWidth * shown) : panelWidth
    QQC2.SplitView.minimumWidth: sliding ? 0 : leastWidth
    QQC2.SplitView.maximumWidth: mostWidth

    onWidthChanged: {
        if (shown === 1) {
            restWidth = width;
        }
    }
    onShownChanged: {
        if (shown === 1) {
            restWidth = width;
        }
    }
    Component.onCompleted: settled = true

    // As long and as eased as the drawers of Kirigami, which is none at all where the
    // desktop is set to do without animations.
    Behavior on shown {
        enabled: root.settled

        NumberAnimation {
            duration: Kirigami.Units.longDuration
            easing.type: root.open ? Easing.OutQuad : Easing.InQuad
        }
    }

    Item {
        id: holder

        x: root.side === "left" ? root.width - width : 0
        width: root.sliding ? root.restWidth : root.width
        height: root.height
    }
}
