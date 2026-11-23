// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Picks the script to run on the selected documents. Writing and changing scripts is done in
// the list of scripts, under the Automation tab of the sidebar.
Kirigami.Dialog {
    id: root

    required property Actions actions

    property var scripts: []
    readonly property int selected: App.documents.selectedIds.length
    readonly property string problem: visible ? App.library.scriptsProblem() : ""

    function show(): void {
        reload();
        open();
    }

    function reload(): void {
        scripts = App.library.scripts();
    }

    title: i18ncp("@title:window", "Run Script on %1 Document", "Run Script on %1 Documents", selected)
    preferredWidth: Kirigami.Units.gridUnit * 34
    preferredHeight: Kirigami.Units.gridUnit * 24
    standardButtons: Kirigami.Dialog.Close
    customFooterActions: [
        Kirigami.Action {
            text: i18nc("@action:button", "Go to Scripts")
            icon.name: "text-x-python"
            tooltip: i18nc("@info:tooltip", "The list of scripts, where they are written and changed")
            onTriggered: {
                root.close();
                root.actions.scripts.trigger();
            }
        }
    ]

    Connections {
        target: App.library
        enabled: root.visible
        function onScriptsChanged(): void {
            root.reload();
        }
    }

    header: ColumnLayout {
        spacing: 0

        Kirigami.Heading {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.largeSpacing
            level: 2
            text: root.title
            elide: Text.ElideRight
        }
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            position: Kirigami.InlineMessage.Position.Header
            visible: root.problem.length > 0
            type: Kirigami.MessageType.Error
            text: root.problem
        }
        Kirigami.Separator {
            Layout.fillWidth: true
        }
    }

    ListView {
        id: list

        implicitHeight: Kirigami.Units.gridUnit * 16
        model: root.scripts
        clip: true

        delegate: QQC2.ItemDelegate {
            id: entry

            required property int index
            required property var modelData
            readonly property bool runnable: modelData.allowed && root.problem.length === 0 && root.selected > 0

            width: ListView.view?.width ?? 0
            enabled: runnable
            onClicked: {
                App.runScript(modelData.file);
                root.close();
            }

            contentItem: RowLayout {
                spacing: Kirigami.Units.largeSpacing

                Kirigami.Icon {
                    source: entry.modelData.allowed ? "text-x-python" : "dialog-warning"
                    implicitWidth: Kirigami.Units.iconSizes.smallMedium
                    implicitHeight: implicitWidth
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    QQC2.Label {
                        Layout.fillWidth: true
                        text: entry.modelData.name
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        visible: text.length > 0
                        text: entry.modelData.allowed ? entry.modelData.description
                                                      : i18nc("@info", "Written or changed somewhere else. Read it and allow it in the list of scripts before it runs on this computer.")
                        textFormat: Text.PlainText
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        font: Kirigami.Theme.smallFont
                        color: Kirigami.Theme.disabledTextColor
                    }
                }
            }
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 4
            visible: list.count === 0
            icon.name: "text-x-python"
            text: i18nc("@info:placeholder", "No scripts yet")
            explanation: i18nc("@info:placeholder", "Scripts are written in the list of scripts, under the Automation tab of the sidebar.")
        }
    }
}
