// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

FormCard.FormCardPage {
    id: root

    property var sources: App.library.sources()

    title: i18nc("@title:window", "Indexed Locations")

    Connections {
        target: App.library
        function onSourcesChanged(): void {
            root.sources = App.library.sources();
        }
        function onChanged(): void {
            root.sources = App.library.sources();
        }
    }

    FormCard.FormHeader {
        title: i18nc("@title:group", "Folders and servers")
    }
    FormCard.FormCard {
        FormCard.FormPlaceholderMessageDelegate {
            visible: root.sources.length === 0
            icon.name: "folder"
            text: i18nc("@info:placeholder", "No locations are indexed")
            explanation: i18nc("@info:placeholder", "Indeks finds the files of an indexed location without moving or changing them.")
        }
        Repeater {
            model: root.sources

            FormCard.AbstractFormDelegate {
                id: entry

                required property var modelData

                background: null
                contentItem: RowLayout {
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Icon {
                        source: entry.modelData.local ? "folder" : "folder-remote"
                        opacity: entry.modelData.available ? 1 : 0.5
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
                        }
                        QQC2.Label {
                            Layout.fillWidth: true
                            text: entry.modelData.display
                            elide: Text.ElideMiddle
                            font: Kirigami.Theme.smallFont
                            color: Kirigami.Theme.disabledTextColor
                        }
                        QQC2.Label {
                            Layout.fillWidth: true
                            text: !entry.modelData.available ? i18nc("@info", "Not connected — its documents stay searchable")
                                : entry.modelData.lastScan.length === 0 ? i18nc("@info", "Not checked yet")
                                : entry.modelData.inbox ? i18ncp("@info", "%1 document, last checked %2 — new files go to the inbox", "%1 documents, last checked %2 — new files go to the inbox", entry.modelData.count, entry.modelData.lastScan)
                                : i18ncp("@info", "%1 document, last checked %2", "%1 documents, last checked %2", entry.modelData.count, entry.modelData.lastScan)
                            wrapMode: Text.Wrap
                            font: Kirigami.Theme.smallFont
                            color: entry.modelData.available ? Kirigami.Theme.disabledTextColor : Kirigami.Theme.neutralTextColor
                        }
                    }
                    QQC2.ToolButton {
                        icon.name: "mail-folder-inbox"
                        text: i18nc("@action:button", "Put New Files into the Inbox")
                        display: QQC2.AbstractButton.IconOnly
                        checkable: true
                        checked: entry.modelData.inbox
                        onToggled: App.library.setSourceUsesInbox(entry.modelData.id, checked)
                        QQC2.ToolTip.text: checked ? i18nc("@info:tooltip", "Files that turn up here are put into the inbox. Click to have them indexed without that.")
                            : i18nc("@info:tooltip", "Files that turn up here are indexed without going to the inbox. Click to have them put there.")
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    }
                    QQC2.ToolButton {
                        icon.name: "view-refresh"
                        text: i18nc("@action:button", "Update Now")
                        display: QQC2.AbstractButton.IconOnly
                        onClicked: App.library.rescanSource(entry.modelData.id)
                        QQC2.ToolTip.text: text
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    }
                    QQC2.ToolButton {
                        icon.name: "list-remove"
                        text: i18nc("@action:button", "Stop Indexing")
                        display: QQC2.AbstractButton.IconOnly
                        onClicked: {
                            App.goTo("source:" + entry.modelData.id);
                            App.action("delete_sidebar_item").trigger();
                        }
                        QQC2.ToolTip.text: text
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    }
                }
            }
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormButtonDelegate {
            text: i18nc("@action:button", "Add Folder…")
            icon.name: "folder-add"
            onClicked: App.action("add_folder").trigger()
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormButtonDelegate {
            text: i18nc("@action:button", "Add Remote Location…")
            icon.name: "folder-remote"
            onClicked: App.action("add_remote").trigger()
        }
    }

    FormCard.FormSectionText {
        text: i18nc("@info", "What is in a location when it is added goes straight into the index. Files that turn up later, a scan that was just saved for example, are put into the inbox as well, where they wait to be filed. The inbox button of a location switches that off for it.")
    }
    FormCard.FormSectionText {
        text: i18nc("@info", "Folders on external drives are indexed like any other. When the drive is unplugged its documents remain in the index, and the index catches up when the drive returns.")
    }
}
