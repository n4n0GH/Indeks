// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The command bar (Ctrl+Alt+I): type a few letters, press Enter, and the action runs.
// It lists every action of the application, so all of Indeks can be driven from here.
Kirigami.SearchDialog {
    id: root

    function show(): void {
        text = "";
        App.actionsModel.filterString = "";
        open();
    }

    // The action runs once the bar is gone. A dialog it opens would otherwise lose the
    // keyboard focus again when the closing bar hands focus back to the window.
    property QtObject pendingAction: null

    function run(action: QtObject): void {
        if (!action.enabled) {
            return;
        }
        pendingAction = action;
        close();
    }

    onClosed: {
        const action = pendingAction;
        pendingAction = null;
        if (action) {
            action.trigger();
        }
    }

    searchFieldPlaceholderText: i18nc("@info:placeholder", "Type to find an action…")
    emptyText: i18nc("@info:placeholder", "No action matches")

    model: App.actionsModel
    onTextChanged: App.actionsModel.filterString = text
    onAccepted: if (currentItem) {
        currentItem.clicked();
    }

    delegate: QQC2.ItemDelegate {
        id: entry

        required property int index
        required property string decoration
        required property string displayName
        required property string shortcut
        required property QtObject qaction

        width: ListView.view.width
        highlighted: ListView.isCurrentItem
        enabled: qaction.enabled

        Accessible.name: displayName

        contentItem: RowLayout {
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                source: entry.decoration
                selected: entry.highlighted
                implicitWidth: Kirigami.Units.iconSizes.smallMedium
                implicitHeight: implicitWidth
            }
            QQC2.Label {
                Layout.fillWidth: true
                text: entry.displayName
                elide: Text.ElideRight
                color: entry.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
            }
            QQC2.Label {
                text: entry.shortcut
                color: entry.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.disabledTextColor
            }
        }

        onClicked: root.run(qaction)
    }
}
