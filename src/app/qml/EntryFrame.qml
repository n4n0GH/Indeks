// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

// The box around a list of entries (see EntryRow), looking like any other list in a dialog.
// While the list is empty it says what belongs into it.
QQC2.Control {
    id: root

    /** What to say while there are no entries. */
    property string hint
    property bool empty: true

    // Each side by itself: the style sets them one by one, which a plain `padding` loses to.
    leftPadding: 1
    rightPadding: 1
    topPadding: 1
    bottomPadding: 1
    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false

    background: Rectangle {
        color: Kirigami.Theme.backgroundColor
        border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, Kirigami.Theme.frameContrast)
        radius: Kirigami.Units.cornerRadius

        QQC2.Label {
            anchors.fill: parent
            anchors.leftMargin: Kirigami.Units.largeSpacing
            anchors.rightMargin: Kirigami.Units.largeSpacing
            visible: root.empty
            verticalAlignment: Text.AlignVCenter
            color: Kirigami.Theme.disabledTextColor
            text: root.hint
            elide: Text.ElideRight
        }
    }
}
