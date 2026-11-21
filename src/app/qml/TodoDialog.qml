// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.dateandtime as DateAndTime

// Writes down something that is to be done, or changes what was written: a line of text, the
// day it is due if it has one, and the document it is about if it is about one.
Kirigami.Dialog {
    id: root

    /** The to-do that is changed; 0 for a new one. */
    property real todoId: 0
    property real documentId: 0
    property string documentName
    property string documentIcon
    /** The day it is due as 2026-11-30, or empty. */
    property string due

    readonly property bool canSubmit: textField.text.trim().length > 0
    /** The one document that is selected in the list, which a to-do may be linked to. */
    readonly property real selectedId: App.documents.selectionCount === 1 ? App.documents.currentId : 0

    /** A new to-do was written down. */
    signal added()

    function link(id: real): void {
        const info = id > 0 ? App.library.documentInfo(id) : {};
        documentId = info.id ?? 0;
        documentName = info.name ?? "";
        documentIcon = info.icon ?? "";
    }

    /** A new to-do, about the document @p documentId if that is one. */
    function ask(documentId: real): void {
        todoId = 0;
        link(documentId);
        due = "";
        textField.text = "";
        open();
        textField.forceActiveFocus();
    }

    function edit(id: real): void {
        const todo = App.library.todo(id);
        if (todo.id === undefined) {
            return;
        }
        todoId = todo.id;
        link(todo.documentId ?? 0);
        due = todo.due ?? "";
        textField.text = todo.text;
        open();
        textField.forceActiveFocus();
        textField.selectAll();
    }

    function submit(): void {
        if (!canSubmit) {
            return;
        }
        if (todoId > 0) {
            App.library.updateTodo(todoId, textField.text, documentId, due);
        } else if (App.library.addTodo(textField.text, documentId, due) > 0) {
            added();
        }
        close();
    }

    // Built from its parts: a day without a time is the same day wherever the clock stands.
    function dayOf(iso: string): date {
        const parts = iso.split("-").map(Number);
        return parts.length === 3 ? new Date(parts[0], parts[1] - 1, parts[2]) : new Date(NaN);
    }
    function isoOf(day: date): string {
        return isNaN(day.getTime()) ? "" : Qt.formatDate(day, "yyyy-MM-dd");
    }
    function inDays(days: int): string {
        const day = new Date();
        day.setDate(day.getDate() + days);
        return isoOf(day);
    }

    title: todoId > 0 ? i18nc("@title:window", "Change To-Do") : i18nc("@title:window", "New To-Do")
    padding: Kirigami.Units.largeSpacing
    preferredWidth: Kirigami.Units.gridUnit * 26
    standardButtons: Kirigami.Dialog.NoButton
    customFooterActions: [
        Kirigami.Action {
            text: root.todoId > 0 ? i18nc("@action:button", "Save") : i18nc("@action:button", "Add To-Do")
            icon.name: root.todoId > 0 ? "document-save" : "list-add"
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
            id: textField

            Kirigami.FormData.label: i18nc("@label:textbox", "To do:")
            Layout.fillWidth: true
            // A form gives a field no more room than it asks for.
            implicitWidth: Kirigami.Units.gridUnit * 18
            placeholderText: i18nc("@info:placeholder", "What is to be done?")
            onAccepted: root.submit()
        }
        RowLayout {
            Kirigami.FormData.label: i18nc("@label the day a to-do is due", "Due:")
            spacing: Kirigami.Units.smallSpacing

            QQC2.Button {
                id: dueButton

                icon.name: "view-calendar"
                text: root.due.length > 0 ? root.dayOf(root.due).toLocaleDateString(Qt.locale(), Locale.LongFormat) : i18nc("@action:button no day is set for the to-do", "No Deadline")
                onClicked: {
                    datePopup.value = root.due.length > 0 ? root.dayOf(root.due) : new Date();
                    datePopup.open();
                }
                QQC2.ToolTip.text: i18nc("@info:tooltip", "Choose the day this is due")
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            }
            QQC2.ToolButton {
                icon.name: "arrow-down"
                text: i18nc("@action:button", "Quick Choices")
                display: QQC2.AbstractButton.IconOnly
                onClicked: quickMenu.popup(this, 0, height)
                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

                QQC2.Menu {
                    id: quickMenu

                    QQC2.MenuItem {
                        text: i18nc("@action:inmenu day a to-do is due", "Today")
                        onTriggered: root.due = root.inDays(0)
                    }
                    QQC2.MenuItem {
                        text: i18nc("@action:inmenu day a to-do is due", "Tomorrow")
                        onTriggered: root.due = root.inDays(1)
                    }
                    QQC2.MenuItem {
                        text: i18nc("@action:inmenu day a to-do is due", "In a Week")
                        onTriggered: root.due = root.inDays(7)
                    }
                    QQC2.MenuItem {
                        text: i18nc("@action:inmenu day a to-do is due", "In a Month")
                        onTriggered: root.due = root.inDays(30)
                    }
                }
            }
            QQC2.ToolButton {
                visible: root.due.length > 0
                icon.name: "edit-clear"
                text: i18nc("@action:button", "No Deadline")
                display: QQC2.AbstractButton.IconOnly
                onClicked: root.due = ""
                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            }
        }
        RowLayout {
            Kirigami.FormData.label: i18nc("@label the document a to-do is about", "About:")
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                visible: root.documentId > 0
                source: root.documentIcon.length > 0 ? root.documentIcon : "text-x-generic"
                implicitWidth: Kirigami.Units.iconSizes.small
                implicitHeight: implicitWidth
            }
            QQC2.Label {
                Layout.fillWidth: true
                Layout.maximumWidth: Kirigami.Units.gridUnit * 14
                text: root.documentId > 0 ? root.documentName : i18nc("@info the to-do is not about a document", "No document")
                textFormat: Text.PlainText
                elide: Text.ElideMiddle
                color: root.documentId > 0 ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
            }
            QQC2.ToolButton {
                visible: root.documentId > 0
                icon.name: "remove-link"
                text: i18nc("@action:button", "Do Not Link a Document")
                display: QQC2.AbstractButton.IconOnly
                onClicked: root.link(0)
                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            }
            QQC2.ToolButton {
                // The document that is selected in the list, where it is not the one already.
                visible: root.selectedId > 0 && root.selectedId !== root.documentId
                icon.name: "link"
                text: i18nc("@action:button", "Link Selected Document")
                display: QQC2.AbstractButton.IconOnly
                onClicked: root.link(root.selectedId)
                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            }
        }
    }

    DateAndTime.DatePopup {
        id: datePopup

        parent: QQC2.Overlay.overlay
        anchors.centerIn: parent
        modal: true
        autoAccept: true
        onAccepted: {
            root.due = root.isoOf(value);
            close();
        }
    }
}
