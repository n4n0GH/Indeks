// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Shows an exported e-mail: its headers, text and attachments. Nothing is loaded from the
// network; pictures and other remote content of HTML mails are left out.
ColumnLayout {
    id: root

    required property real documentId
    property var mail: ({})
    property bool showHtml: false
    /** How large the text of the message is shown; see TextPreview. */
    property real textZoom: 1

    signal zoomRequested(real zoom)

    // For the bar above the preview; see PreviewToolBar.
    readonly property string status: ""
    readonly property bool zoomable: mail.valid ?? false
    readonly property real zoomFactor: textZoom
    readonly property bool zoomIsDefault: textZoom === 1
    readonly property string zoomResetText: i18nc("@info:tooltip", "Back to the normal size")
    function zoomStep(larger: bool): void {
        const next = Math.round(textZoom * (larger ? 1.1 : 1 / 1.1) * 100) / 100;
        zoomRequested(Math.abs(next - 1) < 0.04 ? 1 : Math.max(0.5, Math.min(4, next)));
    }
    function zoomIn(): void {
        zoomStep(true);
    }
    function zoomOut(): void {
        zoomStep(false);
    }
    function zoomReset(): void {
        zoomRequested(1);
    }
    // A message that comes as plain and as formatted text is shown plain at first; the
    // formatted one is the other way of looking at it.
    readonly property list<Kirigami.Action> viewActions: [
        Kirigami.Action {
            visible: root.hasText && root.hasHtml
            text: i18nc("@action:intoolbar show the formatted version of an e-mail", "Show Formatted")
            icon.name: "format-text-bold"
            tooltip: i18nc("@info:tooltip", "This message has a plain and a formatted version")
            checkable: true
            checked: root.showHtml
            onTriggered: {
                root.showHtml = !root.showHtml;
                checked = Qt.binding(() => root.showHtml);
            }
        }
    ]

    readonly property bool hasText: (mail.text ?? "").length > 0
    readonly property bool hasHtml: (mail.html ?? "").length > 0

    spacing: 0

    Component.onCompleted: {
        mail = App.library.email(documentId, 0);
        showHtml = !hasText && hasHtml;
    }

    // One line of the head of a message. Who a message went to can be a great many people:
    // such a line is one line long at first, with a button that opens it up, so that the
    // message itself is not pushed out of sight.
    component HeaderLine: RowLayout {
        id: line

        property alias label: labelItem.text
        property string value
        /** Whether the line is held to one row until it is opened. */
        property bool collapsible: false
        property bool expanded: false
        readonly property bool collapsed: collapsible && !expanded
        // Addresses, counted by what every one of them has.
        readonly property int count: (value.match(/@/g) ?? []).length

        Layout.fillWidth: true
        visible: value.length > 0
        spacing: Kirigami.Units.smallSpacing

        QQC2.Label {
            id: labelItem

            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: Kirigami.Units.gridUnit * 4
            horizontalAlignment: Text.AlignRight
            color: Kirigami.Theme.disabledTextColor
        }
        QQC2.Label {
            id: shortValue

            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            visible: line.collapsed
            text: line.value
            textFormat: Text.PlainText
            elide: Text.ElideRight
            maximumLineCount: 1
            wrapMode: Text.NoWrap
        }
        Kirigami.SelectableLabel {
            Layout.fillWidth: true
            visible: !line.collapsed
            text: line.value
            textFormat: Text.PlainText
        }
        QQC2.ToolButton {
            Layout.alignment: Qt.AlignTop
            // Only where there is more than the one row shows.
            visible: line.collapsible && (line.expanded || shortValue.truncated)
            // No taller than the row it stands in.
            implicitHeight: labelItem.implicitHeight
            topPadding: 0
            bottomPadding: 0
            text: line.expanded ? i18nc("@action:button show one row of the recipients again", "Show Fewer")
                                : line.count > 1 ? i18nc("@action:button show all recipients of a message; %1 is how many there are", "Show All %1", line.count)
                                                 : i18nc("@action:button show the whole of a line that is cut short", "Show All")
            icon.name: line.expanded ? "go-up" : "go-down"
            font: Kirigami.Theme.smallFont
            onClicked: line.expanded = !line.expanded

            QQC2.ToolTip.text: line.expanded ? i18nc("@info:tooltip", "Show only the first of them")
                                             : i18ncp("@info:tooltip", "Show the address", "Show all %1 addresses", line.count)
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
        }
    }

    // Head and text scroll as one: however long the head is, the text is reached.
    QQC2.ScrollView {
        id: scroll

        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: root.mail.valid ?? false
        contentWidth: availableWidth
        QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

        ColumnLayout {
            width: scroll.availableWidth
            spacing: 0

            QQC2.Pane {
                Layout.fillWidth: true

                contentItem: ColumnLayout {
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Heading {
                        Layout.fillWidth: true
                        level: 3
                        text: (root.mail.subject ?? "").length > 0 ? root.mail.subject : i18nc("@info e-mail without subject", "(No subject)")
                        wrapMode: Text.Wrap
                        maximumLineCount: 3
                        elide: Text.ElideRight
                    }
                    HeaderLine {
                        label: i18nc("@label e-mail header", "From:")
                        value: root.mail.from ?? ""
                    }
                    HeaderLine {
                        label: i18nc("@label e-mail header", "To:")
                        value: root.mail.to ?? ""
                        collapsible: true
                    }
                    HeaderLine {
                        label: i18nc("@label e-mail header", "Cc:")
                        value: root.mail.cc ?? ""
                        collapsible: true
                    }
                    HeaderLine {
                        label: i18nc("@label e-mail header", "Bcc:")
                        value: root.mail.bcc ?? ""
                        collapsible: true
                    }
                    HeaderLine {
                        label: i18nc("@label e-mail header", "Sent:")
                        value: root.mail.date ?? ""
                    }

                    Flow {
                        Layout.fillWidth: true
                        visible: (root.mail.attachments ?? []).length > 0
                        spacing: Kirigami.Units.smallSpacing

                        Repeater {
                            model: root.mail.attachments ?? []

                            QQC2.Button {
                                id: attachment

                                required property var modelData

                                text: i18nc("@action:button attachment name and size", "%1 (%2)", modelData.name, modelData.sizeText)
                                icon.name: modelData.icon
                                onClicked: attachmentMenu.popup(attachment, 0, attachment.height)

                                QQC2.Menu {
                                    id: attachmentMenu

                                    QQC2.MenuItem {
                                        text: i18nc("@action:inmenu", "Open")
                                        icon.name: "document-open"
                                        onTriggered: {
                                            const url = App.library.extractAttachment(root.documentId, 0, attachment.modelData.index);
                                            if (url.toString().length > 0) {
                                                Qt.openUrlExternally(url);
                                            }
                                        }
                                    }
                                    QQC2.MenuItem {
                                        text: i18nc("@action:inmenu", "Add to Indeks")
                                        icon.name: "list-add"
                                        onTriggered: App.library.importAttachment(root.documentId, 0, attachment.modelData.index)
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Kirigami.Separator {
                Layout.fillWidth: true
            }

            QQC2.TextArea {
                Layout.fillWidth: true
                // At least what is left of the view, so that a click below a short text lands in it.
                Layout.minimumHeight: Math.max(0, scroll.availableHeight - y)
                readOnly: true
                wrapMode: TextEdit.Wrap
                selectByMouse: true
                textFormat: root.showHtml ? TextEdit.RichText : TextEdit.PlainText
                text: root.showHtml ? (root.mail.html ?? "") : (root.mail.text ?? "")
                font.family: Kirigami.Theme.defaultFont.family
                font.pointSize: Kirigami.Theme.defaultFont.pointSize * root.textZoom
                background: Rectangle {
                    color: Kirigami.Theme.backgroundColor
                }
                Kirigami.Theme.colorSet: Kirigami.Theme.View
                Kirigami.Theme.inherit: false
                onLinkActivated: link => linkDialog.ask(link)
            }
        }

        WheelHandler {
            acceptedModifiers: Qt.ControlModifier
            onWheel: event => {
                if (event.angleDelta.y !== 0) {
                    root.zoomStep(event.angleDelta.y > 0);
                }
            }
        }
    }

    Kirigami.PlaceholderMessage {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.margins: Kirigami.Units.gridUnit
        visible: !(root.mail.valid ?? false)
        icon.name: "mail-message"
        text: i18nc("@info:placeholder", "This file does not contain a readable e-mail")
    }

    // Links in mails are a favourite of phishers: show where one really leads before opening it.
    Kirigami.PromptDialog {
        id: linkDialog

        property string link

        function ask(target: string): void {
            link = target;
            open();
        }

        title: i18nc("@title:window", "Open Link?")
        subtitle: i18nc("@info", "The link in this message leads to:\n%1", link)
        standardButtons: Kirigami.Dialog.Cancel
        customFooterActions: [
            Kirigami.Action {
                text: i18nc("@action:button", "Open Link")
                icon.name: "internet-services"
                onTriggered: {
                    Qt.openUrlExternally(linkDialog.link);
                    linkDialog.close();
                }
            }
        ]
    }
}
