// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The scripts of the open database: run one on the selected documents, write a new one, change
// or remove one.
Kirigami.Dialog {
    id: root

    property var scripts: []
    readonly property int selected: App.documents.selectedIds.length
    readonly property string problem: visible ? App.library.scriptsProblem() : ""

    signal editRequested(string file)
    signal createRequested()
    signal deleteRequested(string file, string name, var rules)
    signal outputRequested()

    function show(): void {
        reload();
        open();
    }

    function reload(): void {
        scripts = App.library.scripts();
    }

    title: i18nc("@title:window", "Scripts")
    preferredWidth: Kirigami.Units.gridUnit * 42
    preferredHeight: Kirigami.Units.gridUnit * 28
    standardButtons: Kirigami.Dialog.Close
    customFooterActions: [
        Kirigami.Action {
            text: i18nc("@action:button", "New Script…")
            icon.name: "list-add"
            onTriggered: root.createRequested()
        },
        Kirigami.Action {
            text: i18nc("@action:button", "Open Folder")
            icon.name: "document-open-folder"
            tooltip: i18nc("@info:tooltip", "Scripts are files in the folder of the database and can be written with any editor")
            onTriggered: App.openScriptsFolder()
        },
        Kirigami.Action {
            text: i18nc("@action:button", "Output")
            icon.name: "view-list-text"
            tooltip: i18nc("@info:tooltip", "What the scripts printed when they last ran, and the errors they ran into")
            onTriggered: root.outputRequested()
        },
        Kirigami.Action {
            text: i18nc("@action:button", "Stop")
            icon.name: "process-stop"
            visible: App.library.scriptsRunning
            onTriggered: App.library.stopScripts()
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

        implicitHeight: Kirigami.Units.gridUnit * 20
        model: root.scripts
        clip: true

        delegate: QQC2.ItemDelegate {
            id: entry

            required property int index
            required property var modelData
            readonly property bool runnable: modelData.allowed && root.problem.length === 0

            width: ListView.view.width
            // The buttons do the work; the line itself opens the script.
            onClicked: root.editRequested(modelData.file)

            contentItem: RowLayout {
                spacing: Kirigami.Units.largeSpacing

                Kirigami.Icon {
                    source: entry.modelData.allowed ? "text-x-python" : "dialog-warning"
                    implicitWidth: Kirigami.Units.iconSizes.medium
                    implicitHeight: Kirigami.Units.iconSizes.medium
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
                        text: {
                            if (!entry.modelData.allowed) {
                                return i18nc("@info", "Written or changed somewhere else. Open it, read it, and allow it before it runs on this computer.");
                            }
                            const parts = [];
                            if (entry.modelData.description.length > 0) {
                                parts.push(entry.modelData.description);
                            }
                            if (entry.modelData.rules.length > 0) {
                                parts.push(i18ncp("@info the rules that run a script", "Run by the rule %2", "Run by the rules %2", entry.modelData.rules.length, entry.modelData.rules.join(", ")));
                            }
                            return parts.join(" · ");
                        }
                        textFormat: Text.PlainText
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        font: Kirigami.Theme.smallFont
                        color: entry.modelData.allowed ? Kirigami.Theme.disabledTextColor : Kirigami.Theme.neutralTextColor
                    }
                }
                QQC2.Button {
                    text: root.selected > 0 ? i18ncp("@action:button run a script on the selected documents", "Run on %1 Document", "Run on %1 Documents", root.selected)
                                            : i18nc("@action:button", "Run")
                    icon.name: "system-run"
                    enabled: entry.runnable && root.selected > 0
                    onClicked: {
                        App.runScript(entry.modelData.file);
                        root.close();
                    }

                    QQC2.ToolTip.text: root.selected > 0 ? i18nc("@info:tooltip", "Runs the script on the documents selected in the list")
                                                         : i18nc("@info:tooltip", "Select the documents to run the script on in the list first")
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                }
                QQC2.ToolButton {
                    text: i18nc("@action:button", "Edit…")
                    icon.name: "document-edit"
                    display: QQC2.AbstractButton.IconOnly
                    onClicked: root.editRequested(entry.modelData.file)

                    QQC2.ToolTip.text: text
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                }
                QQC2.ToolButton {
                    text: i18nc("@action:button", "Move to Trash…")
                    icon.name: "edit-delete"
                    display: QQC2.AbstractButton.IconOnly
                    onClicked: root.deleteRequested(entry.modelData.file, entry.modelData.name, entry.modelData.rules)

                    QQC2.ToolTip.text: text
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                }
            }
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 4
            visible: list.count === 0
            icon.name: "text-x-python"
            text: i18nc("@info:placeholder", "No scripts yet")
            explanation: i18nc("@info:placeholder", "A script is a small Python program that looks at documents and tags, groups, pins or annotates them. Run it on the documents you select, or have a rule run it.")
        }
    }
}
