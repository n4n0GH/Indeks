// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The notes that are stuck to a page of a PDF or to a picture, like sticky notes: a small
// yellow square where each was put, which says what is written on it when the pointer rests
// on it, opens for reading and writing with a click, and can be dragged elsewhere.
//
// This lies over the page it belongs to and is as large. Where a note is, is kept as shares
// of the page's width and height, so it stays put at any zoom. Notes live in the index like
// drawings do; the file is not touched.
Item {
    id: root

    required property real documentId
    /** The page this lies on, from 0; a picture is page 0. */
    property int page: 0
    /** All notes of the document (see Library::notes()); those of this page are shown. */
    property var notes: []
    /** The next click on the page puts a new note there. */
    property bool placing: false
    /** Whether notes can be worked with right now; not while the page is being drawn on. */
    property bool active: true

    /** A note was put down, or putting one down was given up. */
    signal placed()

    readonly property var mine: notes.filter(note => note.page === page && note.x >= 0)
    // The colours of a sticky note are part of the note, not of the interface: it is yellow
    // on a white page under every colour scheme. The same reasoning as for ink.
    readonly property color paper: "#fff3a0"
    readonly property color edge: "#b8960a"
    readonly property color writing: "#232629"
    readonly property real size: Kirigami.Units.iconSizes.smallMedium + Kirigami.Units.smallSpacing

    // Opens a note for reading and writing; one with an id of 0 is new and is put at the
    // place given once something is written on it.
    function edit(id: real, text: string, modified: string, x: real, y: real): void {
        editor.noteId = id;
        editor.at = Qt.point(x, y);
        editor.original = text;
        editor.modified = modified;
        area.text = text;
        // Beside the note, and inside the page where there is room for that.
        editor.x = Math.max(0, Math.min(root.width - editor.width, x * root.width + root.size / 2));
        editor.y = Math.max(0, Math.min(root.height - editor.height, y * root.height - root.size / 2));
        editor.open();
        area.forceActiveFocus();
    }

    function addAt(position: point): void {
        edit(0, "", "", Math.max(0, Math.min(1, position.x / width)), Math.max(0, Math.min(1, position.y / height)));
    }

    HoverHandler {
        enabled: root.placing && root.active
        cursorShape: Qt.CrossCursor
    }
    TapHandler {
        enabled: root.placing && root.active
        onTapped: eventPoint => {
            root.addAt(eventPoint.position);
            root.placed();
        }
    }
    TapHandler {
        id: menuTap

        property point at

        enabled: root.active
        acceptedButtons: Qt.RightButton
        onTapped: eventPoint => {
            at = eventPoint.position;
            pageMenu.popup();
        }
    }

    QQC2.Menu {
        id: pageMenu

        QQC2.MenuItem {
            text: i18nc("@action:inmenu", "Add Note Here")
            icon.name: "note"
            onTriggered: root.addAt(menuTap.at)
        }
    }

    Repeater {
        model: root.mine

        delegate: Rectangle {
            id: sticky

            required property var modelData

            x: Math.round(modelData.x * root.width - width / 2)
            y: Math.round(modelData.y * root.height - height / 2)
            width: root.size
            height: root.size
            radius: 2
            color: root.paper
            border.width: 1
            border.color: root.edge
            enabled: root.active
            opacity: root.active ? 1 : 0.6
            scale: stickyHover.hovered || stickyDrag.active ? 1.15 : 1

            Accessible.role: Accessible.Note
            Accessible.name: modelData.text

            Behavior on scale {
                NumberAnimation {
                    duration: Kirigami.Units.shortDuration
                }
            }

            // Lines of writing, as on a note seen from afar.
            Column {
                anchors.centerIn: parent
                spacing: Math.max(2, Math.round(sticky.height / 7))

                Repeater {
                    model: 3

                    Rectangle {
                        required property int index

                        width: Math.round(sticky.width * (index === 2 ? 0.35 : 0.6))
                        height: 1
                        color: root.writing
                        opacity: 0.6
                    }
                }
            }

            QQC2.ToolTip.text: modelData.text.length > 400 ? modelData.text.slice(0, 400) + "…" : modelData.text
            QQC2.ToolTip.visible: stickyHover.hovered && !stickyDrag.active && !editor.visible
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

            HoverHandler {
                id: stickyHover

                cursorShape: stickyDrag.active ? Qt.ClosedHandCursor : Qt.PointingHandCursor
            }
            TapHandler {
                onTapped: root.edit(sticky.modelData.id, sticky.modelData.text, sticky.modelData.modifiedText, sticky.modelData.x, sticky.modelData.y)
            }
            DragHandler {
                id: stickyDrag

                target: sticky
                xAxis.minimum: -sticky.width / 2
                xAxis.maximum: root.width - sticky.width / 2
                yAxis.minimum: -sticky.height / 2
                yAxis.maximum: root.height - sticky.height / 2
                onActiveChanged: {
                    if (!active) {
                        App.library.moveNote(sticky.modelData.id, (sticky.x + sticky.width / 2) / root.width, (sticky.y + sticky.height / 2) / root.height);
                    }
                }
            }
        }
    }

    // The note opened up.
    QQC2.Popup {
        id: editor

        property real noteId: 0
        property point at
        property string original
        property string modified

        // Whatever was written is kept when the note is put away; a note with nothing on it
        // is taken off.
        function commit(): void {
            const text = area.text.trim();
            if (noteId > 0) {
                if (text !== original) {
                    App.library.updateNote(noteId, text);
                }
            } else if (text.length > 0) {
                App.library.addNote(root.documentId, text, root.page, at.x, at.y);
            }
        }

        width: Kirigami.Units.gridUnit * 16
        padding: Kirigami.Units.smallSpacing
        closePolicy: QQC2.Popup.CloseOnEscape | QQC2.Popup.CloseOnPressOutside
        onClosed: commit()

        background: Rectangle {
            color: root.paper
            border.width: 1
            border.color: root.edge
            radius: 2
        }

        contentItem: ColumnLayout {
            spacing: Kirigami.Units.smallSpacing

            QQC2.ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: Kirigami.Units.gridUnit * 7

                QQC2.TextArea {
                    id: area

                    wrapMode: TextEdit.Wrap
                    textFormat: TextEdit.PlainText
                    color: root.writing
                    placeholderText: i18nc("@info:placeholder", "What is to be noted here")
                    placeholderTextColor: Qt.alpha(root.writing, 0.5)
                    background: null

                    // Ctrl+Enter puts the note away. Claimed here: left alone, the keys go
                    // to the window's action of opening the document in another program.
                    Keys.onShortcutOverride: event => {
                        if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && (event.modifiers & Qt.ControlModifier)) {
                            event.accepted = true;
                        }
                    }
                    Keys.onPressed: event => {
                        if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && (event.modifiers & Qt.ControlModifier)) {
                            editor.close();
                            event.accepted = true;
                        }
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                QQC2.Label {
                    Layout.fillWidth: true
                    text: editor.modified
                    elide: Text.ElideRight
                    font: Kirigami.Theme.smallFont
                    color: Qt.alpha(root.writing, 0.7)
                }
                QQC2.ToolButton {
                    visible: editor.noteId > 0
                    icon.name: "edit-delete"
                    text: i18nc("@action:button", "Delete Note")
                    display: QQC2.AbstractButton.IconOnly
                    onClicked: {
                        const id = editor.noteId;
                        // Nothing to keep of it when it closes.
                        editor.noteId = 0;
                        area.text = "";
                        editor.close();
                        App.library.removeNote(id);
                    }
                    QQC2.ToolTip.text: text
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                }
                QQC2.ToolButton {
                    icon.name: "dialog-ok-apply"
                    text: i18nc("@action:button put a note away", "Done")
                    onClicked: editor.close()
                }
            }
        }
    }
}
