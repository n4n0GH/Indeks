// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// What the middle of the window lists while the sidebar shows its Automation tab: the rules
// or the scripts of the open database, where the documents are listed otherwise. One of
// them is selected at a time; AutomationPane, where the preview is otherwise, changes it.
//
// Selecting is asked for, not done here: the pane may hold changes that were not saved.
QQC2.Pane {
    id: root

    required property Actions actions

    readonly property bool showsRules: App.automationPage === "rules"
    property var rules: []
    property var scripts: []
    readonly property var entries: showsRules ? rules : scripts
    /** How many documents are selected in the list of documents, for running a script on them. */
    readonly property int selectedDocuments: App.documents.selectedIds.length
    readonly property string problem: visible && !showsRules ? App.library.scriptsProblem() : ""

    /** The rule with this id is to be shown for changing. */
    signal ruleRequested(real id)
    /** The script in this file is to be shown for changing. */
    signal scriptRequested(string file)
    /** The entry that is selected is to be changed right now: the pane is wanted in view. */
    signal activated()

    function reload(): void {
        // Only what differs is put in: a list that is set anew loses its place.
        const nextRules = App.library.rules();
        if (JSON.stringify(nextRules) !== JSON.stringify(rules)) {
            rules = nextRules;
        }
        const nextScripts = App.library.scripts();
        if (JSON.stringify(nextScripts) !== JSON.stringify(scripts)) {
            scripts = nextScripts;
        }
    }

    function isCurrent(entry: var): bool {
        return showsRules ? entry.id === App.currentRule : entry.file === App.currentScript;
    }

    function request(entry: var): void {
        if (isCurrent(entry)) {
            return;
        }
        if (showsRules) {
            ruleRequested(entry.id);
        } else {
            scriptRequested(entry.file);
        }
    }

    // To the entry above or below the selected one.
    function step(delta: int): void {
        if (entries.length === 0) {
            return;
        }
        const at = entries.findIndex(entry => isCurrent(entry));
        const next = at < 0 ? (delta > 0 ? 0 : entries.length - 1) : Math.max(0, Math.min(entries.length - 1, at + delta));
        request(entries[next]);
        list.positionViewAtIndex(next, ListView.Contain);
    }

    // What a rule is for, in the words its conditions were picked by.
    function conditions(rule: var): string {
        const labels = (rule.filters ?? []).map(filter => filter.label ?? "").filter(label => label.length > 0);
        return labels.length > 0 ? labels.join(" · ") : rule.query ?? "";
    }

    function ruleState(rule: var): string {
        if (!(rule.enabled ?? true)) {
            return i18nc("@info state of a rule: it does nothing at the moment", "Not active");
        }
        return (rule.onImport ?? true) ? i18nc("@info state of a rule: it runs when a document is added", "Automatic")
                                       : i18nc("@info state of a rule: it runs when asked to", "By hand");
    }

    function scriptDetail(script: var): string {
        if (!script.allowed) {
            return i18nc("@info", "Written or changed somewhere else. Read it and allow it before it runs on this computer.");
        }
        const parts = [];
        if (script.description.length > 0) {
            parts.push(script.description);
        }
        if (script.rules.length > 0) {
            parts.push(i18ncp("@info the rules that run a script", "Run by the rule %2", "Run by the rules %2", script.rules.length, script.rules.join(", ")));
        }
        return parts.join(" · ");
    }

    padding: 0
    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false

    Component.onCompleted: reload()

    Connections {
        target: App.library
        function onChanged(): void {
            root.reload();
        }
        function onScriptsChanged(): void {
            root.reload();
        }
        function onDatabaseChanged(): void {
            root.reload();
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

        // What is listed, and the ways to add to it.
        QQC2.ToolBar {
            Layout.fillWidth: true
            position: QQC2.ToolBar.Header

            contentItem: RowLayout {
                spacing: Kirigami.Units.mediumSpacing

                QQC2.Label {
                    Layout.leftMargin: Kirigami.Units.smallSpacing
                    Layout.minimumHeight: Kirigami.Units.iconSizes.smallMedium + Kirigami.Units.smallSpacing * 2
                    verticalAlignment: Text.AlignVCenter
                    text: root.showsRules ? i18ncp("@info:status", "%1 rule", "%1 rules", root.rules.length)
                                       : i18ncp("@info:status", "%1 script", "%1 scripts", root.scripts.length)
                    color: Kirigami.Theme.disabledTextColor
                }
                Kirigami.ActionToolBar {
                    Layout.fillWidth: true
                    alignment: Qt.AlignRight
                    actions: root.showsRules ? [root.actions.newRule] : [root.actions.newScript, openFolder, root.actions.scriptOutput, root.actions.stopScripts]

                    Kirigami.Action {
                        id: openFolder

                        text: i18nc("@action:button", "Open Folder")
                        icon.name: "document-open-folder"
                        tooltip: i18nc("@info:tooltip", "Scripts are files in the folder of the database and can be written with any editor")
                        onTriggered: App.openScriptsFolder()
                    }
                }
            }
        }
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            position: Kirigami.InlineMessage.Position.Header
            visible: root.problem.length > 0
            type: Kirigami.MessageType.Error
            text: root.problem
        }

        QQC2.ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true

            QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

            ListView {
                id: list

                model: root.entries
                clip: true
                activeFocusOnTab: true
                keyNavigationEnabled: false

                Keys.onUpPressed: root.step(-1)
                Keys.onDownPressed: root.step(1)
                Keys.onReturnPressed: root.activated()
                Keys.onEnterPressed: root.activated()
                Keys.onDeletePressed: {
                    if (root.actions.automationDelete.enabled) {
                        root.actions.automationDelete.trigger();
                    }
                }

                delegate: QQC2.ItemDelegate {
                    id: entry

                    required property int index
                    required property var modelData
                    readonly property bool current: root.isCurrent(modelData)
                    // A rule that does nothing at the moment, a script that may not run.
                    readonly property bool idle: root.showsRules ? !(modelData.enabled ?? true) : !(modelData.allowed ?? true)
                    readonly property string detail: root.showsRules ? root.conditions(modelData) : root.scriptDetail(modelData)

                    width: ListView.view?.width ?? 0
                    highlighted: current
                    onClicked: {
                        list.forceActiveFocus();
                        root.request(modelData);
                    }
                    onDoubleClicked: root.activated()

                    Accessible.name: modelData.name

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.largeSpacing

                        Kirigami.Icon {
                            source: root.showsRules ? "run-build-configure" : entry.modelData.allowed ? "text-x-python" : "dialog-warning"
                            selected: entry.highlighted
                            opacity: root.showsRules && entry.idle ? 0.5 : 1
                            implicitWidth: Kirigami.Units.iconSizes.smallMedium
                            implicitHeight: implicitWidth
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            QQC2.Label {
                                Layout.fillWidth: true
                                text: entry.modelData.name
                                textFormat: Text.PlainText
                                elide: Text.ElideRight
                                color: entry.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                            }
                            QQC2.Label {
                                Layout.fillWidth: true
                                visible: text.length > 0
                                text: entry.detail
                                textFormat: Text.PlainText
                                elide: Text.ElideRight
                                font: Kirigami.Theme.smallFont
                                color: entry.highlighted ? Kirigami.Theme.highlightedTextColor
                                    : !root.showsRules && entry.idle ? Kirigami.Theme.neutralTextColor : Kirigami.Theme.disabledTextColor
                            }
                        }
                        QQC2.Label {
                            visible: root.showsRules
                            text: root.showsRules ? root.ruleState(entry.modelData) : ""
                            font: Kirigami.Theme.smallFont
                            color: entry.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.disabledTextColor
                        }
                        QQC2.Button {
                            visible: !root.showsRules
                            text: root.selectedDocuments > 0 ? i18ncp("@action:button run a script on the selected documents", "Run on %1 Document", "Run on %1 Documents", root.selectedDocuments)
                                                             : i18nc("@action:button", "Run")
                            icon.name: "system-run"
                            enabled: !root.showsRules && (entry.modelData.allowed ?? false) && root.problem.length === 0 && root.selectedDocuments > 0
                            onClicked: App.runScript(entry.modelData.file)

                            QQC2.ToolTip.text: root.selectedDocuments > 0 ? i18nc("@info:tooltip", "Runs the script on the documents that are selected in the list of documents")
                                                                          : i18nc("@info:tooltip", "Select the documents to run the script on in the list of documents first")
                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                        }
                    }

                    TapHandler {
                        acceptedButtons: Qt.RightButton
                        onTapped: {
                            list.forceActiveFocus();
                            root.request(entry.modelData);
                            entryMenu.popup();
                        }
                    }
                }

                Kirigami.PlaceholderMessage {
                    anchors.centerIn: parent
                    width: parent.width - Kirigami.Units.gridUnit * 4
                    visible: list.count === 0
                    icon.name: root.showsRules ? "run-build-configure" : "text-x-python"
                    text: root.showsRules ? i18nc("@info:placeholder", "No rules yet") : i18nc("@info:placeholder", "No scripts yet")
                    explanation: root.showsRules
                        ? i18nc("@info:placeholder", "A rule says which documents it is for and what is to happen to them: a tag, a group, a pin. It can do so by itself whenever a document is added.")
                        : i18nc("@info:placeholder", "A script is a small Python program that looks at documents and tags, groups, pins or annotates them. Run it on the documents you select, or have a rule run it.")
                    helpfulAction: root.showsRules ? root.actions.newRule : root.actions.newScript
                }
            }
        }
    }

    QQC2.Menu {
        id: entryMenu

        QQC2.MenuItem {
            action: root.actions.applyRule
            visible: root.showsRules
            height: visible ? implicitHeight : 0
        }
        QQC2.MenuItem {
            action: root.actions.automationDelete
        }
    }
}
