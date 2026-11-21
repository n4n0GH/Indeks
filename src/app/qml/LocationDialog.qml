// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Adds a location on a server to the index. Anything the file manager can open works.
Kirigami.Dialog {
    id: root

    function ask(): void {
        addressField.text = "";
        nameField.text = "";
        recursiveBox.checked = true;
        open();
        addressField.forceActiveFocus();
    }

    readonly property url location: App.locationFromInput(addressField.text)
    readonly property bool valid: location.toString().length > 0

    readonly property bool canSubmit: root.valid

    // Also reached with Enter from the text fields.
    function submit(): void {
        if (!canSubmit) {
            return;
        }
        const id = App.library.addSource(root.location, nameField.text, recursiveBox.checked);
        if (id > 0) {
            App.goTo("source:" + id);
            root.close();
        }
    }

    title: i18nc("@title:window", "Add Remote Location")
    padding: Kirigami.Units.largeSpacing
    preferredWidth: Kirigami.Units.gridUnit * 28
    standardButtons: Kirigami.Dialog.NoButton
    customFooterActions: [
        Kirigami.Action {
            text: i18nc("@action:button", "Add Location")
            icon.name: "list-add"
            enabled: root.canSubmit
            onTriggered: root.submit()
        },
        Kirigami.Action {
            text: i18nc("@action:button", "Cancel")
            icon.name: "dialog-cancel"
            onTriggered: root.close()
        }
    ]

    Kirigami.FormLayout {
        QQC2.TextField {
            id: addressField

            onAccepted: root.submit()

            Kirigami.FormData.label: i18nc("@label:textbox", "Address:")
            Layout.fillWidth: true
            placeholderText: i18nc("@info:placeholder example address, do not translate the protocol", "sftp://user@server/path/to/folder")
            inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase
        }
        QQC2.TextField {
            id: nameField

            onAccepted: root.submit()

            Kirigami.FormData.label: i18nc("@label:textbox", "Name:")
            Layout.fillWidth: true
            placeholderText: i18nc("@info:placeholder", "Optional, shown in the sidebar")
        }
        QQC2.CheckBox {
            id: recursiveBox

            text: i18nc("@option:check", "Include subfolders")
        }
        QQC2.Label {
            Layout.fillWidth: true
            Layout.maximumWidth: Kirigami.Units.gridUnit * 20
            text: i18nc("@info", "Use an address as the file manager shows it, for example starting with sftp://, smb:// or webdav://. Indeks reads the files to index them and keeps them findable while the server is out of reach. It never changes anything on the server.")
            wrapMode: Text.Wrap
            font: Kirigami.Theme.smallFont
            color: Kirigami.Theme.disabledTextColor
        }
    }
}
