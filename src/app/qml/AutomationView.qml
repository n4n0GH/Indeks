// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Automation, as a view of a panel: two entries, Rules and Scripts, each of which puts its
// list where the documents are listed otherwise.
//
// One of the views that the user puts into whichever area of whichever panel; see
// PanelLayout and SidePanel.
QQC2.Pane {
    id: root

    required property Actions actions

    property int ruleCount: 0
    property int scriptCount: 0

    function countAutomation(): void {
        ruleCount = App.library.rules().length;
        scriptCount = App.library.scripts().length;
    }

    // Shows the rules or the scripts in the middle of the window.
    function go(page: string): void {
        App.automationPage = page;
        App.sidebarTab = "automation";
    }

    Component.onCompleted: countAutomation()

    Connections {
        target: App.library
        function onChanged(): void {
            root.countAutomation();
        }
        function onScriptsChanged(): void {
            root.countAutomation();
        }
        function onDatabaseChanged(): void {
            root.countAutomation();
        }
    }

    padding: 0
    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false

    contentItem: QQC2.ScrollView {
        QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

        ListView {
            id: automationList

            clip: true
            activeFocusOnTab: true
            keyNavigationEnabled: false
            model: [
                { page: "rules", label: i18nc("@item sidebar", "Rules"), icon: "run-build-configure", count: root.ruleCount },
                { page: "scripts", label: i18nc("@item sidebar", "Scripts"), icon: "text-x-python", count: root.scriptCount }
            ]

            Keys.onUpPressed: root.go("rules")
            Keys.onDownPressed: root.go("scripts")

            delegate: QQC2.ItemDelegate {
                id: page

                required property var modelData

                width: ListView.view?.width ?? 0
                // The one whose list the middle of the window shows, while it shows one.
                highlighted: App.automation && App.automationPage === modelData.page
                topPadding: Kirigami.Units.smallSpacing
                bottomPadding: Kirigami.Units.smallSpacing
                leftPadding: Kirigami.Units.largeSpacing
                onClicked: root.go(modelData.page)

                Accessible.name: modelData.label

                contentItem: RowLayout {
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Icon {
                        source: page.modelData.icon
                        selected: page.highlighted
                        implicitWidth: Kirigami.Units.iconSizes.smallMedium
                        implicitHeight: implicitWidth
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: page.modelData.label
                        elide: Text.ElideRight
                        color: page.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                    }
                    QQC2.Label {
                        visible: page.modelData.count > 0
                        text: page.modelData.count
                        color: page.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.disabledTextColor
                        font: Kirigami.Theme.smallFont
                    }
                }

                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: {
                        root.go(page.modelData.page);
                        automationMenu.popup();
                    }
                }
            }
        }
    }

    QQC2.Menu {
        id: automationMenu

        QQC2.MenuItem {
            action: App.automationPage === "rules" ? root.actions.newRule : root.actions.newScript
        }
    }
}
