// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Where the preview is otherwise, while the window is about automation: the rule or
// the script that is selected in AutomationList, to be changed here.
//
// Rules and scripts each have an editor of their own, and each keeps what it holds while the
// other is looked at, or the documents are. What was changed and not saved is asked about
// only when it would be lost: when another rule, or another script, is to take its place.
QQC2.Pane {
    id: root

    required property Actions actions

    readonly property bool showsRules: App.automationPage === "rules"
    /** Something was changed and not saved, here or on the other page. */
    readonly property bool dirty: ruleEditor.dirty || scriptEditor.dirty
    /** Whether the page that is shown has a rule or script to show. */
    readonly property bool occupied: showsRules ? ruleEditor.loaded : scriptEditor.loaded

    /** Shows the rule @p id for changing, a new one for 0, once what is there may go. */
    function editRule(id: real): void {
        guarded(ruleEditor, () => {
            ruleEditor.load(id);
            App.currentRule = id;
            if (id === 0) {
                ruleEditor.focusName();
            }
        });
    }

    /** Shows the script in @p file for changing, a new one for none, once what is there may go. */
    function editScript(file: string): void {
        guarded(scriptEditor, () => {
            if (file.length === 0) {
                scriptEditor.create();
                App.currentScript = "";
            } else if (scriptEditor.edit(file)) {
                App.currentScript = file;
            }
        });
    }

    /** Calls @p then once nothing unsaved is in the way: at once, or after the user said so. */
    function guarded(editor: Item, then: var): void {
        if (editor.dirty) {
            unsaved.ask(editor, then);
        } else {
            then();
        }
    }

    /** As guarded(), for everything at once: before the window closes. */
    function guardedAll(then: var): void {
        guarded(ruleEditor, () => root.guarded(scriptEditor, then));
    }

    function showMatches(): void {
        const query = ruleEditor.query;
        App.goTo("place:all");
        App.documents.filter = query;
    }

    padding: 0
    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false

    Connections {
        target: ruleEditor
        function onSaved(id: real): void {
            App.currentRule = id;
        }
    }
    Connections {
        target: scriptEditor
        function onSaved(file: string): void {
            App.currentScript = file;
        }
    }

    // What is being changed here may be removed meanwhile, with its command or elsewhere.
    Connections {
        target: App.library
        function onChanged(): void {
            if (ruleEditor.loaded && ruleEditor.ruleId > 0 && App.library.rule(ruleEditor.ruleId).id === undefined) {
                ruleEditor.clear();
                App.currentRule = 0;
            }
        }
        function onScriptsChanged(): void {
            if (scriptEditor.loaded && scriptEditor.file.length > 0 && App.library.script(scriptEditor.file).file === undefined) {
                scriptEditor.clear();
                App.currentScript = "";
            }
        }
        function onDatabaseChanged(): void {
            // Rules and scripts belong to the database.
            ruleEditor.clear();
            scriptEditor.clear();
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

        QQC2.ToolBar {
            Layout.fillWidth: true
            position: QQC2.ToolBar.Header

            contentItem: RowLayout {
                spacing: Kirigami.Units.mediumSpacing

                QQC2.Label {
                    Layout.leftMargin: Kirigami.Units.smallSpacing
                    Layout.maximumWidth: Math.max(0, Math.round(root.width * 0.45))
                    Layout.minimumHeight: Kirigami.Units.iconSizes.smallMedium + Kirigami.Units.smallSpacing * 2
                    verticalAlignment: Text.AlignVCenter
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    color: Kirigami.Theme.disabledTextColor
                    text: {
                        if (!root.occupied) {
                            return "";
                        }
                        const editor = root.showsRules ? ruleEditor : scriptEditor;
                        const fresh = root.showsRules ? ruleEditor.ruleId === 0 : scriptEditor.file.length === 0;
                        if (fresh) {
                            return root.showsRules ? i18nc("@info:status", "New rule, not saved yet") : i18nc("@info:status", "New script, not saved yet");
                        }
                        return editor.dirty ? i18nc("@info:status %1 is the name of a rule or script", "%1, changed", editor.name) : editor.name;
                    }
                }
                Kirigami.ActionToolBar {
                    Layout.fillWidth: true
                    alignment: Qt.AlignRight
                    // What does not fit goes into a menu at the end; the pane can be narrow.
                    actions: !root.occupied ? [] : root.showsRules ? [saveRule, revertRule, root.actions.applyRule, matching, root.actions.automationDelete]
                                                                : [saveScript, revertScript, tryScript, runScript, reference, root.actions.automationDelete]

                    Kirigami.Action {
                        id: saveRule

                        text: i18nc("@action:button", "Save")
                        icon.name: "document-save"
                        enabled: ruleEditor.canSave
                        tooltip: ruleEditor.complete ? "" : i18nc("@info:tooltip", "A rule needs a name, something to match and something to do")
                        onTriggered: ruleEditor.save()
                    }
                    Kirigami.Action {
                        id: revertRule

                        text: i18nc("@action:button", "Discard Changes")
                        icon.name: "edit-undo"
                        displayHint: Kirigami.DisplayHint.IconOnly
                        enabled: ruleEditor.dirty
                        onTriggered: ruleEditor.revert()
                    }
                    Kirigami.Action {
                        id: matching

                        text: i18nc("@action:button", "Show Matching Documents")
                        icon.name: "search"
                        displayHint: Kirigami.DisplayHint.IconOnly
                        tooltip: i18nc("@info:tooltip", "Shows the documents the conditions find at the moment, in the list of documents")
                        enabled: ruleEditor.query.trim().length > 0
                        onTriggered: root.showMatches()
                    }

                    Kirigami.Action {
                        id: saveScript

                        text: i18nc("@action:button", "Save")
                        icon.name: "document-save"
                        enabled: scriptEditor.canSave
                        onTriggered: scriptEditor.save()
                    }
                    Kirigami.Action {
                        id: revertScript

                        text: i18nc("@action:button", "Discard Changes")
                        icon.name: "edit-undo"
                        displayHint: Kirigami.DisplayHint.IconOnly
                        enabled: scriptEditor.dirty
                        onTriggered: scriptEditor.revert()
                    }
                    Kirigami.Action {
                        id: tryScript

                        text: i18nc("@action:button", "Try")
                        icon.name: "debug-run"
                        enabled: scriptEditor.canTry
                        tooltip: scriptEditor.currentDocument > 0
                            ? i18nc("@info:tooltip %1 is the name of a document", "Shows what the script would do to “%1”, the document selected in the list of documents, without doing it", scriptEditor.currentDocumentName)
                            : i18nc("@info:tooltip", "Select a document in the list of documents first")
                        onTriggered: scriptEditor.tryOut()
                    }
                    Kirigami.Action {
                        id: runScript

                        readonly property int selected: App.documents.selectedIds.length

                        text: i18nc("@action:button", "Run")
                        icon.name: "system-run"
                        displayHint: Kirigami.DisplayHint.IconOnly
                        enabled: scriptEditor.file.length > 0 && !scriptEditor.dirty && scriptEditor.allowed && scriptEditor.problem.length === 0 && selected > 0
                        tooltip: selected > 0 ? i18ncp("@info:tooltip", "Runs the script on the document that is selected in the list of documents",
                                                       "Runs the script on the %1 documents that are selected in the list of documents", selected)
                                              : i18nc("@info:tooltip", "Select the documents to run the script on in the list of documents first")
                        onTriggered: App.runScript(scriptEditor.file)
                    }
                    Kirigami.Action {
                        id: reference

                        text: i18nc("@action:button", "Reference")
                        icon.name: "help-contents"
                        displayHint: Kirigami.DisplayHint.IconOnly
                        tooltip: i18nc("@info:tooltip", "What a script can ask of a document and do to it")
                        onTriggered: scriptEditor.showReference()
                    }
                }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: !root.occupied ? 0 : root.showsRules ? 1 : 2

            Item {
                Kirigami.PlaceholderMessage {
                    anchors.centerIn: parent
                    width: parent.width - Kirigami.Units.gridUnit * 4
                    icon.name: root.showsRules ? "run-build-configure" : "text-x-python"
                    text: root.showsRules ? i18nc("@info:placeholder", "Select a rule to change it here") : i18nc("@info:placeholder", "Select a script to change it here")
                }
            }
            RuleEditor {
                id: ruleEditor
            }
            ScriptEditor {
                id: scriptEditor
            }
        }
    }

    // What was changed and not saved, before something else takes its place.
    Kirigami.PromptDialog {
        id: unsaved

        property Item editor: null
        property var then: null

        function ask(editor: Item, then: var): void {
            unsaved.editor = editor;
            unsaved.then = then;
            open();
        }

        function proceed(): void {
            const next = then;
            then = null;
            close();
            if (next) {
                next();
            }
        }

        parent: root.QQC2.Overlay.overlay
        title: i18nc("@title:window", "Save Changes?")
        subtitle: editor === ruleEditor ? i18nc("@info %1 is the name of a rule", "The changes to the rule “%1” have not been saved.", ruleEditor.name)
                                        : i18nc("@info %1 is the name of a script", "The changes to the script “%1” have not been saved.", scriptEditor.name)
        standardButtons: Kirigami.Dialog.Cancel
        customFooterActions: [
            Kirigami.Action {
                text: i18nc("@action:button", "Save")
                icon.name: "document-save"
                enabled: unsaved.editor?.canSave ?? false
                onTriggered: {
                    if (unsaved.editor.save()) {
                        unsaved.proceed();
                    }
                }
            },
            Kirigami.Action {
                text: i18nc("@action:button", "Discard")
                icon.name: "edit-delete"
                onTriggered: {
                    // As it was stored, so that it no longer counts as changed.
                    unsaved.editor.revert();
                    unsaved.proceed();
                }
            }
        ]
    }
}
