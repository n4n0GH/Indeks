// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Right sidebar: what Indeks knows about the selected document, and the place to tag it,
// file it into groups and take notes on it.
QQC2.Pane {
    id: root

    required property Actions actions

    readonly property real documentId: App.documents.selectionCount === 1 ? App.documents.currentId : 0
    readonly property int selectionCount: App.documents.selectionCount
    property var info: ({})
    property var notes: []

    /** A note that is stuck to a page was clicked: the page is wanted in view. */
    signal noteActivated(var note)

    padding: 0
    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    function refresh(): void {
        info = documentId > 0 ? App.library.documentInfo(documentId) : {};
        notes = documentId > 0 ? App.library.notes(documentId) : [];
        if (!commentField.activeFocus) {
            commentField.text = info.comment ?? "";
        }
    }

    onDocumentIdChanged: {
        commentField.focus = false;
        refresh();
    }
    Component.onCompleted: refresh()

    Connections {
        target: App.library
        function onDocumentChanged(id: real): void {
            if (id === root.documentId) {
                root.refresh();
            }
        }
        function onChanged(): void {
            root.refresh();
        }
    }

    component SectionTitle: Kirigami.Heading {
        Layout.fillWidth: true
        Layout.topMargin: Kirigami.Units.largeSpacing
        level: 5
        elide: Text.ElideRight
    }

    component Fact: RowLayout {
        property alias label: factLabel.text
        property alias value: factValue.text

        Layout.fillWidth: true
        visible: factValue.text.length > 0
        spacing: Kirigami.Units.smallSpacing

        QQC2.Label {
            id: factLabel

            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: Kirigami.Units.gridUnit * 6
            horizontalAlignment: Text.AlignRight
            wrapMode: Text.Wrap
            color: Kirigami.Theme.disabledTextColor
        }
        Kirigami.SelectableLabel {
            id: factValue

            Layout.fillWidth: true
            textFormat: Text.PlainText
            wrapMode: Text.WrapAnywhere
        }
    }

    contentItem: Item {
        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 2
            visible: root.selectionCount !== 1
            icon.name: root.selectionCount > 1 ? "document-multiple" : "documentinfo"
            text: root.selectionCount > 1
                ? i18ncp("@info:placeholder", "%1 document selected", "%1 documents selected", root.selectionCount)
                : i18nc("@info:placeholder", "No document selected")
            explanation: root.selectionCount > 1 ? i18nc("@info:placeholder", "Tags and groups can be given to all of them at once.") : ""

            QQC2.Button {
                Layout.alignment: Qt.AlignHCenter
                visible: root.selectionCount > 1
                action: root.actions.tag
            }
            QQC2.Button {
                Layout.alignment: Qt.AlignHCenter
                visible: root.selectionCount > 1
                action: root.actions.group
            }
        }

        QQC2.ScrollView {
            id: scroll

            anchors.fill: parent
            visible: root.selectionCount === 1
            contentWidth: availableWidth
            QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

            ColumnLayout {
                width: scroll.availableWidth
                spacing: Kirigami.Units.smallSpacing

                // Name and type
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.margins: Kirigami.Units.largeSpacing
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Icon {
                        Layout.alignment: Qt.AlignHCenter
                        source: root.info.icon ?? "unknown"
                        implicitWidth: Kirigami.Units.iconSizes.huge
                        implicitHeight: implicitWidth
                    }
                    Kirigami.Heading {
                        Layout.fillWidth: true
                        level: 4
                        text: root.info.name ?? ""
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WrapAnywhere
                        maximumLineCount: 4
                        elide: Text.ElideRight
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: root.info.mimeComment ?? ""
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        color: Kirigami.Theme.disabledTextColor
                    }
                }

                Kirigami.InlineMessage {
                    Layout.fillWidth: true
                    Layout.leftMargin: Kirigami.Units.largeSpacing
                    Layout.rightMargin: Kirigami.Units.largeSpacing
                    visible: root.info.missing ?? false
                    type: Kirigami.MessageType.Warning
                    text: (root.info.trashed ?? false)
                        ? i18nc("@info", "The file is gone from where it was, which is why this document is in the trash. If the file comes back, so does the document. Its text, tags and notes are kept until the trash is emptied.")
                        : i18nc("@info", "The file is no longer where it was. Its text, tags and notes are kept.")
                }
                Kirigami.InlineMessage {
                    Layout.fillWidth: true
                    Layout.leftMargin: Kirigami.Units.largeSpacing
                    Layout.rightMargin: Kirigami.Units.largeSpacing
                    visible: (root.info.offline ?? false) && !(root.info.missing ?? false)
                    type: Kirigami.MessageType.Information
                    text: i18nc("@info", "The location “%1” is not connected.", root.info.sourceName ?? "")
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: Kirigami.Units.largeSpacing
                    Layout.rightMargin: Kirigami.Units.largeSpacing
                    Layout.bottomMargin: Kirigami.Units.largeSpacing
                    spacing: Kirigami.Units.smallSpacing

                    // Facts
                    Fact {
                        label: i18nc("@label", "Size:")
                        value: root.info.sizeText ?? ""
                    }
                    Fact {
                        label: i18nc("@label", "Modified:")
                        value: root.info.modifiedText ?? ""
                    }
                    Fact {
                        label: i18nc("@label", "Added:")
                        value: root.info.addedText ?? ""
                    }
                    Fact {
                        label: i18nc("@label", "Location:")
                        value: root.info.location ?? ""
                    }
                    Fact {
                        label: i18nc("@label indexed location a document comes from", "Indexed from:")
                        value: root.info.sourceName ?? ""
                    }
                    Fact {
                        label: i18nc("@label", "Pages:")
                        value: (root.info.pages ?? 0) > 0 ? root.info.pages : ""
                    }
                    Fact {
                        label: i18nc("@label optical character recognition, keep short", "OCR:")
                        value: {
                            switch (root.info.ocr ?? 0) {
                            case 1:
                                return i18nc("@info OCR state", "Waiting");
                            case 2:
                                return i18nc("@info OCR state", "Done");
                            case 3:
                                return i18nc("@info OCR state", "Failed");
                            }
                            return "";
                        }
                    }

                    // Tags
                    SectionTitle {
                        text: i18nc("@title:group", "Tags")
                    }
                    Flow {
                        Layout.fillWidth: true
                        visible: (root.info.tags ?? []).length > 0
                        spacing: Kirigami.Units.smallSpacing

                        Repeater {
                            model: root.info.tags ?? []

                            Kirigami.Chip {
                                required property string modelData

                                text: modelData
                                icon.name: "tag"
                                checkable: false
                                onRemoved: App.library.removeTag([root.documentId], modelData)
                            }
                        }
                    }
                    Kirigami.ActionTextField {
                        id: tagField

                        Layout.fillWidth: true
                        placeholderText: i18nc("@info:placeholder", "Add a tag…")
                        onAccepted: {
                            if (text.trim().length > 0) {
                                App.library.addTag([root.documentId], text);
                                text = "";
                            }
                        }
                        rightActions: [
                            Kirigami.Action {
                                icon.name: "list-add"
                                text: i18nc("@action:button", "Add Tag")
                                visible: tagField.text.length > 0
                                onTriggered: tagField.accepted()
                            }
                        ]
                    }

                    // Groups
                    SectionTitle {
                        text: i18nc("@title:group", "Groups")
                    }
                    Flow {
                        Layout.fillWidth: true
                        visible: (root.info.groups ?? []).length > 0
                        spacing: Kirigami.Units.smallSpacing

                        Repeater {
                            model: root.info.groups ?? []

                            Kirigami.Chip {
                                required property var modelData

                                text: modelData.name
                                icon.name: "folder"
                                checkable: false
                                onRemoved: App.library.removeFromGroup([root.documentId], modelData.id)
                            }
                        }
                    }
                    QQC2.Button {
                        action: root.actions.group
                    }

                    // Comment
                    SectionTitle {
                        text: i18nc("@title:group", "Comment")
                    }
                    QQC2.TextArea {
                        id: commentField

                        Layout.fillWidth: true
                        Layout.minimumHeight: Kirigami.Units.gridUnit * 4
                        wrapMode: TextEdit.Wrap
                        placeholderText: i18nc("@info:placeholder", "Anything worth remembering about this document. Comments are searchable.")
                        onEditingFinished: if (text !== (root.info.comment ?? "")) {
                            App.library.setComment(root.documentId, text);
                        }
                    }

                    // Notes
                    SectionTitle {
                        text: i18nc("@title:group", "Notes")
                    }
                    Repeater {
                        model: root.notes

                        Kirigami.AbstractCard {
                            id: noteCard

                            required property var modelData

                            Layout.fillWidth: true

                            contentItem: ColumnLayout {
                                spacing: Kirigami.Units.smallSpacing

                                // Where a sticky note is; a click goes there.
                                Kirigami.LinkButton {
                                    Layout.fillWidth: true
                                    visible: noteCard.modelData.page >= 0
                                    horizontalAlignment: Text.AlignLeft
                                    elide: Text.ElideRight
                                    text: (root.info.kind ?? 0) === 4 ? i18nc("@action:button where a sticky note is", "On the picture")
                                                                      : i18nc("@action:button where a sticky note is", "On page %1", noteCard.modelData.page + 1)
                                    onClicked: root.noteActivated(noteCard.modelData)
                                }
                                QQC2.Label {
                                    Layout.fillWidth: true
                                    text: noteCard.modelData.text
                                    textFormat: Text.PlainText
                                    wrapMode: Text.Wrap
                                }
                                RowLayout {
                                    Layout.fillWidth: true

                                    QQC2.Label {
                                        Layout.fillWidth: true
                                        text: noteCard.modelData.modifiedText
                                        font: Kirigami.Theme.smallFont
                                        color: Kirigami.Theme.disabledTextColor
                                        elide: Text.ElideRight
                                    }
                                    QQC2.ToolButton {
                                        icon.name: "edit-delete"
                                        text: i18nc("@action:button", "Delete Note")
                                        display: QQC2.AbstractButton.IconOnly
                                        onClicked: App.library.removeNote(noteCard.modelData.id)
                                        QQC2.ToolTip.text: text
                                        QQC2.ToolTip.visible: hovered
                                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                                    }
                                }
                            }
                        }
                    }
                    QQC2.Button {
                        action: root.actions.note
                    }

                    // What the file says about itself
                    SectionTitle {
                        visible: (root.info.metadata ?? []).length > 0
                        text: i18nc("@title:group", "Details")
                    }
                    Repeater {
                        model: root.info.metadata ?? []

                        Fact {
                            required property var modelData

                            label: i18nc("@label a metadata field name followed by a colon", "%1:", modelData.label)
                            value: modelData.value
                        }
                    }
                }
            }
        }
    }
}
