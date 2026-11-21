// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// What the toolbar shows between the panel buttons and the search field, and in which order.
// Changes take effect at once.
FormCard.FormCardPage {
    id: root

    // Names nothing is known under any more, such as actions of an older version, are dropped.
    readonly property list<string> names: Config.toolbarActions.filter(name => App.toolbarItem(name).name !== undefined)
    readonly property var candidates: App.toolbarCandidates()

    title: i18nc("@title:window", "Toolbar")

    function apply(list: var): void {
        Config.toolbarActions = list;
        Config.save();
    }

    function move(index: int, delta: int): void {
        const target = index + delta;
        if (target < 0 || target >= names.length) {
            return;
        }
        const list = Array.from(names);
        const moved = list.splice(index, 1)[0];
        list.splice(target, 0, moved);
        apply(list);
    }

    function remove(index: int): void {
        const list = Array.from(names);
        list.splice(index, 1);
        apply(list);
    }

    function add(name: string): void {
        apply(Array.from(names).concat([name]));
    }

    FormCard.FormHeader {
        title: i18nc("@title:group", "Buttons, from left to right")
    }
    FormCard.FormCard {
        FormCard.FormPlaceholderMessageDelegate {
            visible: root.names.length === 0
            icon.name: "configure-toolbars"
            text: i18nc("@info:placeholder", "The toolbar has no buttons")
            explanation: i18nc("@info:placeholder", "Every function is still in the menu and in the command bar.")
        }
        Repeater {
            model: root.names

            FormCard.AbstractFormDelegate {
                id: entry

                required property int index
                required property string modelData
                readonly property var item: App.toolbarItem(modelData)
                readonly property bool separator: modelData === "separator"

                background: null
                Accessible.name: item.text ?? ""

                contentItem: RowLayout {
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Icon {
                        source: entry.item.icon ?? ""
                        opacity: entry.separator ? 0.5 : 1
                        implicitWidth: Kirigami.Units.iconSizes.smallMedium
                        implicitHeight: implicitWidth
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: entry.item.text ?? ""
                        elide: Text.ElideRight
                        color: entry.separator ? Kirigami.Theme.disabledTextColor : Kirigami.Theme.textColor
                    }
                    QQC2.ToolButton {
                        icon.name: "go-up"
                        text: i18nc("@action:button move a toolbar button towards the start", "Move Up")
                        display: QQC2.AbstractButton.IconOnly
                        enabled: entry.index > 0
                        onClicked: root.move(entry.index, -1)
                        QQC2.ToolTip.text: text
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    }
                    QQC2.ToolButton {
                        icon.name: "go-down"
                        text: i18nc("@action:button move a toolbar button towards the end", "Move Down")
                        display: QQC2.AbstractButton.IconOnly
                        enabled: entry.index < root.names.length - 1
                        onClicked: root.move(entry.index, 1)
                        QQC2.ToolTip.text: text
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    }
                    QQC2.ToolButton {
                        icon.name: "list-remove"
                        text: i18nc("@action:button", "Remove from Toolbar")
                        display: QQC2.AbstractButton.IconOnly
                        onClicked: root.remove(entry.index)
                        QQC2.ToolTip.text: text
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    }
                }
            }
        }
    }

    FormCard.FormCard {
        Layout.topMargin: Kirigami.Units.largeSpacing

        FormCard.FormButtonDelegate {
            text: i18nc("@action:button", "Add Button…")
            icon.name: "list-add"
            onClicked: addDialog.open()
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormButtonDelegate {
            text: i18nc("@action:button", "Add Separator")
            icon.name: "distribute-horizontal-x"
            onClicked: root.add("separator")
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormButtonDelegate {
            text: i18nc("@action:button", "Restore Defaults")
            icon.name: "edit-reset"
            enabled: root.names.join(",") !== App.defaultToolbar().join(",")
            onClicked: root.apply(App.defaultToolbar())
        }
    }

    FormCard.FormHeader {
        title: i18nc("@title:group", "Appearance")
    }
    FormCard.FormCard {
        FormCard.FormSwitchDelegate {
            text: i18nc("@option:check", "Show text beside the icons")
            description: i18nc("@info", "Without text more buttons fit; their names appear when the pointer rests on them.")
            checked: !Config.toolbarIconsOnly
            onToggled: {
                Config.toolbarIconsOnly = !checked;
                Config.save();
            }
        }
    }

    FormCard.FormSectionText {
        text: i18nc("@info", "The buttons for the panels, the search field and the menu keep their places. Buttons that do not fit the window move into a menu at the end of the toolbar.")
    }

    Kirigami.Dialog {
        id: addDialog

        // What is not in the toolbar yet and matches what was typed.
        readonly property var offered: root.candidates.filter(item => !root.names.includes(item.name)
            && item.text.toLowerCase().includes(filterField.text.trim().toLowerCase()))

        // In the settings window, not in the main window where dialogs go by default.
        parent: root.QQC2.Overlay.overlay
        title: i18nc("@title:window", "Add Button to Toolbar")
        preferredWidth: Kirigami.Units.gridUnit * 24
        preferredHeight: Kirigami.Units.gridUnit * 26
        standardButtons: Kirigami.Dialog.Close
        onOpened: {
            filterField.text = "";
            filterField.forceActiveFocus();
        }

        header: ColumnLayout {
            spacing: 0

            Kirigami.Heading {
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.largeSpacing
                level: 2
                text: addDialog.title
                elide: Text.ElideRight
            }
            Kirigami.SearchField {
                id: filterField

                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.largeSpacing
                Layout.rightMargin: Kirigami.Units.largeSpacing
                Layout.bottomMargin: Kirigami.Units.largeSpacing
            }
            Kirigami.Separator {
                Layout.fillWidth: true
            }
        }

        ListView {
            id: offeredList

            implicitHeight: Kirigami.Units.gridUnit * 18
            model: addDialog.offered
            clip: true

            delegate: QQC2.ItemDelegate {
                required property var modelData

                width: offeredList.width
                text: modelData.text
                icon.name: modelData.icon
                onClicked: root.add(modelData.name)
            }

            Kirigami.PlaceholderMessage {
                anchors.centerIn: parent
                width: parent.width - Kirigami.Units.gridUnit * 2
                visible: offeredList.count === 0
                icon.name: "edit-none"
                text: filterField.text.length > 0 ? i18nc("@info:placeholder", "Nothing matches") : i18nc("@info:placeholder", "Everything is in the toolbar already")
            }
        }
    }
}
