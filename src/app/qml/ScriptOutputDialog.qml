// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// How the latest runs of scripts went: what they printed, where they failed and why.
Kirigami.Dialog {
    id: root

    property var reports: []

    function show(): void {
        reports = App.library.scriptReports();
        open();
    }

    // Everything a run has to tell, as text that can be selected and copied.
    function details(report: var): string {
        const lines = [];
        if (report.problem.length > 0) {
            lines.push(report.problem);
        }
        for (const entry of report.entries) {
            if (lines.length > 0) {
                lines.push("");
            }
            lines.push(i18nc("@info a document in the output of a script", "%1:", entry.name));
            if (entry.output.length > 0) {
                lines.push(entry.output.replace(/\n+$/, ""));
            }
            if (entry.error.length > 0) {
                lines.push(entry.error);
            }
        }
        if (report.more > 0) {
            lines.push("");
            lines.push(i18ncp("@info", "…and %1 more document with something to tell.", "…and %1 more documents with something to tell.", report.more));
        }
        return lines.join("\n");
    }

    title: i18nc("@title:window", "Script Output")
    preferredWidth: Kirigami.Units.gridUnit * 44
    preferredHeight: Kirigami.Units.gridUnit * 30
    standardButtons: Kirigami.Dialog.Close
    customFooterActions: [
        Kirigami.Action {
            text: i18nc("@action:button", "Clear")
            icon.name: "edit-clear-history"
            enabled: root.reports.length > 0
            onTriggered: {
                App.library.clearScriptReports();
                root.reports = [];
            }
        }
    ]

    ScriptTexts {
        id: texts
    }

    Connections {
        target: App.library
        enabled: root.visible
        function onScriptFinished(): void {
            root.reports = App.library.scriptReports();
        }
    }

    ListView {
        id: list

        implicitHeight: Kirigami.Units.gridUnit * 22
        model: root.reports
        clip: true
        spacing: 0

        delegate: ColumnLayout {
            id: run

            required property int index
            required property var modelData
            readonly property string details: root.details(modelData)

            width: ListView.view.width
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.largeSpacing

                Kirigami.Icon {
                    source: run.modelData.problem.length > 0 || run.modelData.failed > 0 ? "dialog-warning" : "text-x-python"
                    implicitWidth: Kirigami.Units.iconSizes.smallMedium
                    implicitHeight: Kirigami.Units.iconSizes.smallMedium
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    QQC2.Label {
                        Layout.fillWidth: true
                        text: run.modelData.rule.length > 0
                            ? i18nc("@info a script and the rule it ran for", "%1, for the rule “%2”", run.modelData.name, run.modelData.rule)
                            : run.modelData.name
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                        font.weight: Font.DemiBold
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: i18nc("@info when a script ran and how it went", "%1 · %2", run.modelData.time.toLocaleString(Qt.locale(), Locale.ShortFormat), texts.summary(run.modelData))
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                        font: Kirigami.Theme.smallFont
                        color: Kirigami.Theme.disabledTextColor
                    }
                }
            }
            QQC2.TextArea {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.largeSpacing
                Layout.rightMargin: Kirigami.Units.largeSpacing
                Layout.bottomMargin: Kirigami.Units.largeSpacing
                visible: run.details.length > 0
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.Wrap
                textFormat: TextEdit.PlainText
                font: Kirigami.Theme.fixedWidthFont
                text: run.details
            }
            Kirigami.Separator {
                Layout.fillWidth: true
                visible: run.index < list.count - 1
            }
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 2
            visible: list.count === 0
            icon.name: "text-x-python"
            text: i18nc("@info:placeholder", "No script has run yet")
            explanation: i18nc("@info:placeholder", "What scripts print, and the errors they run into, are kept here until Indeks is closed.")
        }
    }
}
