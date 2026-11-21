// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Saves a search so that it appears in the sidebar, or edits a saved one.
Kirigami.Dialog {
    id: root

    property real searchId: 0

    function ask(searchId: real, name: string, query: string): void {
        root.searchId = searchId;
        nameField.text = name;
        queryField.text = query;
        open();
        nameField.forceActiveFocus();
    }

    readonly property bool canSubmit: nameField.text.trim().length > 0 && queryField.text.trim().length > 0

    // Also reached with Enter from the text fields.
    function submit(): void {
        if (!canSubmit) {
            return;
        }
        const id = App.library.saveSearch(root.searchId, nameField.text, queryField.text);
        if (id > 0) {
            App.goTo("search:" + id);
        }
        root.close();
    }

    title: searchId > 0 ? i18nc("@title:window", "Edit Saved Search") : i18nc("@title:window", "Save Search")
    padding: Kirigami.Units.largeSpacing
    preferredWidth: Kirigami.Units.gridUnit * 26
    standardButtons: Kirigami.Dialog.NoButton
    customFooterActions: [
        Kirigami.Action {
            text: i18nc("@action:button", "Save")
            icon.name: "document-save"
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
            id: nameField

            onAccepted: root.submit()

            Kirigami.FormData.label: i18nc("@label:textbox", "Name:")
            Layout.fillWidth: true
        }
        QQC2.TextField {
            id: queryField

            onAccepted: root.submit()

            Kirigami.FormData.label: i18nc("@label:textbox", "Search for:")
            Layout.fillWidth: true
            placeholderText: i18nc("@info:placeholder example of a search", "invoice kind:pdf added:30d")
        }
        QQC2.Label {
            Layout.fillWidth: true
            Layout.maximumWidth: Kirigami.Units.gridUnit * 20
            text: i18nc("@info", "Words find names, content and notes. Filters: tag:, group:, kind:, ext:, source:, name:, added:, modified:, size:, is:inbox, is:annotated, is:pinned, is:untagged. A minus in front excludes, OR between two matches either.")
            wrapMode: Text.Wrap
            font: Kirigami.Theme.smallFont
            color: Kirigami.Theme.disabledTextColor
        }
    }
}
