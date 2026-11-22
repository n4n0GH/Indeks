// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Dialogs as Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Starts a new, empty database in a folder of the user's choosing, on a NAS for example. A
// database that exists is added with AddDatabaseDialog; here, a folder that holds one is
// turned down, and one that holds anything else is warned about.
Kirigami.Dialog {
    id: root

    readonly property var folder: App.inspectNewDatabaseFolder(folderField.text)
    readonly property bool canSubmit: (folder.valid ?? false) && (folder.problem ?? "").length === 0
    // Something is in the folder already: the database would put its files among it.
    readonly property bool crowded: canSubmit && !(folder.empty ?? true)
    property string failure

    function ask(): void {
        nameField.text = "";
        folderField.text = "";
        failure = "";
        open();
    }

    // Not in ask(): a dialog that closes at the same moment would take the focus back.
    onOpened: folderField.forceActiveFocus()

    // Also reached with Enter from the text fields.
    function submit(): void {
        if (!canSubmit) {
            return;
        }
        failure = App.createDatabase(nameField.text, folderField.text);
        if (failure.length === 0) {
            close();
        }
    }

    title: i18nc("@title:window", "New Database")
    padding: Kirigami.Units.largeSpacing
    preferredWidth: Kirigami.Units.gridUnit * 30
    standardButtons: Kirigami.Dialog.NoButton
    customFooterActions: [
        Kirigami.Action {
            text: root.crowded ? i18nc("@action:button", "Create Database Here Anyway") : i18nc("@action:button", "Create Database")
            icon.name: "folder-new"
            enabled: root.canSubmit
            onTriggered: root.submit()
        },
        Kirigami.Action {
            text: i18nc("@action:button", "Cancel")
            icon.name: "dialog-cancel"
            onTriggered: root.close()
        }
    ]

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        Kirigami.FormLayout {
            Layout.fillWidth: true

            RowLayout {
                Kirigami.FormData.label: i18nc("@label:textbox", "Folder:")
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                QQC2.TextField {
                    id: folderField

                    Layout.fillWidth: true
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 16
                    placeholderText: i18nc("@info:placeholder example of a folder on a mounted network share", "/mnt/nas/Indeks")
                    inputMethodHints: Qt.ImhNoAutoUppercase
                    onTextChanged: root.failure = ""
                    onAccepted: root.submit()
                }
                QQC2.Button {
                    icon.name: "document-open-folder"
                    text: i18nc("@action:button", "Choose…")
                    onClicked: folderDialog.open()
                }
            }
            QQC2.TextField {
                id: nameField

                Kirigami.FormData.label: i18nc("@label:textbox", "Name:")
                Layout.fillWidth: true
                placeholderText: i18nc("@info:placeholder", "Optional; the folder's name otherwise")
                onAccepted: root.submit()
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            // Long texts wrap instead of making the dialog as wide as they are.
            Layout.preferredWidth: Kirigami.Units.gridUnit * 26
            visible: text.length > 0
            type: root.canSubmit && !root.crowded && root.failure.length === 0 ? Kirigami.MessageType.Information : Kirigami.MessageType.Warning
            text: {
                if (root.failure.length > 0) {
                    return root.failure;
                }
                if (!(root.folder.valid ?? false)) {
                    return folderField.text.trim().length > 0 ? i18nc("@info", "Choose a folder on this computer or on a mounted share.") : "";
                }
                if ((root.folder.problem ?? "").length > 0) {
                    return root.folder.problem;
                }
                return root.crowded
                    ? i18nc("@info %1 is a file name", "This folder is not empty. The new database would put its file “%1” and its folders among what is there. A folder of its own keeps a database apart.", "index.sqlite")
                    : i18nc("@info", "A new, empty database will be started in this folder.");
            }
        }

        QQC2.Label {
            Layout.fillWidth: true
            Layout.preferredWidth: Kirigami.Units.gridUnit * 26
            wrapMode: Text.WordWrap
            color: Kirigami.Theme.disabledTextColor
            text: (root.folder.network ?? false)
                ? i18nc("@info", "This folder is on a network share. The index, the documents you create in Indeks and your working copies are stored there. The database can be open on one computer at a time, and Indeks goes back to the local database when the share cannot be reached.")
                : i18nc("@info", "The index, the documents you create in Indeks and your working copies are stored in the folder of the database. Files that are only indexed stay where they are.")
        }
    }

    Dialogs.FolderDialog {
        id: folderDialog

        title: i18nc("@title:window", "Choose a Folder for the New Database")
        onAccepted: folderField.text = App.localPath(selectedFolder.toString())
    }
}
