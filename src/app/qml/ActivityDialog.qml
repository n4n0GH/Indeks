// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The indexer's to-do list: what it is working on at the top, with a progress bar, and below it
// every file and location that is known to wait.
Kirigami.Dialog {
    id: root

    title: i18nc("@title:window", "Indexing Activity")
    preferredWidth: Kirigami.Units.gridUnit * 34
    preferredHeight: Kirigami.Units.gridUnit * 28
    standardButtons: Kirigami.Dialog.Close
    customFooterActions: [
        // Indexing takes care of itself; a script of the user's may need calling off.
        Kirigami.Action {
            text: i18nc("@action:button", "Stop Scripts")
            icon.name: "process-stop"
            visible: App.library.scriptsRunning
            onTriggered: App.library.stopScripts()
        }
    ]

    header: ColumnLayout {
        spacing: 0

        Kirigami.Heading {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.largeSpacing
            Layout.bottomMargin: Kirigami.Units.smallSpacing
            level: 2
            text: root.title
            elide: Text.ElideRight
        }
        // The same summary as in the sidebar, with the progress of the whole batch.
        QQC2.Label {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing
            text: {
                if (App.opening) {
                    return i18nc("@info:status", "Opening database…");
                }
                if (!App.library.busy) {
                    return i18nc("@info:status", "Nothing is being indexed at the moment.");
                }
                return App.library.status.length > 0 ? App.library.status : i18nc("@info:status", "Working…");
            }
            elide: Text.ElideRight
            color: Kirigami.Theme.disabledTextColor
        }
        QQC2.ProgressBar {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.largeSpacing
            Layout.topMargin: Kirigami.Units.smallSpacing
            visible: App.library.busy && App.library.progress >= 0
            value: Math.max(0, App.library.progress)
        }
        Item {
            Layout.fillWidth: true
            implicitHeight: Kirigami.Units.largeSpacing
            visible: !(App.library.busy && App.library.progress >= 0)
        }
        Kirigami.Separator {
            Layout.fillWidth: true
        }
    }

    ListView {
        id: list

        implicitHeight: Kirigami.Units.gridUnit * 20
        model: App.activity
        clip: true
        reuseItems: true

        delegate: Item {
            id: entry

            required property int index
            required property string name
            required property string location
            required property string sizeText
            required property string stateText
            required property string iconName
            required property bool active
            required property real progress

            width: ListView.view.width
            implicitHeight: row.implicitHeight + Kirigami.Units.largeSpacing * 2

            Accessible.role: Accessible.ListItem
            Accessible.name: i18nc("@info accessible name: file name, state", "%1, %2", name, stateText)

            RowLayout {
                id: row

                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.largeSpacing

                Kirigami.Icon {
                    Layout.alignment: Qt.AlignTop
                    source: entry.iconName
                    fallback: "unknown"
                    implicitWidth: Kirigami.Units.iconSizes.medium
                    implicitHeight: Kirigami.Units.iconSizes.medium
                    opacity: entry.active ? 1 : 0.6
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.largeSpacing

                        QQC2.Label {
                            Layout.fillWidth: true
                            text: entry.name
                            textFormat: Text.PlainText
                            elide: Text.ElideMiddle
                            maximumLineCount: 1
                            font.weight: entry.active ? Font.DemiBold : Font.Normal
                        }
                        QQC2.Label {
                            visible: text.length > 0
                            text: entry.sizeText
                            color: Kirigami.Theme.disabledTextColor
                        }
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: entry.location.length > 0
                            ? i18nc("@info state of a file in the queue, then the folder it is in", "%1 · %2", entry.stateText, entry.location)
                            : entry.stateText
                        textFormat: Text.PlainText
                        elide: Text.ElideMiddle
                        maximumLineCount: 1
                        font: Kirigami.Theme.smallFont
                        color: Kirigami.Theme.disabledTextColor
                    }
                    QQC2.ProgressBar {
                        Layout.fillWidth: true
                        visible: entry.active
                        // Mostly there is no telling how far Indeks is inside one file; text
                        // recognition by page and transfers are the exceptions.
                        indeterminate: entry.progress < 0
                        value: Math.max(0, entry.progress)
                    }
                }
            }
            Kirigami.Separator {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.leftMargin: Kirigami.Units.largeSpacing
                anchors.rightMargin: Kirigami.Units.largeSpacing
                visible: entry.index < list.count - 1
            }
        }

        footer: QQC2.Label {
            width: ListView.view.width
            visible: App.activity.overflow > 0
            height: visible ? implicitHeight : 0
            padding: Kirigami.Units.largeSpacing
            horizontalAlignment: Text.AlignHCenter
            text: i18ncp("@info", "…and %1 more document waiting for text recognition", "…and %1 more documents waiting for text recognition", App.activity.overflow)
            color: Kirigami.Theme.disabledTextColor
            wrapMode: Text.WordWrap
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 2
            visible: list.count === 0
            icon.name: "checkmark"
            text: i18nc("@info:placeholder", "Nothing to do")
            explanation: i18nc("@info:placeholder", "Files that are being indexed, and those waiting for it, are listed here.")
        }
    }
}
