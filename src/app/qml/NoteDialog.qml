// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Takes a note on a document. Notes are kept in the index and can be searched.
Kirigami.Dialog {
    id: root

    property real documentId: 0

    function ask(documentId: real, name: string): void {
        root.documentId = documentId;
        root.title = i18nc("@title:window", "Note on %1", name);
        area.text = "";
        open();
        area.forceActiveFocus();
    }

    padding: Kirigami.Units.largeSpacing
    preferredWidth: Kirigami.Units.gridUnit * 24
    standardButtons: Kirigami.Dialog.NoButton
    customFooterActions: [
        Kirigami.Action {
            text: i18nc("@action:button", "Add Note")
            icon.name: "list-add"
            enabled: area.text.trim().length > 0
            onTriggered: {
                App.library.addNote(root.documentId, area.text, -1);
                root.close();
            }
        },
        Kirigami.Action {
            text: i18nc("@action:button", "Cancel")
            icon.name: "dialog-cancel"
            onTriggered: root.close()
        }
    ]

    QQC2.TextArea {
        id: area

        implicitHeight: Kirigami.Units.gridUnit * 8
        wrapMode: TextEdit.Wrap
        placeholderText: i18nc("@info:placeholder", "Write your note here… (Ctrl+Enter adds it)")
        Keys.onPressed: event => {
            if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && (event.modifiers & Qt.ControlModifier) && text.trim().length > 0) {
                App.library.addNote(root.documentId, text, -1);
                root.close();
                event.accepted = true;
            }
        }
    }
}
