// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import org.kde.kirigami as Kirigami

// A zone to grab the border between two panels by. The border itself is a line a pixel wide,
// too thin to hit; this shows the resize cursor over a strip one can hit and reports where the
// pointer drags the border to.
//
// The strip begins at the border line and reaches into the panel behind it (to the right, or
// below) only. What ends at a border is often a scroll bar, and that must stay usable over its
// whole width; what begins behind a border is margin.
Item {
    id: root

    /** True for a border between a panel above and one below. */
    property bool horizontalBorder: false
    /**
     * Where the border line begins (its left or upper edge), along the axis it is dragged on,
     * in the parent's coordinates.
     */
    property real border: 0
    /** How far the zone reaches from there. */
    readonly property real reach: Kirigami.Units.largeSpacing + Kirigami.Units.smallSpacing / 2
    readonly property bool active: hover.hovered || drag.active
    /** A border is being dragged right now. */
    readonly property bool dragging: drag.active

    /** The pointer drags the border; @p position is where its line is to begin. */
    signal moved(real position)

    x: horizontalBorder ? 0 : border
    y: horizontalBorder ? border : 0
    width: horizontalBorder ? (parent?.width ?? 0) : reach
    height: horizontalBorder ? reach : (parent?.height ?? 0)
    z: 10

    HoverHandler {
        id: hover

        cursorShape: root.horizontalBorder ? Qt.SplitVCursor : Qt.SplitHCursor
    }
    DragHandler {
        id: drag

        // Nothing is moved by the handler itself; the panel's size follows the pointer.
        target: null
        dragThreshold: 0
        // Once the border is being dragged it stays so until the button is let go, whatever
        // the pointer passes over: lists and rows that take drags of their own after a few
        // pixels must not take this one.
        grabPermissions: PointerHandler.CanTakeOverFromAnything
        cursorShape: root.horizontalBorder ? Qt.SplitVCursor : Qt.SplitHCursor

        // How far behind the border line the pointer took hold; the line keeps that distance
        // and does not jump to the pointer.
        property real hold: 0

        function along(scenePoint: point): real {
            const point = root.parent.mapFromItem(null, scenePoint.x, scenePoint.y);
            return root.horizontalBorder ? point.y : point.x;
        }

        onActiveChanged: {
            if (active && root.parent) {
                hold = along(centroid.scenePressPosition) - root.border;
            }
        }
        onCentroidChanged: {
            if (active && root.parent) {
                root.moved(along(centroid.scenePosition) - hold);
            }
        }
    }

    // Shows which border is about to move.
    Rectangle {
        visible: root.active
        x: 0
        y: 0
        width: root.horizontalBorder ? parent.width : 2
        height: root.horizontalBorder ? 2 : parent.height
        color: Kirigami.Theme.highlightColor
        opacity: drag.active ? 1 : 0.6
    }
}
