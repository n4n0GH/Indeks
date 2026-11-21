// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.syntaxhighlighting as Syntax

// Writes a script: its name, its text, and a way to try it on the selected document without
// anything being done to that document.
Kirigami.Dialog {
    id: root

    /** The file of the script being edited; empty for one that was never saved. */
    property string file
    property string savedName
    property string savedSource
    /** Whether the script may run on this computer as it is stored; see Scripts. */
    property bool allowed: true
    /** Called with the script's file once it is saved for the first time. */
    property var whenCreated: null
    /** How the last try went: what ScriptHost reports. */
    property var trial: null
    property bool trying: false

    readonly property bool dirty: codeView.text !== savedSource || nameField.text.trim() !== savedName
    readonly property bool canSave: nameField.text.trim().length > 0 && (dirty || file.length === 0)
    readonly property real currentDocument: App.documents.currentId
    readonly property string problem: visible ? App.library.scriptsProblem() : ""

    function create(whenCreated: var): void {
        load("", "", App.library.newScriptText(), true);
        root.whenCreated = whenCreated;
        nameField.forceActiveFocus();
    }

    function edit(file: string): void {
        const script = App.library.script(file);
        if (script.file === undefined) {
            return;
        }
        load(script.file, script.name, script.source, script.allowed);
        codeView.editor.forceActiveFocus();
    }

    function load(file: string, name: string, source: string, allowed: bool): void {
        root.file = file;
        root.savedName = name;
        root.savedSource = source;
        root.allowed = allowed;
        root.whenCreated = null;
        root.trial = null;
        root.trying = false;
        nameField.text = name;
        open();
        codeView.show(source, [], []);
    }

    function save(): bool {
        if (!canSave) {
            return file.length > 0;
        }
        const saved = App.library.saveScript(file, nameField.text, codeView.text);
        if (saved.length === 0) {
            return false; // the library has said why
        }
        const created = file.length === 0;
        file = saved;
        savedName = saved.slice(0, -3);
        nameField.text = savedName;
        savedSource = codeView.text;
        allowed = true; // what is saved here may run here
        if (created && whenCreated) {
            const callback = whenCreated;
            whenCreated = null;
            callback(saved);
        }
        return true;
    }

    function tryOut(): void {
        trial = null;
        trying = true;
        App.library.tryScript(nameField.text, codeView.text, [currentDocument]);
    }

    // Leaving with changes that were not saved takes a second thought.
    function leave(): void {
        if (dirty) {
            discard.open();
        } else {
            close();
        }
    }

    // What the last try came to, as text.
    readonly property string trialText: {
        if (!trial) {
            return "";
        }
        if (trial.problem.length > 0) {
            return trial.problem;
        }
        const entry = trial.entries[0];
        if (!entry) {
            return i18nc("@info", "The script was not run: the selected document is in the trash.");
        }
        const lines = [];
        if (entry.error.length > 0) {
            lines.push(entry.error);
        } else if (entry.actions.length === 0) {
            lines.push(i18nc("@info result of trying a script", "The script would leave “%1” as it is.", entry.name));
        } else {
            lines.push(i18nc("@info result of trying a script; a list follows", "With “%1” the script would:", entry.name));
            for (const action of entry.actions) {
                lines.push("  • " + texts.actionText(action));
            }
        }
        if (entry.output.length > 0) {
            lines.push("");
            lines.push(i18nc("@info result of trying a script; what it printed follows", "It printed:"));
            lines.push(entry.output.replace(/\n+$/, ""));
        }
        return lines.join("\n");
    }

    title: file.length > 0 ? i18nc("@title:window", "Edit Script") : i18nc("@title:window", "New Script")
    padding: Kirigami.Units.largeSpacing
    // Text that took a while to write is not lost to a click beside the dialog.
    closePolicy: QQC2.Popup.NoAutoClose
    showCloseButton: false
    standardButtons: Kirigami.Dialog.NoButton
    customFooterActions: [
        Kirigami.Action {
            text: i18nc("@action:button", "Reference")
            icon.name: "help-contents"
            onTriggered: reference.open()
        },
        Kirigami.Action {
            text: i18nc("@action:button", "Try on Selected Document")
            icon.name: "debug-run"
            enabled: root.currentDocument > 0 && !root.trying && root.problem.length === 0
            tooltip: root.currentDocument > 0 ? i18nc("@info:tooltip", "Shows what the script would do to the selected document, without doing it")
                                             : i18nc("@info:tooltip", "Select a document in the list first")
            onTriggered: root.tryOut()
        },
        Kirigami.Action {
            text: i18nc("@action:button", "Save")
            icon.name: "document-save"
            enabled: root.canSave
            onTriggered: root.save()
        },
        Kirigami.Action {
            text: i18nc("@action:button", "Close")
            icon.name: "dialog-close"
            onTriggered: root.leave()
        }
    ]

    ScriptTexts {
        id: texts
    }

    Connections {
        target: App.library
        enabled: root.visible
        function onScriptTried(report: var): void {
            root.trying = false;
            root.trial = report;
        }
    }

    // As much room as the window has to give, and no more: the text scrolls by itself. A layout
    // works out its own size, so the size is stated by an item around it.
    Item {
        implicitWidth: Math.max(Kirigami.Units.gridUnit * 24, Math.min(Kirigami.Units.gridUnit * 52, (root.parent?.width ?? 0) - Kirigami.Units.gridUnit * 6))
        implicitHeight: Math.max(Kirigami.Units.gridUnit * 14, Math.min(Kirigami.Units.gridUnit * 36, (root.parent?.height ?? 0) - Kirigami.Units.gridUnit * 10))

        ColumnLayout {
            anchors.fill: parent
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
                    placeholderText: i18nc("@info:placeholder", "What the script is called, in rules and in menus")
                    onAccepted: codeView.editor.forceActiveFocus()
                }
            }

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                visible: root.problem.length > 0
                type: Kirigami.MessageType.Error
                text: root.problem
            }
            Kirigami.InlineMessage {
                Layout.fillWidth: true
                visible: !root.allowed && !root.dirty
                type: Kirigami.MessageType.Warning
                text: i18nc("@info", "This script was written or changed somewhere else. It does not run on this computer until you allow it. Read it first: a script can read every document in this database and write what it finds into tags, comments and notes.")
                actions: [
                    Kirigami.Action {
                        text: i18nc("@action:button", "Allow on This Computer")
                        icon.name: "dialog-ok-apply"
                        onTriggered: {
                            App.library.allowScript(root.file);
                            root.allowed = true;
                        }
                    }
                ]
            }

            QQC2.Frame {
                Layout.fillWidth: true
                Layout.fillHeight: true
                padding: 1

                CodeView {
                    id: codeView

                    anchors.fill: parent
                    readOnly: false
                    indentWidth: 4
                    syntax: Syntax.Repository.definitionForName("Python")
                    onSaveRequested: root.save()
                    onEscapePressed: root.leave()
                }
            }

            // How trying it went.
            QQC2.BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                visible: root.trying
                running: visible
            }
            QQC2.ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(Kirigami.Units.gridUnit * 9, trialArea.implicitHeight)
                visible: root.trialText.length > 0 && !root.trying

                QQC2.TextArea {
                    id: trialArea

                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.Wrap
                    textFormat: TextEdit.PlainText
                    font: Kirigami.Theme.fixedWidthFont
                    text: root.trialText
                }
            }
        }
    }

    Kirigami.PromptDialog {
        id: discard

        title: i18nc("@title:window", "Discard Changes?")
        subtitle: i18nc("@info", "The changes to this script have not been saved.")
        standardButtons: Kirigami.Dialog.Cancel
        customFooterActions: [
            Kirigami.Action {
                text: i18nc("@action:button", "Save")
                icon.name: "document-save"
                enabled: nameField.text.trim().length > 0
                onTriggered: {
                    discard.close();
                    if (root.save()) {
                        root.close();
                    }
                }
            },
            Kirigami.Action {
                text: i18nc("@action:button", "Discard")
                icon.name: "edit-delete"
                onTriggered: {
                    discard.close();
                    root.close();
                }
            }
        ]
    }

    Kirigami.Dialog {
        id: reference

        title: i18nc("@title:window", "What a Script Can Do")
        padding: Kirigami.Units.largeSpacing
        preferredWidth: Kirigami.Units.gridUnit * 46
        standardButtons: Kirigami.Dialog.Close

        QQC2.TextArea {
            readOnly: true
            selectByMouse: true
            wrapMode: TextEdit.Wrap
            textFormat: TextEdit.PlainText
            font: Kirigami.Theme.fixedWidthFont
            background: null
            text: texts.reference
        }
    }
}
