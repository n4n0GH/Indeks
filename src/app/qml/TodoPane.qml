// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The list of things to do: a line of text each, perhaps with a day it is due and a document
// it is about. What is due within days stands out in orange, what is past its day in red.
// Clicking a line that is about a document shows that document.
QQC2.Pane {
    id: root

    required property Actions actions

    property var todos: []
    property string shown: ""

    signal editRequested(real id)

    function reload(): void {
        const list = App.library.todos();
        // The index changes all the time while files are read; the list is only built anew
        // when it differs, so that it stays where it was scrolled to.
        const text = JSON.stringify(list);
        if (text !== shown) {
            shown = text;
            todos = list;
        }
    }

    padding: 0
    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    Component.onCompleted: reload()

    Connections {
        target: App.library
        function onTodosChanged(): void {
            root.reload();
        }
        // A document a to-do is about was renamed, or went.
        function onChanged(): void {
            root.reload();
        }
    }
    // A new day makes other things due.
    Timer {
        interval: 10 * 60 * 1000
        repeat: true
        running: true
        onTriggered: root.reload()
    }

    contentItem: ColumnLayout {
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.smallSpacing
            spacing: Kirigami.Units.smallSpacing

            QQC2.ToolButton {
                action: root.actions.newTodo
                display: QQC2.AbstractButton.TextBesideIcon
            }
            Item {
                Layout.fillWidth: true
            }
            QQC2.ToolButton {
                action: root.actions.clearDoneTodos
                display: QQC2.AbstractButton.IconOnly
                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            }
        }
        Kirigami.Separator {
            Layout.fillWidth: true
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Kirigami.PlaceholderMessage {
                anchors.centerIn: parent
                width: parent.width - Kirigami.Units.gridUnit * 2
                visible: root.todos.length === 0
                icon.name: "view-task"
                text: i18nc("@info:placeholder", "Nothing to do")
                explanation: i18nc("@info:placeholder", "Write down what is to be done, with a day it is due if it has one. A to-do made from a document's context menu leads back to that document.")
            }

            QQC2.ScrollView {
                id: scroll

                anchors.fill: parent
                visible: root.todos.length > 0
                QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

                ListView {
                    id: list

                    model: root.todos
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    spacing: 1

                    delegate: QQC2.ItemDelegate {
                        id: row

                        required property var modelData

                        readonly property bool linked: (modelData.documentId ?? 0) > 0
                        readonly property bool overdue: modelData.state === "overdue"
                        readonly property bool soon: modelData.state === "soon"
                        // The colours the desktop has for a warning and for what went wrong.
                        readonly property color mark: overdue ? Kirigami.Theme.negativeTextColor : soon ? Kirigami.Theme.neutralTextColor : "transparent"

                        width: list.width
                        leftPadding: Kirigami.Units.largeSpacing + stripe.width
                        rightPadding: Kirigami.Units.smallSpacing
                        topPadding: Kirigami.Units.smallSpacing
                        bottomPadding: Kirigami.Units.smallSpacing
                        hoverEnabled: true

                        Accessible.name: modelData.text

                        // Clicking leads to the document it is about; one without is changed.
                        onClicked: {
                            if (linked) {
                                App.showDocument(modelData.documentId);
                            }
                        }
                        onDoubleClicked: {
                            if (!linked) {
                                root.editRequested(modelData.id);
                            }
                        }

                        QQC2.ToolTip.text: !linked ? i18nc("@info:tooltip", "Double-click to change")
                            : modelData.documentTrashed ? i18nc("@info:tooltip %1 is a file name", "Show “%1”, which is in the trash", modelData.documentName)
                            : i18nc("@info:tooltip %1 is a file name", "Show “%1”", modelData.documentName)
                        QQC2.ToolTip.visible: hovered && !menu.visible
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

                        background: Rectangle {
                            color: row.overdue ? Kirigami.Theme.negativeBackgroundColor
                                : row.soon ? Kirigami.Theme.neutralBackgroundColor
                                : row.hovered ? Kirigami.Theme.alternateBackgroundColor : "transparent"
                            border.width: row.visualFocus ? 1 : 0
                            border.color: Kirigami.Theme.focusColor

                            Rectangle {
                                id: stripe

                                width: Kirigami.Units.smallSpacing
                                height: parent.height
                                color: row.mark
                            }
                            Rectangle {
                                anchors.fill: parent
                                visible: row.hovered && (row.overdue || row.soon)
                                color: Kirigami.Theme.textColor
                                opacity: 0.06
                            }
                        }

                        contentItem: RowLayout {
                            spacing: Kirigami.Units.smallSpacing

                            QQC2.CheckBox {
                                // In the middle of the line, whether or not a day stands below the text.
                                Layout.alignment: Qt.AlignVCenter
                                checked: row.modelData.done
                                onToggled: App.library.setTodoDone(row.modelData.id, checked)

                                Accessible.name: i18nc("@option:check", "Done")
                                QQC2.ToolTip.text: checked ? i18nc("@info:tooltip", "Done. Click to open it again.") : i18nc("@info:tooltip", "Mark as done")
                                QQC2.ToolTip.visible: hovered
                                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Kirigami.Units.smallSpacing

                                    // A to-do that is about a document says so with its icon.
                                    Kirigami.Icon {
                                        Layout.alignment: Qt.AlignTop
                                        visible: row.linked
                                        source: row.modelData.documentIcon ?? "text-x-generic"
                                        implicitWidth: Kirigami.Units.iconSizes.small
                                        implicitHeight: implicitWidth
                                        opacity: row.modelData.done || row.modelData.documentTrashed ? 0.5 : 1
                                    }
                                    QQC2.Label {
                                        Layout.fillWidth: true
                                        text: row.modelData.text
                                        textFormat: Text.PlainText
                                        wrapMode: Text.Wrap
                                        maximumLineCount: 4
                                        elide: Text.ElideRight
                                        font.strikeout: row.modelData.done
                                        color: row.modelData.done ? Kirigami.Theme.disabledTextColor : Kirigami.Theme.textColor
                                    }
                                }
                                QQC2.Label {
                                    Layout.fillWidth: true
                                    visible: text.length > 0
                                    text: {
                                        const due = row.modelData.dueText ?? "";
                                        if (due.length === 0) {
                                            return "";
                                        }
                                        return row.overdue ? i18nc("@info a to-do that is past its day; %1 is that day", "Was due %1", due)
                                            : i18nc("@info a to-do; %1 is the day it is due", "Due %1", due);
                                    }
                                    textFormat: Text.PlainText
                                    elide: Text.ElideRight
                                    font: Kirigami.Theme.smallFont
                                    color: row.modelData.done ? Kirigami.Theme.disabledTextColor
                                        : row.overdue ? Kirigami.Theme.negativeTextColor
                                        : row.soon ? Kirigami.Theme.neutralTextColor : Kirigami.Theme.disabledTextColor
                                }
                            }
                            QQC2.ToolButton {
                                Layout.alignment: Qt.AlignVCenter
                                // Keeps its room, so that the text does not jump on hover.
                                opacity: row.hovered || visualFocus ? 1 : 0
                                icon.name: "edit-entry"
                                text: i18nc("@action:button", "Change…")
                                display: QQC2.AbstractButton.IconOnly
                                onClicked: root.editRequested(row.modelData.id)
                                QQC2.ToolTip.text: text
                                QQC2.ToolTip.visible: hovered
                                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                            }
                        }

                        TapHandler {
                            acceptedButtons: Qt.RightButton
                            onTapped: menu.popup()
                        }

                        QQC2.Menu {
                            id: menu

                            QQC2.MenuItem {
                                text: i18nc("@action:inmenu", "Show Document")
                                icon.name: "document-preview"
                                visible: row.linked
                                height: visible ? implicitHeight : 0
                                onTriggered: App.showDocument(row.modelData.documentId)
                            }
                            QQC2.MenuItem {
                                text: row.modelData.done ? i18nc("@action:inmenu", "Open Again") : i18nc("@action:inmenu", "Mark as Done")
                                icon.name: row.modelData.done ? "edit-undo" : "task-complete"
                                onTriggered: App.library.setTodoDone(row.modelData.id, !row.modelData.done)
                            }
                            QQC2.MenuItem {
                                text: i18nc("@action:inmenu", "Change…")
                                icon.name: "edit-entry"
                                onTriggered: root.editRequested(row.modelData.id)
                            }
                            QQC2.MenuSeparator {}
                            QQC2.MenuItem {
                                text: i18nc("@action:inmenu", "Delete")
                                icon.name: "edit-delete"
                                onTriggered: App.library.removeTodo(row.modelData.id)
                            }
                        }
                    }
                }
            }
        }
    }
}
