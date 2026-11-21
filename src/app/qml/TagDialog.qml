// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

// Gives a tag to the selected documents: pick an existing one or type a new name.
Kirigami.PromptDialog {
    id: root

    property var ids: []

    function ask(ids: var): void {
        root.ids = ids;
        combo.model = App.library.tagNames();
        combo.editText = "";
        open();
        combo.forceActiveFocus();
    }

    title: i18nc("@title:window", "Add Tag")
    subtitle: i18ncp("@info", "Choose a tag for the selected document or type a new one.", "Choose a tag for the %1 selected documents or type a new one.", ids.length)
    standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel
    onAccepted: if (combo.editText.trim().length > 0) {
        App.library.addTag(ids, combo.editText);
    }
    onOpened: standardButton(Kirigami.Dialog.Ok).enabled = Qt.binding(() => combo.editText.trim().length > 0)

    QQC2.ComboBox {
        id: combo

        editable: true
        currentIndex: -1
        onAccepted: if (editText.trim().length > 0) {
            root.accept();
        }
        Accessible.name: i18nc("@label:textbox", "Tag")
    }
}
