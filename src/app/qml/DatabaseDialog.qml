// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The databases Indeks knows. One is open at a time; a click opens another one. A database is
// a folder with the index, the documents created in Indeks and the working copies, and can be
// on a network share.
Kirigami.Dialog {
    id: root

    property var databases: []
    property string problem

    signal addRequested()
    signal newRequested()
    /** The user wants to call the database at @p path something else; the caller asks what. */
    signal renameRequested(string name, string path)
    /** The user wants @p path off the list; the caller asks whether they are sure. */
    signal forgetRequested(string name, string path)

    function ask(): void {
        problem = "";
        reload();
        open();
    }

    function reload(): void {
        databases = App.databases();
    }

    title: i18nc("@title:window", "Databases")
    preferredWidth: Kirigami.Units.gridUnit * 30
    preferredHeight: Kirigami.Units.gridUnit * 22
    standardButtons: Kirigami.Dialog.Close
    customFooterActions: [
        Kirigami.Action {
            text: i18nc("@action:button", "Add Database…")
            icon.name: "document-open"
            onTriggered: root.addRequested()
        },
        Kirigami.Action {
            text: i18nc("@action:button", "New Database…")
            icon.name: "folder-new"
            onTriggered: root.newRequested()
        }
    ]

    Connections {
        target: App
        enabled: root.visible
        function onDatabasesChanged(): void {
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
            type: Kirigami.MessageType.Warning
            text: root.problem
        }
        Kirigami.Separator {
            Layout.fillWidth: true
        }
    }

    ListView {
        id: list

        implicitHeight: Kirigami.Units.gridUnit * 14
        model: root.databases
        clip: true

        delegate: QQC2.ItemDelegate {
            id: entry

            required property var modelData
            readonly property bool usable: modelData.active || modelData.problem.length === 0

            width: list.width
            highlighted: modelData.active
            Accessible.name: modelData.name

            onClicked: {
                if (modelData.active) {
                    root.close();
                    return;
                }
                root.problem = App.openDatabase(modelData.path);
                if (root.problem.length === 0) {
                    root.close();
                }
            }

            contentItem: RowLayout {
                spacing: Kirigami.Units.largeSpacing

                Kirigami.Icon {
                    source: entry.modelData.builtin ? "drive-harddisk" : entry.modelData.network ? "folder-remote" : "folder"
                    selected: entry.highlighted
                    opacity: entry.usable ? 1 : 0.5
                    implicitWidth: Kirigami.Units.iconSizes.medium
                    implicitHeight: implicitWidth
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    QQC2.Label {
                        Layout.fillWidth: true
                        text: entry.modelData.name
                        elide: Text.ElideRight
                        font.weight: entry.modelData.active ? Font.DemiBold : Font.Normal
                        color: entry.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: entry.modelData.path
                        textFormat: Text.PlainText
                        elide: Text.ElideMiddle
                        font: Kirigami.Theme.smallFont
                        color: entry.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.disabledTextColor
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: entry.modelData.active ? i18nc("@info state of a database", "Open")
                            : !entry.modelData.reachable ? i18nc("@info state of a database", "Not connected")
                            : entry.modelData.problem.length > 0 ? i18nc("@info state of a database", "Cannot be opened at the moment")
                            : i18nc("@info state of a database", "Click to open")
                        elide: Text.ElideRight
                        font: Kirigami.Theme.smallFont
                        color: entry.highlighted ? Kirigami.Theme.highlightedTextColor
                            : entry.usable ? Kirigami.Theme.disabledTextColor : Kirigami.Theme.neutralTextColor
                    }
                }
                QQC2.ToolButton {
                    icon.name: "edit-rename"
                    text: i18nc("@action:button", "Rename…")
                    display: QQC2.AbstractButton.IconOnly
                    onClicked: root.renameRequested(entry.modelData.name, entry.modelData.path)
                    QQC2.ToolTip.text: text
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                }
                QQC2.ToolButton {
                    visible: !entry.modelData.builtin
                    icon.name: "list-remove"
                    text: i18nc("@action:button", "Remove from This List")
                    display: QQC2.AbstractButton.IconOnly
                    onClicked: root.forgetRequested(entry.modelData.name, entry.modelData.path)
                    QQC2.ToolTip.text: text
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                }
            }
        }
    }
}
