// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Makes a document out of what is on the clipboard: shows what was found there, and asks
// what to make of it, where it can be more than one thing, and for a name. An address of a
// website can become a website: a document that holds the address. The content was
// taken from the clipboard when the command was given (App.clipping()), so what is shown
// here is what is stored.
Kirigami.Dialog {
    id: root

    property var clipping: ({})
    property string format
    /** The name that was put into the field, to tell whether the user wrote another. */
    property string suggestedName

    readonly property var formats: clipping.formats ?? []
    readonly property bool picture: format === "picture"
    readonly property bool canSubmit: nameField.text.trim().length > 0

    function ask(): void {
        clipping = App.clipping();
        if (formats.length === 0) {
            return;
        }
        nameField.text = "";
        suggestedName = "";
        format = clipping.format;
        formatBox.currentIndex = formats.findIndex(entry => entry.format === format);
        suggestName();
        open();
    }

    // A name that follows from the content, as long as none was typed.
    function suggestName(): void {
        if (nameField.text !== suggestedName || formats.length === 0) {
            return;
        }
        const textName = clipping.textName ?? "";
        // Asked of the format itself: this runs when the format changes, which is before
        // what is bound to it has followed.
        const websiteName = clipping.websiteName ?? "";
        suggestedName = format === "picture" ? clipping.pictureName
            : format === "website" && websiteName.length > 0 ? websiteName
            : textName.length > 0 ? textName : i18nc("default name of a new document", "Untitled");
        nameField.text = suggestedName;
    }

    // Also reached with Enter from the name.
    function submit(): void {
        if (!canSubmit) {
            return;
        }
        App.createFromClipboard(nameField.text.trim(), format);
        close();
    }

    onFormatChanged: suggestName()
    // Not in ask(): a dialog that closes at the same moment would take the focus back.
    onOpened: {
        nameField.forceActiveFocus();
        nameField.selectAll();
    }
    onClosed: {
        // What was taken from the clipboard is let go of, the picture above all.
        App.forgetClipboard();
        clipping = {};
        nameField.text = "";
        suggestedName = "";
    }

    title: i18nc("@title:window", "New Document from Clipboard")
    padding: Kirigami.Units.largeSpacing
    preferredWidth: Kirigami.Units.gridUnit * 30
    standardButtons: Kirigami.Dialog.NoButton
    customFooterActions: [
        Kirigami.Action {
            text: i18nc("@action:button", "Create Document")
            icon.name: "document-new"
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

        // What was found: the beginning of the text, or the picture.
        QQC2.Frame {
            Layout.fillWidth: true
            Layout.preferredWidth: Kirigami.Units.gridUnit * 26
            padding: Kirigami.Units.largeSpacing

            contentItem: ColumnLayout {
                spacing: Kirigami.Units.smallSpacing

                QQC2.Label {
                    Layout.fillWidth: true
                    visible: !root.picture
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    maximumLineCount: 6
                    elide: Text.ElideRight
                    text: root.clipping.text ?? ""
                }
                Image {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.maximumWidth: parent.width
                    Layout.maximumHeight: Kirigami.Units.gridUnit * 9
                    Layout.preferredWidth: implicitWidth
                    Layout.preferredHeight: implicitHeight
                    visible: root.picture
                    fillMode: Image.PreserveAspectFit
                    source: root.picture ? (root.clipping.thumbnail ?? "") : ""
                }
                QQC2.Label {
                    Layout.fillWidth: true
                    visible: root.picture
                    horizontalAlignment: Text.AlignHCenter
                    font: Kirigami.Theme.smallFont
                    color: Kirigami.Theme.disabledTextColor
                    text: i18nc("@info size of a picture, width and height", "%1 × %2 pixels", root.clipping.width ?? 0, root.clipping.height ?? 0)
                }
            }
        }

        Kirigami.FormLayout {
            Layout.fillWidth: true

            QQC2.ComboBox {
                id: formatBox

                Kirigami.FormData.label: i18nc("@label:listbox what to make of the clipboard", "Create a:")
                Layout.fillWidth: true
                // No choice where the clipboard holds one thing only.
                visible: root.formats.length > 1
                model: root.formats
                textRole: "label"
                valueRole: "format"
                onActivated: root.format = currentValue
            }
            QQC2.TextField {
                id: nameField

                Kirigami.FormData.label: i18nc("@label:textbox", "Name:")
                Layout.fillWidth: true
                // Room for a name that was made of a line of text.
                Layout.preferredWidth: Kirigami.Units.gridUnit * 22
                onAccepted: root.submit()
            }
        }

        QQC2.Label {
            Layout.fillWidth: true
            Layout.preferredWidth: Kirigami.Units.gridUnit * 26
            wrapMode: Text.WordWrap
            color: Kirigami.Theme.disabledTextColor
            text: root.format === "html"
                ? i18nc("@info", "The document is stored in Indeks and starts out in the inbox. Pictures within the text are not taken along.")
                : root.format === "website"
                ? i18nc("@info", "The document holds the address and nothing of the page itself. Opening it shows the website in your browser. It is stored in Indeks and starts out in the inbox.")
                : i18nc("@info", "The document is stored in Indeks and starts out in the inbox.")
        }
    }
}
