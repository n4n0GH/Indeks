// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Dialogs as Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// What a new installation starts with, before there is a database: start a new one, in the
// usual folder unless another is chosen, or open one that exists. There is no App yet; what
// this window needs it has from Welcome.
QQC2.ApplicationWindow {
    id: root

    required property Welcome backend

    readonly property bool starting: newChoice.checked
    readonly property var folder: backend.inspectNewFolder(folderField.text)
    readonly property var database: backend.inspectIndexFile(fileField.text)
    readonly property var chosen: starting ? folder : database
    readonly property bool canSubmit: (chosen.valid ?? false) && (chosen.problem ?? "").length === 0
    // Something is in the folder already: the database would put its files among it.
    readonly property bool crowded: starting && canSubmit && !(folder.empty ?? true)
    property string failure

    function submit(): void {
        if (!canSubmit) {
            return;
        }
        failure = starting ? backend.startNew(folderField.text) : backend.openExisting(fileField.text);
    }

    title: i18nc("@title:window", "Welcome to Indeks")
    width: Kirigami.Units.gridUnit * 34
    // Room for the message that comes and goes, so that the buttons stay where they are.
    height: Kirigami.Units.gridUnit * 22
    minimumWidth: Kirigami.Units.gridUnit * 26
    minimumHeight: Kirigami.Units.gridUnit * 20
    visible: true

    ColumnLayout {
        id: content

        anchors.fill: parent
        anchors.margins: Kirigami.Units.gridUnit
        spacing: Kirigami.Units.largeSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing

            Kirigami.Icon {
                Layout.preferredWidth: Kirigami.Units.iconSizes.huge
                Layout.preferredHeight: Kirigami.Units.iconSizes.huge
                Layout.alignment: Qt.AlignTop
                source: "qrc:/io.github.n4n0gh.indeks.svg"
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Heading {
                    Layout.fillWidth: true
                    text: i18nc("@title", "Welcome to Indeks")
                    wrapMode: Text.WordWrap
                }
                QQC2.Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: i18nc("@info", "Indeks keeps what it knows in a database: a folder with its index, the documents you create in Indeks and the copies it makes for editing. Your files stay where they are. Start a new database, or open one you have already.")
                }
            }
        }

        Kirigami.Separator {
            Layout.fillWidth: true
        }

        QQC2.RadioButton {
            id: newChoice

            text: i18nc("@option:radio", "Start a new database")
            checked: true
            onToggled: root.failure = ""
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.gridUnit * 1.5
            enabled: newChoice.checked
            spacing: Kirigami.Units.smallSpacing

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                QQC2.Label {
                    text: i18nc("@label:textbox", "Folder:")
                }
                QQC2.TextField {
                    id: folderField

                    Layout.fillWidth: true
                    text: root.backend.standardFolder
                    inputMethodHints: Qt.ImhNoAutoUppercase
                    Accessible.name: i18nc("@label:textbox", "Folder of the new database")
                    onTextChanged: root.failure = ""
                    onAccepted: root.submit()
                }
                QQC2.Button {
                    icon.name: "document-open-folder"
                    text: i18nc("@action:button", "Choose…")
                    onClicked: folderDialog.open()
                }
            }
            QQC2.Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Kirigami.Theme.disabledTextColor
                visible: newChoice.checked
                text: (root.folder.standard ?? false)
                    ? i18nc("@info", "This is the usual folder for the database. If you have no other in mind, leave it as it is.")
                    : (root.folder.network ?? false)
                        ? i18nc("@info", "This folder is on a network share. The database can be open on one computer at a time, and Indeks uses a database on this computer while the share cannot be reached.")
                        : i18nc("@info", "The index, the documents you create in Indeks and your working copies will be stored in this folder.")
            }
        }

        QQC2.RadioButton {
            id: existingChoice

            text: i18nc("@option:radio", "Open an existing database")
            onToggled: {
                root.failure = "";
                if (checked) {
                    fileField.forceActiveFocus();
                }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.gridUnit * 1.5
            enabled: existingChoice.checked
            spacing: Kirigami.Units.smallSpacing

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                QQC2.Label {
                    text: i18nc("@label:textbox", "Index file:")
                }
                QQC2.TextField {
                    id: fileField

                    Layout.fillWidth: true
                    placeholderText: i18nc("@info:placeholder example of the index file of a database on a mounted network share", "/mnt/nas/Indeks/index.sqlite")
                    inputMethodHints: Qt.ImhNoAutoUppercase
                    Accessible.name: i18nc("@label:textbox", "Index file of the database to open")
                    onTextChanged: root.failure = ""
                    onAccepted: root.submit()
                }
                QQC2.Button {
                    icon.name: "document-open"
                    text: i18nc("@action:button", "Choose…")
                    onClicked: fileDialog.open()
                }
            }
            QQC2.Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Kirigami.Theme.disabledTextColor
                visible: existingChoice.checked
                text: i18nc("@info %1 is a file name", "A database is a folder, and the file “%1” in it is its index. Choose that file.", "index.sqlite")
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: text.length > 0
            type: Kirigami.MessageType.Warning
            text: {
                if (root.failure.length > 0) {
                    return root.failure;
                }
                if (!(root.chosen.valid ?? false)) {
                    if (root.starting) {
                        return folderField.text.trim().length > 0 ? i18nc("@info", "Choose a folder on this computer or on a mounted share.") : "";
                    }
                    return fileField.text.trim().length > 0
                        ? i18nc("@info %1 is a file name", "Choose the file “%1” of a database, on this computer or on a mounted share.", "index.sqlite")
                        : "";
                }
                if ((root.chosen.problem ?? "").length > 0) {
                    return root.chosen.problem;
                }
                return root.crowded
                    ? i18nc("@info %1 is a file name", "This folder is not empty. The new database would put its file “%1” and its folders among what is there. A folder of its own keeps a database apart.", "index.sqlite")
                    : "";
            }
        }

        Item {
            Layout.fillHeight: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Item {
                Layout.fillWidth: true
            }
            QQC2.Button {
                icon.name: root.starting ? "folder-new" : "document-open"
                text: !root.starting ? i18nc("@action:button", "Open Database")
                    : root.crowded ? i18nc("@action:button", "Create Database Here Anyway")
                                   : i18nc("@action:button", "Create Database")
                enabled: root.canSubmit
                highlighted: true
                onClicked: root.submit()
            }
            QQC2.Button {
                icon.name: "application-exit"
                text: i18nc("@action:button", "Quit")
                onClicked: root.close()
            }
        }
    }

    Dialogs.FolderDialog {
        id: folderDialog

        title: i18nc("@title:window", "Choose a Folder for the New Database")
        // Where Indeks would put its database if it were not asked.
        currentFolder: root.backend.standardFolderUrl
        onAccepted: folderField.text = root.backend.localPath(selectedFolder.toString())
    }

    Dialogs.FileDialog {
        id: fileDialog

        title: i18nc("@title:window", "Choose the Index File of the Database")
        fileMode: Dialogs.FileDialog.OpenFile
        nameFilters: [i18nc("@item:inlistbox kind of file in a file dialog; keep the part in brackets as it is", "Indeks database (index.sqlite)")]
        onAccepted: fileField.text = root.backend.localPath(selectedFile.toString())
    }
}
