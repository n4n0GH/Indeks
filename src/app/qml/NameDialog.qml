// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

// Asks for one line of text, such as the name of a new group.
Kirigami.PromptDialog {
    id: root

    property var callback: null
    property alias placeholderText: field.placeholderText

    function ask(title: string, subtitle: string, initialText: string, callback: var): void {
        root.title = title;
        root.subtitle = subtitle;
        root.callback = callback;
        field.text = initialText;
        open();
        field.forceActiveFocus();
        field.selectAll();
    }

    // The answer is handed over once the dialog is gone. What the callback opens or focuses
    // would otherwise lose the keyboard focus again when the closing dialog gives focus back.
    property string acceptedText

    standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel
    onAccepted: acceptedText = field.text.trim()
    onOpened: {
        acceptedText = "";
        standardButton(Kirigami.Dialog.Ok).enabled = Qt.binding(() => field.text.trim().length > 0);
    }
    onClosed: {
        const text = acceptedText;
        acceptedText = "";
        if (text.length > 0 && callback) {
            callback(text);
        }
    }

    QQC2.TextField {
        id: field

        onAccepted: if (text.trim().length > 0) {
            root.accept();
        }
    }
}
