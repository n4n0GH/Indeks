// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.syntaxhighlighting as Syntax

// Writes a script: its name, its text, and a way to try it on the selected document without
// anything being done to that document. Nothing is stored before save(); `dirty` says
// whether there is something to store.
Item {
    id: root

    /** The file of the script being edited; empty for one that was never saved. */
    property string file
    /** Whether there is a script here at all, a new one included. */
    property bool loaded: false
    property string savedName
    property string savedSource
    /** Whether the script may run on this computer as it is stored; see Scripts. */
    property bool allowed: true
    /** How the last try went: what ScriptHost reports. */
    property var trial: null
    property bool trying: false

    /** The script was stored, in this file. */
    signal saved(string file)

    readonly property string name: nameField.text.trim()
    readonly property bool dirty: loaded && (codeView.text !== savedSource || name !== savedName)
    readonly property bool canSave: loaded && name.length > 0 && (dirty || file.length === 0)
    readonly property real currentDocument: App.documents.currentId
    readonly property string currentDocumentName: currentDocument > 0 ? (App.library.documentInfo(currentDocument).name ?? "") : ""
    readonly property string problem: visible ? App.library.scriptsProblem() : ""
    readonly property bool canTry: loaded && currentDocument > 0 && !trying && problem.length === 0

    /** A new script, with the text every script starts from. What was here before is gone. */
    function create(): void {
        load("", "", App.library.newScriptText(), true);
        nameField.forceActiveFocus();
    }

    /** Shows the script in @p file for changing. False if there is no such script. */
    function edit(file: string): bool {
        const script = App.library.script(file);
        if (script.file === undefined) {
            return false;
        }
        load(script.file, script.name, script.source, script.allowed);
        return true;
    }

    function load(file: string, name: string, source: string, allowed: bool): void {
        root.file = file;
        root.savedName = name;
        root.savedSource = source;
        root.allowed = allowed;
        root.trial = null;
        root.trying = false;
        nameField.text = name;
        codeView.show(source, [], []);
        root.loaded = true;
    }

    /** Back to how the script is stored, or to the starting text for a new one. */
    function revert(): void {
        if (file.length > 0) {
            edit(file);
        } else {
            create();
        }
    }

    function clear(): void {
        load("", "", "", true);
        loaded = false;
    }

    function showReference(): void {
        reference.open();
    }

    function save(): bool {
        if (!canSave) {
            return file.length > 0;
        }
        const saved = App.library.saveScript(file, nameField.text, codeView.text);
        if (saved.length === 0) {
            return false; // the library has said why
        }
        file = saved;
        savedName = saved.slice(0, -3);
        nameField.text = savedName;
        savedSource = codeView.text;
        allowed = true; // what is saved here may run here
        root.saved(saved);
        return true;
    }

    function tryOut(): void {
        trial = null;
        trying = true;
        App.library.tryScript(nameField.text, codeView.text, [currentDocument]);
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

    Item {
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing

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

    Kirigami.Dialog {
        id: reference

        // In the middle of the window, not of the pane the editor is in.
        parent: root.QQC2.Overlay.overlay
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
