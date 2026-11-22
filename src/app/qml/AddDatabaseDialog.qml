// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Dialogs as Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Adds a database that exists, on a NAS for example, by its index file. The file is what is
// chosen, not the folder: in a folder dialog a database looks like an empty folder, and a
// folder without one would be taken for one that was meant. A database that does not exist
// yet is started with NewDatabaseDialog.
Kirigami.Dialog {
    id: root

    readonly property var database: App.inspectDatabaseFile(fileField.text)
    readonly property bool canSubmit: (database.valid ?? false) && (database.problem ?? "").length === 0
    property string failure

    function ask(): void {
        nameField.text = "";
        fileField.text = "";
        failure = "";
        open();
    }

    // Not in ask(): a dialog that closes at the same moment would take the focus back.
    onOpened: fileField.forceActiveFocus()

    // Also reached with Enter from the text fields.
    function submit(): void {
        if (!canSubmit) {
            return;
        }
        failure = App.addDatabase(nameField.text, fileField.text);
        if (failure.length === 0) {
            close();
        }
    }

    title: i18nc("@title:window", "Add Database")
    padding: Kirigami.Units.largeSpacing
    preferredWidth: Kirigami.Units.gridUnit * 30
    standardButtons: Kirigami.Dialog.NoButton
    customFooterActions: [
        Kirigami.Action {
            text: i18nc("@action:button", "Open Database")
            icon.name: "document-open"
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
                Kirigami.FormData.label: i18nc("@label:textbox", "Index file:")
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                QQC2.TextField {
                    id: fileField

                    Layout.fillWidth: true
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 16
                    placeholderText: i18nc("@info:placeholder example of the index file of a database on a mounted network share", "/mnt/nas/Indeks/index.sqlite")
                    inputMethodHints: Qt.ImhNoAutoUppercase
                    onTextChanged: root.failure = ""
                    onAccepted: root.submit()
                }
                QQC2.Button {
                    icon.name: "document-open"
                    text: i18nc("@action:button", "Choose…")
                    onClicked: fileDialog.open()
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
            type: root.canSubmit ? Kirigami.MessageType.Information : Kirigami.MessageType.Warning
            text: {
                if (root.failure.length > 0) {
                    return root.failure;
                }
                if (!(root.database.valid ?? false)) {
                    return fileField.text.trim().length > 0
                        ? i18nc("@info %1 is a file name", "Choose the file “%1” of a database, on this computer or on a mounted share.", "index.sqlite")
                        : "";
                }
                if ((root.database.problem ?? "").length > 0) {
                    return root.database.problem;
                }
                return i18nc("@info %1 is a folder", "This is the index of the Indeks database in “%1”. It will be opened as it is.", root.database.path);
            }
        }

        QQC2.Label {
            Layout.fillWidth: true
            Layout.preferredWidth: Kirigami.Units.gridUnit * 26
            wrapMode: Text.WordWrap
            color: Kirigami.Theme.disabledTextColor
            text: (root.database.network ?? false)
                ? i18nc("@info", "This database is on a network share. It can be open on one computer at a time, and Indeks goes back to the local database when the share cannot be reached.")
                : i18nc("@info %1 is a file name", "A database is a folder, and the file “%1” in it is its index. The documents you create in Indeks and your working copies are kept in the same folder.", "index.sqlite")
        }
    }

    Dialogs.FileDialog {
        id: fileDialog

        title: i18nc("@title:window", "Choose the Index File of the Database")
        fileMode: Dialogs.FileDialog.OpenFile
        // Nothing but index files is shown, so that a folder shows at a glance whether it
        // holds a database.
        nameFilters: [i18nc("@item:inlistbox kind of file in a file dialog; keep the part in brackets as it is", "Indeks database (index.sqlite)")]
        onAccepted: fileField.text = App.localPath(selectedFile.toString())
    }
}
