// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The filter bar: search criteria that stack, as in a web shop. Each one that is added shows
// as a chip and narrows the list down at once; any of them can be taken off again by itself.
// They add to what is typed in the search field rather than replacing it.
//
// Every filter is a piece of the query language of the search field, so the stack can be saved
// as a search like any other. The stack itself is FilterStack.
QQC2.Pane {
    id: root

    required property Actions actions

    property alias filters: stack.filters
    readonly property alias query: stack.query

    signal closeRequested()

    padding: Kirigami.Units.smallSpacing
    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    onQueryChanged: App.documents.criteria = query
    // Filters nobody can see must not go on narrowing the list.
    onVisibleChanged: {
        if (!visible) {
            filters = [];
        }
    }

    contentItem: RowLayout {
        spacing: Kirigami.Units.smallSpacing

        FilterStack {
            id: stack

            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            hint: i18nc("@info", "Filters narrow the list down one by one and add to what is typed in the search field.")
            within: [App.documents.scope, App.documents.filter]
            // With a word of one or two letters in the search, looking it up in a large index
            // takes seconds, and a menu would do so once for every choice.
            counting: !App.documents.searchTerms.some(term => term.trim().length < 3)
        }

        QQC2.Label {
            Layout.alignment: Qt.AlignTop
            Layout.preferredHeight: stack.buttonHeight
            verticalAlignment: Text.AlignVCenter
            visible: App.documents.effectiveFilter.length > 0
            color: Kirigami.Theme.disabledTextColor
            text: App.documents.truncated
                ? i18ncp("@info", "More than %1 document", "More than %1 documents", App.documents.count)
                : i18ncp("@info", "%1 document", "%1 documents", App.documents.count)
        }
        QQC2.ToolButton {
            Layout.alignment: Qt.AlignTop
            visible: root.filters.length > 0
            text: i18nc("@action:button", "Remove All Filters")
            icon.name: "edit-clear-all"
            display: QQC2.AbstractButton.IconOnly
            onClicked: root.filters = []

            QQC2.ToolTip.text: text
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
        }
        QQC2.ToolButton {
            Layout.alignment: Qt.AlignTop
            action: root.actions.saveSearch
            display: QQC2.AbstractButton.IconOnly

            QQC2.ToolTip.text: i18nc("@info:tooltip", "Keep this search in the sidebar; it finds what matches at the time it is opened")
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
        }
        QQC2.ToolButton {
            Layout.alignment: Qt.AlignTop
            text: i18nc("@action:button", "Close Filter Bar")
            icon.name: "dialog-close"
            display: QQC2.AbstractButton.IconOnly
            onClicked: root.closeRequested()

            QQC2.ToolTip.text: i18nc("@info:tooltip", "Remove all filters and close the filter bar")
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
        }
    }
}
