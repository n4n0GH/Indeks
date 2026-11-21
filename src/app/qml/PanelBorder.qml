// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import org.kde.kirigami as Kirigami

// The line between two panels of a split view, and nothing but the line.
//
// The style's own handle can be grabbed some way to either side of the line it draws, which
// takes half of a scroll bar that ends at the border away from the scroll bar. Here the place
// to grab a border by is the strip behind it; see ResizeGrip.
Kirigami.Separator {
    implicitWidth: 1
    implicitHeight: 1

    // Not to be taken hold of at all, or the split view and the strip would both move the
    // border when the pointer is right on the line: nothing counts as inside this.
    containmentMask: Item {
        width: 0
        height: 0
    }
}
