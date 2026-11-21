// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// What Indeks is doing in the background: scanning, indexing, recognising text, fetching
// remote files. Slides in at the bottom of the sidebar while there is something to report
// and takes no room otherwise. A click asks for the list of everything that waits.
Item {
    id: root

    // Follows App.library.busy with a short delay, so that work which is over in an instant,
    // like re-reading one changed file, does not make the sidebar twitch.
    property bool shown: false
    // The last status that was reported; kept while sliding out, when there is none any more.
    property string text
    property string detail
    property real progress: -1

    readonly property bool busy: App.library.busy
    readonly property string status: App.library.status
    readonly property string statusDetail: App.library.statusDetail
    readonly property real statusProgress: App.library.progress

    signal clicked()

    onBusyChanged: settle.restart()
    onStatusChanged: update()
    onStatusDetailChanged: update()
    onStatusProgressChanged: update()

    function update(): void {
        if (status.length > 0) {
            text = status;
            detail = statusDetail;
            progress = statusProgress;
        }
    }

    // How much of the indicator has slid in, from 0 (gone) to 1.
    property real reveal: shown ? 1 : 0

    // The height is stated to the layout outright. Left to the implicit height alone, a layout
    // takes a height of nothing for "no opinion" and keeps the item at the size it had, so
    // the indicator would stay up for good after its work is done.
    implicitHeight: content.implicitHeight * reveal
    Layout.preferredHeight: implicitHeight
    visible: reveal > 0
    clip: true

    Behavior on reveal {
        NumberAnimation {
            duration: Kirigami.Units.longDuration
            easing.type: Easing.InOutQuad
        }
    }

    Timer {
        id: settle

        interval: Kirigami.Units.veryLongDuration
        onTriggered: {
            if (root.busy && root.status.length === 0 && !root.shown) {
                root.text = i18nc("@info:status", "Working…");
                root.detail = "";
                root.progress = -1;
            }
            root.shown = root.busy;
        }
    }

    ColumnLayout {
        id: content

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: 0

        Kirigami.Separator {
            Layout.fillWidth: true
        }
        QQC2.ItemDelegate {
            id: button

            Layout.fillWidth: true
            padding: Kirigami.Units.largeSpacing

            Accessible.name: root.text
            Accessible.description: root.detail

            QQC2.ToolTip.text: i18nc("@info:tooltip", "Show what is being indexed and what waits")
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

            onClicked: root.clicked()

            contentItem: ColumnLayout {
                spacing: Kirigami.Units.smallSpacing

                QQC2.Label {
                    Layout.fillWidth: true
                    text: root.text
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }
                QQC2.Label {
                    Layout.fillWidth: true
                    visible: root.detail.length > 0
                    text: root.detail
                    textFormat: Text.PlainText
                    elide: Text.ElideMiddle
                    maximumLineCount: 1
                    font: Kirigami.Theme.smallFont
                    color: Kirigami.Theme.disabledTextColor
                }
                QQC2.ProgressBar {
                    Layout.fillWidth: true
                    indeterminate: root.progress < 0
                    value: Math.max(0, root.progress)
                    // An idle bar must not keep animating behind the scenes.
                    visible: root.visible
                }
            }
        }
    }
}
