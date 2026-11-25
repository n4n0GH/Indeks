// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Changes a rule, or puts a new one together: which documents it is for, and what happens to
// them. Both are lists put together entry by entry: the conditions from the same filters the
// filter bar has, the actions from a menu. A new rule starts with neither. Nothing is stored
// before save(); `dirty` says whether there is something to store.
Item {
    id: root

    /** The rule being changed; 0 for one that was never saved. */
    property real ruleId: 0
    /** Whether there is a rule here at all, a new one included. */
    property bool loaded: false
    property var tags: []
    property var groups: []
    property var scripts: []

    /** The rule was stored, under this id. */
    signal saved(real id)

    readonly property string name: nameField.text.trim()
    /** What the conditions come to, in the search language. */
    readonly property string query: stack.query

    // Everything that is stored, as one text: what differs from what was loaded is unsaved.
    property int actionRevision: 0
    readonly property string snapshot: JSON.stringify({
        "name": name,
        "filters": stack.filters,
        "actions": actionRevision >= 0 ? actions() : [],
        "onImport": onImportBox.checked,
        "enabled": enabledBox.checked
    })
    property string savedSnapshot
    readonly property bool dirty: loaded && snapshot !== savedSnapshot
    /** How many documents the conditions find at the moment; -1 while that is not known. */
    readonly property int matchCount: matches.matches
    readonly property bool complete: name.length > 0 && stack.query.trim().length > 0 && actionModel.count > 0
    readonly property bool canSave: complete && (dirty || ruleId === 0)

    /** Shows the rule @p id for changing, or a new one for 0. What was here before is gone. */
    function load(id: real): void {
        ruleId = id;
        reloadChoices();
        const rule = id > 0 ? App.library.rule(id) : {};
        nameField.text = rule.name ?? "";
        const filters = rule.filters ?? [];
        // A rule from before conditions were put together from filters has its search only.
        stack.filters = filters.length > 0 || !rule.query ? filters.map(renamed) : [stack.wordsFilter(rule.query)];
        actionModel.clear();
        for (const action of rule.actions ?? []) {
            actionModel.append(entryOf(action));
        }
        onImportBox.checked = rule.onImport ?? true;
        enabledBox.checked = rule.enabled ?? true;
        loaded = true;
        savedSnapshot = snapshot;
    }

    /** Back to how the rule is stored. */
    function revert(): void {
        load(ruleId);
    }

    function clear(): void {
        loaded = false;
        ruleId = 0;
        nameField.text = "";
        stack.filters = [];
        actionModel.clear();
        savedSnapshot = snapshot;
    }

    function focusName(): void {
        nameField.forceActiveFocus();
    }

    // Pinned documents were called flagged once, and rules from then say so in their filters.
    function renamed(filter: var): var {
        const pinned = stack.states.find(state => state.query === "is:pinned");
        return filter.query === "is:flagged" ? { type: filter.type, label: pinned.label, query: pinned.query } : filter;
    }

    function reloadChoices(): void {
        tags = App.library.tags();
        groups = App.library.groups();
        scripts = App.library.scripts();
    }

    Connections {
        target: App.library
        enabled: root.visible
        function onScriptsChanged(): void {
            root.scripts = App.library.scripts();
        }
    }

    // Also reached with Enter from the name field. Returns the rule's id, 0 if it was not
    // stored.
    function save(): real {
        if (!canSave) {
            return 0;
        }
        const id = App.library.saveRule({
            "id": root.ruleId,
            "name": nameField.text,
            "query": stack.query,
            "filters": stack.filters,
            "actions": actions(),
            "onImport": onImportBox.checked,
            "enabled": enabledBox.checked
        });
        if (id > 0) {
            ruleId = id;
            savedSnapshot = snapshot;
            saved(id);
        }
        return id;
    }

    // ------------------------------------------------------------------------ the actions

    // What the rule does, in order. An action is an object with `type` and, where it needs
    // one, the `name` of a tag or the `id` of a group; see Library::saveRule(). The list
    // keeps them as entries which all have the same members, as a list model wants it.
    ListModel {
        id: actionModel

        // Whatever happens to the list is a change to the rule.
        onCountChanged: root.actionRevision++
        onRowsMoved: root.actionRevision++
        onDataChanged: root.actionRevision++
    }

    function entryOf(action: var): var {
        return { type: action.type, name: action.name ?? "", target: action.id ?? 0 };
    }

    function actionOf(entry: var): var {
        switch (entry.type) {
        case "tag":
        case "untag":
            return { type: entry.type, name: entry.name };
        case "group":
        case "ungroup":
            return { type: entry.type, id: entry.target };
        case "script":
            return { type: entry.type, name: entry.name };
        }
        return { type: entry.type };
    }

    function actions(): var {
        const list = [];
        for (let i = 0; i < actionModel.count; ++i) {
            list.push(actionOf(actionModel.get(i)));
        }
        return list;
    }

    function groupPath(id: real): string {
        const group = groups.find(group => group.id === id);
        return group ? group.path : i18nc("@item a group that no longer exists", "(deleted group)");
    }

    function actionLabel(action: var): string {
        switch (action.type) {
        case "tag":
            return i18nc("@item what a rule does", "Add the tag “%1”", action.name);
        case "untag":
            return i18nc("@item what a rule does", "Remove the tag “%1”", action.name);
        case "group":
            return i18nc("@item what a rule does", "Add to the group “%1”", groupPath(action.id));
        case "ungroup":
            return i18nc("@item what a rule does", "Remove from the group “%1”", groupPath(action.id));
        case "flag":
            return i18nc("@item what a rule does", "Pin");
        case "unflag":
            return i18nc("@item what a rule does", "Unpin");
        case "inbox":
            return i18nc("@item what a rule does", "Remove from the inbox");
        case "script":
            return scripts.some(script => script.file === action.name)
                ? i18nc("@item what a rule does", "Run the script “%1”", action.name.slice(0, -3))
                : i18nc("@item what a rule does; the script is gone", "Run the script “%1” (which is missing)", action.name.slice(0, -3));
        }
        return action.type;
    }

    function actionIcon(type: string): string {
        switch (type) {
        case "tag":
            return "tag-new";
        case "untag":
            return "tag-delete";
        case "group":
            return "folder-new";
        case "ungroup":
            return "folder";
        case "flag":
            return "pin";
        case "unflag":
            return "window-unpin";
        case "inbox":
            return "mail-mark-read";
        }
        return "system-run";
    }

    // What an action is about: two actions about the same thing make no sense side by side,
    // like adding a tag and removing it again. The one added last takes the other's place.
    function subject(action: var): string {
        switch (action.type) {
        case "tag":
        case "untag":
            return "tag:" + action.name.toLowerCase();
        case "group":
        case "ungroup":
            return "group:" + action.id;
        case "flag":
        case "unflag":
            return "flag";
        case "script":
            return "script:" + action.name;
        }
        return action.type;
    }

    function addAction(action: var): void {
        const about = subject(action);
        for (let i = 0; i < actionModel.count; ++i) {
            if (subject(actionOf(actionModel.get(i))) === about) {
                // In the place of the one it replaces: the order was chosen with care.
                actionModel.set(i, entryOf(action));
                return;
            }
        }
        actionModel.append(entryOf(action));
    }

    // A submenu to pick one of a list from, filled when it opens.
    component PickMenu: QQC2.Menu {
        id: pickMenu

        // Objects with `label` and whatever picked() is to hand on.
        property var entries: []

        signal picked(var entry)

        Instantiator {
            model: pickMenu.entries

            delegate: QQC2.MenuItem {
                required property var modelData

                text: modelData.label
                onTriggered: pickMenu.picked(modelData)
            }
            onObjectAdded: (index, object) => pickMenu.insertItem(index, object)
            onObjectRemoved: (index, object) => pickMenu.removeItem(object)
        }
    }

    QQC2.ScrollView {
        id: scroll

        anchors.fill: parent
        contentWidth: availableWidth
        contentHeight: form.implicitHeight + Kirigami.Units.largeSpacing * 2
        QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

        ColumnLayout {
            id: form

            // Not wider than reads well, however wide the pane is.
            x: Kirigami.Units.largeSpacing
            y: Kirigami.Units.largeSpacing
            width: Math.min(scroll.availableWidth - Kirigami.Units.largeSpacing * 2, Kirigami.Units.gridUnit * 40)
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
                    onAccepted: root.save()
                }
            }

            // ---------------------------------------------------------------- the conditions
            Kirigami.Heading {
                Layout.fillWidth: true
                Layout.topMargin: Kirigami.Units.smallSpacing
                level: 4
                text: i18nc("@title:group", "For documents that match all of this")
            }
            FilterStack {
                id: stack

                Layout.fillWidth: true
                withWords: true
                listed: true
                // A rule is also for documents that are not there yet.
                emptyChoices: true
                hint: i18nc("@info", "Nothing yet. Add what a document has to match.")
            }
            MatchCount {
                id: matches

                Layout.fillWidth: true
                filterStack: stack
                active: root.visible
            }

            // ------------------------------------------------------------------- the actions
            Kirigami.Heading {
                Layout.fillWidth: true
                Layout.topMargin: Kirigami.Units.smallSpacing
                level: 4
                text: i18nc("@title:group", "Do this, in this order")
            }
            EntryFrame {
                Layout.fillWidth: true
                hint: i18nc("@info", "Nothing yet. Add what is to happen to the documents.")
                empty: actionModel.count === 0
                implicitHeight: Math.max(actionList.contentHeight + 2, addActionButton.height + Kirigami.Units.smallSpacing)

                contentItem: ListView {
                    id: actionList

                    interactive: false
                    model: actionModel

                    moveDisplaced: Transition {
                        YAnimator {
                            duration: Kirigami.Units.shortDuration
                            easing.type: Easing.InOutQuad
                        }
                    }

                    // The line itself is a child of the delegate: the handle takes it out of its
                    // place while it is dragged.
                    delegate: Item {
                        id: line

                        required property int index
                        required property var model

                        width: actionList.width
                        height: entry.implicitHeight + (index > 0 ? 1 : 0)

                        Kirigami.Separator {
                            width: parent.width
                            visible: line.index > 0
                        }
                        EntryRow {
                            id: entry

                            y: line.index > 0 ? 1 : 0
                            width: line.width
                            index: line.index
                            count: actionModel.count
                            label: root.actionLabel(root.actionOf(line.model))
                            iconName: root.actionIcon(line.model.type)
                            reorderView: actionList
                            onMoveRequested: (from, to) => actionModel.move(from, to, 1)
                            onRemoveRequested: actionModel.remove(line.index)
                        }
                    }
                }
            }
            QQC2.Button {
                id: addActionButton

                text: i18nc("@action:button", "Add Action")
                icon.name: "list-add"
                down: actionMenu.visible
                onClicked: actionMenu.popup(addActionButton, 0, addActionButton.height)
            }

            // ------------------------------------------------------------------------- when
            Kirigami.Heading {
                Layout.fillWidth: true
                Layout.topMargin: Kirigami.Units.smallSpacing
                level: 4
                text: i18nc("@title:group", "When")
            }
            QQC2.CheckBox {
                id: onImportBox

                text: i18nc("@option:check", "Automatically, when a document is added")
            }
            QQC2.CheckBox {
                id: enabledBox

                text: i18nc("@option:check", "Rule is active")
            }
            QQC2.Label {
                Layout.fillWidth: true
                text: i18nc("@info", "A rule that is not run automatically waits to be applied by hand, with Apply Selected Rule above.")
                wrapMode: Text.Wrap
                font: Kirigami.Theme.smallFont
                color: Kirigami.Theme.disabledTextColor
            }
            // Room below the last line when the pane is scrolled to its end.
            Item {
                implicitHeight: Kirigami.Units.largeSpacing
            }
        }
    }

    QQC2.Menu {
        id: actionMenu

        // Nothing in here is ever greyed out for want of a tag or a group: each list ends
        // with a way to name a new one.
        PickMenu {
            title: i18nc("@title:menu what a rule does", "Add Tag")
            icon.name: "tag-new"
            entries: root.tags.map(tag => ({ label: tag.name, name: tag.name }))
            onPicked: entry => root.addAction({ type: "tag", name: entry.name })

            QQC2.MenuSeparator {
                visible: root.tags.length > 0
                height: visible ? implicitHeight : 0
            }
            QQC2.MenuItem {
                text: i18nc("@action:inmenu", "New Tag…")
                icon.name: "list-add"
                onTriggered: prompt.askForTag("tag")
            }
        }
        PickMenu {
            title: i18nc("@title:menu what a rule does", "Remove Tag")
            icon.name: "tag-delete"
            entries: root.tags.map(tag => ({ label: tag.name, name: tag.name }))
            onPicked: entry => root.addAction({ type: "untag", name: entry.name })

            QQC2.MenuSeparator {
                visible: root.tags.length > 0
                height: visible ? implicitHeight : 0
            }
            QQC2.MenuItem {
                // A tag that does not exist yet may well come from another rule later.
                text: i18nc("@action:inmenu", "Other Tag…")
                icon.name: "edit-rename"
                onTriggered: prompt.askForTag("untag")
            }
        }
        QQC2.MenuSeparator {}
        PickMenu {
            title: i18nc("@title:menu what a rule does", "Add to Group")
            icon.name: "folder-new"
            entries: root.groups.map(group => ({ label: group.path, id: group.id }))
            onPicked: entry => root.addAction({ type: "group", id: entry.id })

            QQC2.MenuSeparator {
                visible: root.groups.length > 0
                height: visible ? implicitHeight : 0
            }
            QQC2.MenuItem {
                text: i18nc("@action:inmenu", "New Group…")
                icon.name: "list-add"
                onTriggered: prompt.askForGroup("group")
            }
        }
        PickMenu {
            title: i18nc("@title:menu what a rule does", "Remove from Group")
            icon.name: "folder"
            entries: root.groups.map(group => ({ label: group.path, id: group.id }))
            onPicked: entry => root.addAction({ type: "ungroup", id: entry.id })

            QQC2.MenuSeparator {
                visible: root.groups.length > 0
                height: visible ? implicitHeight : 0
            }
            QQC2.MenuItem {
                text: i18nc("@action:inmenu", "New Group…")
                icon.name: "list-add"
                onTriggered: prompt.askForGroup("ungroup")
            }
        }
        QQC2.MenuSeparator {}
        QQC2.MenuItem {
            text: i18nc("@action:inmenu what a rule does", "Pin")
            icon.name: "pin"
            onTriggered: root.addAction({ type: "flag" })
        }
        QQC2.MenuItem {
            text: i18nc("@action:inmenu what a rule does", "Unpin")
            icon.name: "window-unpin"
            onTriggered: root.addAction({ type: "unflag" })
        }
        QQC2.MenuItem {
            text: i18nc("@action:inmenu what a rule does", "Remove from the Inbox")
            icon.name: "mail-mark-read"
            onTriggered: root.addAction({ type: "inbox" })
        }
        QQC2.MenuSeparator {}
        PickMenu {
            title: i18nc("@title:menu what a rule does", "Run Script")
            icon.name: "system-run"
            entries: root.scripts.map(script => ({ label: script.name, file: script.file }))
            onPicked: entry => root.addAction({ type: "script", name: entry.file })
            // Scripts are written in their own list; a rule picks from what is there.
            enabled: root.scripts.length > 0
        }
    }

    // Asks for the name of a tag or of a new group to use in an action. The tag is created
    // when the rule first applies; the group right away, since actions know groups by
    // their place in the index, not by name.
    Kirigami.Dialog {
        id: prompt

        // The type of action to add with the answer.
        property string actionType
        readonly property bool forGroup: actionType === "group" || actionType === "ungroup"

        function askForTag(actionType: string): void {
            show(actionType);
        }

        function askForGroup(actionType: string): void {
            parentBox.currentIndex = 0;
            show(actionType);
        }

        function show(actionType: string): void {
            prompt.actionType = actionType;
            promptField.text = "";
            open();
            promptField.forceActiveFocus();
        }

        function submit(): void {
            const name = promptField.text.trim();
            if (name.length === 0) {
                return;
            }
            if (forGroup) {
                const id = App.library.createGroup(name, parentBox.currentValue ?? 0);
                if (id <= 0) {
                    return;
                }
                root.reloadChoices();
                root.addAction({ type: actionType, id: id });
            } else {
                root.addAction({ type: actionType, name: name });
            }
            close();
        }

        title: forGroup ? i18nc("@title:window", "New Group") : actionType === "untag" ? i18nc("@title:window", "Remove Tag") : i18nc("@title:window", "New Tag")
        padding: Kirigami.Units.largeSpacing
        standardButtons: Kirigami.Dialog.NoButton
        customFooterActions: [
            Kirigami.Action {
                text: prompt.forGroup ? i18nc("@action:button", "Create Group") : i18nc("@action:button", "Add")
                icon.name: prompt.forGroup ? "folder-new" : "list-add"
                enabled: promptField.text.trim().length > 0
                onTriggered: prompt.submit()
            },
            Kirigami.Action {
                text: i18nc("@action:button", "Cancel")
                icon.name: "dialog-cancel"
                onTriggered: prompt.close()
            }
        ]

        ColumnLayout {
            spacing: Kirigami.Units.smallSpacing

            QQC2.TextField {
                id: promptField

                Layout.fillWidth: true
                Layout.preferredWidth: Kirigami.Units.gridUnit * 18
                placeholderText: prompt.forGroup ? i18nc("@info:placeholder", "Name of the group") : i18nc("@info:placeholder", "Name of the tag")
                onAccepted: prompt.submit()
            }
            RowLayout {
                Layout.fillWidth: true
                visible: prompt.forGroup && root.groups.length > 0
                spacing: Kirigami.Units.smallSpacing

                QQC2.Label {
                    text: i18nc("@label:listbox the group a new group is made in", "Inside:")
                }
                QQC2.ComboBox {
                    id: parentBox

                    Layout.fillWidth: true
                    textRole: "label"
                    valueRole: "id"
                    model: [{ id: 0, label: i18nc("@item:inlistbox a new group that is in no other group", "No other group") }]
                        .concat(root.groups.map(group => ({ id: group.id, label: group.path })))
                }
            }
            QQC2.Label {
                Layout.fillWidth: true
                Layout.preferredWidth: Kirigami.Units.gridUnit * 18
                visible: prompt.forGroup
                wrapMode: Text.WordWrap
                font: Kirigami.Theme.smallFont
                color: Kirigami.Theme.disabledTextColor
                text: i18nc("@info", "The group is created right away and stays even if the rule is not saved.")
            }
        }
    }
}
