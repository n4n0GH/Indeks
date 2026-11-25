// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The tab of a view of the panels, while it is being dragged: what follows the pointer, and
// what the places it can be dropped on see coming (PanelArea, and the edges of the window
// for a panel that is not shown).
//
// There is one of these, made by the window. A tab that is dragged tells it so with
// begin(), moveTo() and finish(); where it is dropped is told by `view`, and the layout is
// asked to put the view there (PanelLayout::place()).
Item {
    id: root

    /** The view whose tab is being dragged; empty while none is. */
    property string view
    readonly property bool dragging: view.length > 0
    /** What the places to drop on go by. */
    readonly property string key: "application/x-indeks-panel-view"

    function begin(view: string, scenePoint: point): void {
        root.view = view;
        moveTo(scenePoint);
    }

    function moveTo(scenePoint: point): void {
        if (!parent) {
            return;
        }
        const place = parent.mapFromItem(null, scenePoint.x, scenePoint.y);
        x = place.x - width / 2;
        y = place.y - height / 2;
    }

    // Let go of: whatever is below it takes the view, if anything is.
    function finish(): void {
        if (dragging) {
            Drag.drop();
            view = "";
        }
    }

    width: chip.implicitWidth
    height: chip.implicitHeight
    z: 1000
    visible: dragging

    Drag.active: dragging
    Drag.keys: [key]
    Drag.hotSpot.x: width / 2
    Drag.hotSpot.y: height / 2

    Rectangle {
        id: chip

        implicitWidth: row.implicitWidth + Kirigami.Units.largeSpacing * 2
        implicitHeight: row.implicitHeight + Kirigami.Units.smallSpacing * 2
        radius: Kirigami.Units.cornerRadius
        color: Kirigami.Theme.backgroundColor
        border.color: Kirigami.Theme.highlightColor
        opacity: 0.9

        Kirigami.Theme.colorSet: Kirigami.Theme.Button
        Kirigami.Theme.inherit: false

        RowLayout {
            id: row

            anchors.centerIn: parent
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                source: root.dragging ? App.panels.icon(root.view) : ""
                implicitWidth: Kirigami.Units.iconSizes.smallMedium
                implicitHeight: implicitWidth
            }
            QQC2.Label {
                text: root.dragging ? App.panels.title(root.view) : ""
            }
        }
    }
}
