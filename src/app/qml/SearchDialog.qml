// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Saves a search so that it appears in the sidebar, or changes a saved one. What the search
// finds is put together the way the conditions of a rule are: filter by filter, each of
// which can be changed and taken off by itself, with the number of documents they find
// below. Words to search for are one of the filters; the query language can be used there.
Kirigami.Dialog {
    id: root

    property real searchId: 0

    /**
     * Asks for the name under which what is being searched for at the moment is kept:
     * @p pieces are the pieces of the query language that narrow the list down (the place in
     * the sidebar, what is typed in the search field), @p filters what the filter bar adds.
     */
    function askNew(pieces: var, filters: var): void {
        searchId = 0;
        nameField.text = "";
        let all = [];
        for (const piece of pieces) {
            all = all.concat(stack.filtersOf(piece));
        }
        stack.filters = all.concat(filters);
        open();
        nameField.forceActiveFocus();
    }

    /** Shows the saved search @p searchId for changing. */
    function edit(searchId: real, name: string, query: string): void {
        root.searchId = searchId;
        nameField.text = name;
        // A search from before filters were kept with it, or one whose query was changed
        // without them, has its query only.
        const filters = App.library.savedSearchFilters(searchId);
        stack.filters = filters.length > 0 ? filters : stack.filtersOf(query);
        open();
        nameField.forceActiveFocus();
        nameField.selectAll();
    }

    readonly property bool canSubmit: nameField.text.trim().length > 0 && stack.query.trim().length > 0

    // Also reached with Enter from the name field.
    function submit(): void {
        if (!canSubmit) {
            return;
        }
        const id = App.library.saveSearch(root.searchId, nameField.text, stack.query, stack.filters);
        if (id > 0) {
            App.goTo("search:" + id);
        }
        root.close();
    }

    title: searchId > 0 ? i18nc("@title:window", "Edit Saved Search") : i18nc("@title:window", "Save Search")
    padding: Kirigami.Units.largeSpacing
    preferredWidth: Kirigami.Units.gridUnit * 30
    standardButtons: Kirigami.Dialog.NoButton
    // Not by a click beside it: what was put together here would be gone with it.
    closePolicy: QQC2.Popup.CloseOnEscape
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

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            QQC2.Label {
                text: i18nc("@label:textbox", "Name:")
            }
            QQC2.TextField {
                id: nameField

                Layout.fillWidth: true
                onAccepted: root.submit()
            }
        }

        Kirigami.Heading {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.smallSpacing
            level: 4
            text: i18nc("@title:group", "Find documents that match all of this")
        }
        FilterStack {
            id: stack

            Layout.fillWidth: true
            withWords: true
            listed: true
            // A search is also for documents that are not there yet.
            emptyChoices: true
            hint: i18nc("@info", "Nothing yet. Add what a document has to match.")
        }
        MatchCount {
            Layout.fillWidth: true
            filterStack: stack
            active: root.visible
        }
        QQC2.Label {
            Layout.fillWidth: true
            // Wraps to the width of the dialog instead of setting it.
            Layout.preferredWidth: Kirigami.Units.gridUnit * 26
            text: i18nc("@info", "The search finds what matches at the time it is opened. Words to search for are a filter too, and can use the search language: tag:, kind:, added:, a minus in front to exclude, OR between two to match either.")
            wrapMode: Text.Wrap
            font: Kirigami.Theme.smallFont
            color: Kirigami.Theme.disabledTextColor
        }
    }
}
