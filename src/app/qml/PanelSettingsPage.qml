// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// The sidebar and the side panel: what is set here beyond where their views are, which is
// arranged in the window itself (see PanelArea).
FormCard.FormCardPage {
    id: root

    title: i18nc("@title:window", "Panels")

    FormCard.FormHeader {
        title: i18nc("@title:group", "Tabs")
    }
    FormCard.FormCard {
        FormCard.FormSwitchDelegate {
            text: i18nc("@option:check", "Hide the tab bar where there is only one view")
            description: i18nc("@info", "An area of the sidebar or the side panel that holds a single view is shown without the bar of tabs above it.")
            checked: Config.hideSingleTabBar
            onToggled: {
                Config.hideSingleTabBar = checked;
                Config.save();
            }
        }
    }
    FormCard.FormSectionText {
        text: i18nc("@info", "Without its tab, a view is moved with the commands of the command bar, “Move … to the Right Panel” for one, or by turning this off for the moment. Another view can still be dragged into the area.")
    }

    FormCard.FormHeader {
        title: i18nc("@title:group", "Arrangement")
    }
    FormCard.FormCard {
        FormCard.FormButtonDelegate {
            text: i18nc("@action:button", "Reset Sidebar and Side Panel")
            description: i18nc("@info", "Puts every view back where Indeks starts with it.")
            icon.name: "edit-reset"
            onClicked: App.action("panel_reset")?.trigger()
        }
    }
}
