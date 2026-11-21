// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The panel on the right, which serves more than one purpose: tabs at its top lead to what
// Indeks knows about the selected document and to the list of things to do.
QQC2.Pane {
    id: root

    required property Actions actions

    /** A note that is stuck to a page was clicked: the page is wanted in view. */
    signal noteActivated(var note)
    /** A to-do is to be changed. */
    signal todoEditRequested(real id)

    // In the order of the tabs.
    readonly property list<string> tabs: ["info", "todo"]
    readonly property int tabIndex: Math.max(0, tabs.indexOf(Config.rightPanelTab))

    padding: 0
    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    contentItem: ColumnLayout {
        spacing: 0

        Kirigami.NavigationTabBar {
            id: tabBar

            Layout.fillWidth: true
            position: QQC2.ToolBar.Header
            actions: [
                Kirigami.Action {
                    text: i18nc("@title:tab what is known about the selected document", "Information")
                    tooltip: i18nc("@info:tooltip", "Information on the selected document")
                    icon.name: "documentinfo"
                },
                Kirigami.Action {
                    text: i18nc("@title:tab the list of things to do", "To-Do")
                    tooltip: i18nc("@info:tooltip", "To-do list")
                    icon.name: "view-task"
                }
            ]
            // The tabs show their icons alone, which leaves the bar lower; what each is for
            // says its tooltip, and its name is still there for screen readers.
            delegate: Kirigami.NavigationTabButton {
                required property QQC2.Action modelData

                parent: tabBar.contentItem
                action: modelData
                // As the bar's own tabs do: an action is not checkable unless told to be.
                onActionChanged: action.checkable = true
                display: QQC2.AbstractButton.IconOnly
                implicitHeight: Kirigami.Units.gridUnit * 2 + Kirigami.Units.smallSpacing

                Layout.minimumWidth: tabBar.buttonWidth
                Layout.maximumWidth: tabBar.buttonWidth
                Layout.fillHeight: true

                Accessible.name: modelData.text
            }

            // The tab that is shown is a setting; the bar follows it and changes it.
            Component.onCompleted: currentIndex = root.tabIndex
            onCurrentIndexChanged: {
                if (currentIndex >= 0 && currentIndex !== root.tabIndex) {
                    App.showSidePanel(root.tabs[currentIndex]);
                }
            }

            Connections {
                target: root
                function onTabIndexChanged(): void {
                    tabBar.currentIndex = root.tabIndex;
                }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.tabIndex

            InfoPane {
                actions: root.actions
                onNoteActivated: note => root.noteActivated(note)
            }
            TodoPane {
                actions: root.actions
                onEditRequested: id => root.todoEditRequested(id)
            }
        }
    }
}
