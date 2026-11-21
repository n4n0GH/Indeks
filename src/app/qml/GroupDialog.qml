// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Lets the user pick the group the selected documents go into.
Kirigami.Dialog {
    id: root

    property var ids: []
    property var groups: []

    signal newGroupRequested()

    function ask(ids: var): void {
        root.ids = ids;
        reload();
        open();
    }

    function reload(): void {
        groups = App.library.groups();
    }

    title: i18nc("@title:window", "Add to Group")
    preferredWidth: Kirigami.Units.gridUnit * 22
    preferredHeight: Kirigami.Units.gridUnit * 20
    standardButtons: Kirigami.Dialog.Cancel
    customFooterActions: [
        Kirigami.Action {
            text: i18nc("@action:button", "New Group…")
            icon.name: "folder-new"
            onTriggered: root.newGroupRequested()
        }
    ]

    Connections {
        target: App.library
        enabled: root.visible
        function onChanged(): void {
            root.reload();
        }
    }

    ListView {
        id: list

        implicitHeight: Kirigami.Units.gridUnit * 14
        model: root.groups
        clip: true

        delegate: QQC2.ItemDelegate {
            id: entry

            required property var modelData

            width: ListView.view.width
            leftPadding: Kirigami.Units.largeSpacing + modelData.depth * Kirigami.Units.gridUnit
            text: modelData.name
            icon.name: "folder"
            onClicked: {
                App.library.addToGroup(root.ids, modelData.id);
                root.close();
            }
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 2
            visible: list.count === 0
            icon.name: "folder"
            text: i18nc("@info:placeholder", "No groups yet")
            explanation: i18nc("@info:placeholder", "Groups collect documents that belong together, wherever their files are stored.")
        }
    }
}
