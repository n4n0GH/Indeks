// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import org.kde.kirigami as Kirigami

// Asks before something is removed. The confirming button names what will happen.
Kirigami.PromptDialog {
    id: root

    property var callback: null
    property string confirmText
    property string confirmIcon

    function ask(title: string, subtitle: string, confirmText: string, confirmIcon: string, callback: var): void {
        root.title = title;
        root.subtitle = subtitle;
        root.confirmText = confirmText;
        root.confirmIcon = confirmIcon;
        root.callback = callback;
        open();
    }

    standardButtons: Kirigami.Dialog.Cancel
    customFooterActions: [
        Kirigami.Action {
            text: root.confirmText
            icon.name: root.confirmIcon
            onTriggered: {
                root.close();
                if (root.callback) {
                    root.callback();
                }
            }
        }
    ]
}
