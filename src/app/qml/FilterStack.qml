// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Search criteria that stack, as in a web shop: a button to add one, and an entry for each that
// was added, which can be changed and taken off again by itself. What they come to is `query`,
// a piece of the query language of the search field.
//
// Used by the filter bar above the list of documents, where the entries are chips beside the
// button, and for the conditions of rules, where they are the lines of a list above it.
GridLayout {
    id: root

    // The stack: objects with `type`, `label` (what the chip says), `query` (its piece of the
    // query language) and, for those that can be edited, `data` to fill the editor from.
    property var filters: []
    readonly property string query: filters.map(filter => filter.query).join(" ")

    /** What to say while the stack is empty. */
    property string hint
    /**
     * The choices in the menus say how many documents each would leave. This is what is
     * searched besides the stack itself: pieces of the query language.
     */
    property var within: []
    /** Whether to count at all; counting takes a search for every choice. */
    property bool counting: true
    /**
     * Whether a choice that leaves no document can be made. Not where the filters narrow
     * down a list: such a choice would only empty it. But where they say what something is
     * for, as in a rule, a choice may well be for documents that are yet to come: the inbox
     * is empty most of the time, and a new tag has no documents.
     */
    property bool emptyChoices: false
    /** Offers words to search for as a filter: for where there is no search field. */
    property bool withWords: false
    /** Shows the filters as a list, one to a line, with the button to add one below it. */
    property bool listed: false
    readonly property real buttonHeight: addButton.height

    columns: listed ? 1 : 2
    rowSpacing: Kirigami.Units.smallSpacing
    columnSpacing: Kirigami.Units.smallSpacing

    // ---------------------------------------------------------------- the stack

    // Of some kinds there can only be one: a document has one size and one type.
    function isSingle(type: string): bool {
        return ["kind", "size", "modified", "added", "source"].includes(type);
    }

    function add(filter: var): void {
        const kept = isSingle(filter.type) ? filters.filter(other => other.type !== filter.type) : filters.filter(other => other.query !== filter.query);
        filters = kept.concat([filter]);
    }

    function removeAt(index: int): void {
        filters = filters.filter((filter, position) => position !== index);
    }

    function removeQuery(query: string): void {
        filters = filters.filter(filter => filter.query !== query);
    }

    function has(query: string): bool {
        return filters.some(filter => filter.query === query);
    }

    // Adds a filter, or takes it off if it is on: for the choices in the menus.
    function toggle(filter: var): void {
        if (has(filter.query)) {
            removeQuery(filter.query);
        } else {
            add(filter);
        }
    }

    // The icon that stands for a kind of filter in the list.
    function iconFor(type: string): string {
        switch (type) {
        case "words":
            return "search";
        case "name":
            return "edit-rename";
        case "content":
            return "edit-find";
        case "notes":
            return "note";
        case "folder":
        case "group":
            return "folder";
        case "extension":
            return "document-properties";
        case "kind":
            return "document-multiple";
        case "modified":
        case "added":
            return "view-calendar";
        case "size":
            return "transform-scale";
        case "tag":
            return "tag";
        case "source":
            return "folder-saved-search";
        case "state":
            return "pin";
        }
        return "view-filter";
    }

    // ---------------------------------------------------------------- building filters

    // A value as one token of the query language: in quotes if it would fall apart otherwise.
    function quoted(value: string): string {
        const cleaned = value.replace(/"/g, "").trim();
        return /\s/.test(cleaned) || cleaned === "OR" ? "\"" + cleaned + "\"" : cleaned;
    }

    // Words, with "several words in quotes" kept together.
    function words(text: string): list<string> {
        const result = [];
        const pattern = /"([^"]*)"|(\S+)/g;
        let match;
        while ((match = pattern.exec(text)) !== null) {
            const word = (match[1] ?? match[2]).trim();
            if (word.length > 0) {
                result.push(word);
            }
        }
        return result;
    }

    // A date as the query language wants it (2026-03-31), from that or from the way dates are
    // written here; empty if it is neither.
    function isoDate(text: string): string {
        const trimmed = text.trim();
        if (/^\d{4}-\d{2}-\d{2}$/.test(trimmed)) {
            return isNaN(Date.parse(trimmed)) ? "" : trimmed;
        }
        const date = Date.fromLocaleDateString(Qt.locale(), trimmed, Locale.ShortFormat);
        return isNaN(date.getTime()) ? "" : Qt.formatDate(date, "yyyy-MM-dd");
    }

    function nameFilter(text: string): var {
        const names = words(text);
        return {
            type: "name",
            label: i18nc("@item filter on the file name", "Name: %1", names.join(", ")),
            query: names.map(name => "name:" + quoted(name)).join(" OR "),
            data: { text: text }
        };
    }

    function contentFilter(text: string, match: string): var {
        const terms = words(text);
        if (match === "phrase") {
            return {
                type: "content",
                label: i18nc("@item filter on the text of documents", "Contains “%1”", text.trim()),
                query: "content:" + quoted(text.replace(/"/g, "")),
                data: { text: text, match: match }
            };
        }
        return {
            type: "content",
            label: match === "any" ? i18nc("@item filter on the text of documents", "Contains any of: %1", terms.join(", "))
                                   : i18nc("@item filter on the text of documents", "Contains: %1", terms.join(", ")),
            query: terms.map(word => "content:" + quoted(word)).join(match === "any" ? " OR " : " "),
            data: { text: text, match: match }
        };
    }

    function notesFilter(text: string): var {
        return {
            type: "notes",
            label: i18nc("@item filter on notes and comments", "Notes: %1", text.trim()),
            query: "note:" + quoted(text),
            data: { text: text }
        };
    }

    // Words as they are typed into the search field, where the stack has no search field
    // beside it: found in names, text and notes, and free to use the query language.
    function wordsFilter(text: string): var {
        return {
            type: "words",
            label: i18nc("@item filter: words to search for", "Search: %1", text.trim()),
            query: text.trim(),
            data: { text: text }
        };
    }

    function folderFilter(text: string): var {
        return {
            type: "folder",
            label: i18nc("@item filter on the folder a file is in", "Folder: %1", text.trim()),
            query: "path:" + quoted(text),
            data: { text: text }
        };
    }

    function extensionFilter(text: string): var {
        const extensions = text.split(/[\s,;]+/).map(ext => ext.replace(/^\*?\./, "")).filter(ext => ext.length > 0);
        return {
            type: "extension",
            label: i18nc("@item filter on the file name extension", "Extension: %1", extensions.join(", ")),
            query: extensions.map(ext => "ext:" + quoted(ext)).join(" OR "),
            data: { text: text }
        };
    }

    function dateFilter(field: string, mode: string, from: string, to: string): var {
        const modified = field === "modified";
        let query = "";
        let label = "";
        if (mode === "between") {
            query = field + ":>=" + from + " " + field + ":<=" + to;
            label = modified ? i18nc("@item filter", "Modified from %1 to %2", from, to) : i18nc("@item filter", "Added from %1 to %2", from, to);
        } else if (mode === "after") {
            query = field + ":>=" + from;
            label = modified ? i18nc("@item filter", "Modified on or after %1", from) : i18nc("@item filter", "Added on or after %1", from);
        } else {
            query = field + ":<" + from;
            label = modified ? i18nc("@item filter", "Modified before %1", from) : i18nc("@item filter", "Added before %1", from);
        }
        return { type: field, label: label, query: query, data: { mode: mode, from: from, to: to } };
    }

    function sizeFilter(operator: string, amount: int, unit: string): var {
        const units = { "K": i18nc("@item unit of file size", "KiB"), "M": i18nc("@item unit of file size", "MiB"), "G": i18nc("@item unit of file size", "GiB") };
        const size = i18nc("@item a file size: number and unit", "%1 %2", amount, units[unit]);
        return {
            type: "size",
            label: operator === ">=" ? i18nc("@item filter on file size", "At least %1", size) : i18nc("@item filter on file size", "At most %1", size),
            query: "size:" + operator + amount + unit,
            data: { operator: operator, amount: amount, unit: unit }
        };
    }

    // ---------------------------------------------------------------- choices with counts

    // How many documents are left with @p extra added to everything that already narrows them
    // down, leaving out the filter it would replace. Returns -1 for "not counted".
    function countWith(type: string, extra: string): int {
        if (!counting) {
            return -1;
        }
        const others = filters.filter(filter => !(isSingle(type) && filter.type === type) && filter.query !== extra).map(filter => filter.query);
        return App.library.count(within.concat(others, [extra]).filter(part => part.trim().length > 0).join(" "));
    }

    // Menu entries for choices: each with the number of documents it would leave.
    function choices(type: string, options: var): var {
        return options.map(option => ({
            type: type,
            label: option.label,
            chip: option.chip ?? option.label,
            query: option.query,
            active: has(option.query),
            count: countWith(type, option.query)
        }));
    }

    readonly property var kinds: [
        { query: "kind:text", label: i18nc("@item file type", "Plain text, code and data") },
        { query: "kind:rich", label: i18nc("@item file type", "Formatted text") },
        { query: "kind:pdf", label: i18nc("@item file type", "PDF documents") },
        { query: "kind:image", label: i18nc("@item file type", "Images") },
        { query: "kind:email", label: i18nc("@item file type", "E-mails") },
        { query: "kind:canvas", label: i18nc("@item file type", "Obsidian canvases") },
        { query: "kind:other", label: i18nc("@item file type", "Everything else") }
    ]
    readonly property var states: [
        { query: "is:pinned", label: i18nc("@item state of a document", "Pinned") },
        { query: "is:annotated", label: i18nc("@item state of a document", "With notes or drawings") },
        { query: "is:inbox", label: i18nc("@item state of a document", "In the inbox") },
        { query: "is:linked", label: i18nc("@item state of a document", "Linked to other documents") },
        { query: "is:unfiled", label: i18nc("@item state of a document", "In no group") },
        { query: "is:untagged", label: i18nc("@item state of a document", "Untagged") },
        { query: "is:copied", label: i18nc("@item state of a document", "Worked on in a copy") },
        { query: "is:duplicate", label: i18nc("@item state of a document", "Duplicates") },
        { query: "is:missing", label: i18nc("@item state of a document", "Missing their file") }
    ]

    function datePresets(field: string): var {
        const modified = field === "modified";
        return [
            { query: field + ":today", label: i18nc("@item when", "Today"), chip: modified ? i18nc("@item filter", "Modified today") : i18nc("@item filter", "Added today") },
            { query: field + ":yesterday", label: i18nc("@item when", "Yesterday"), chip: modified ? i18nc("@item filter", "Modified yesterday") : i18nc("@item filter", "Added yesterday") },
            { query: field + ":7d", label: i18nc("@item when", "In the last 7 days"), chip: modified ? i18nc("@item filter", "Modified in the last 7 days") : i18nc("@item filter", "Added in the last 7 days") },
            { query: field + ":30d", label: i18nc("@item when", "In the last 30 days"), chip: modified ? i18nc("@item filter", "Modified in the last 30 days") : i18nc("@item filter", "Added in the last 30 days") },
            { query: field + ":1y", label: i18nc("@item when", "In the last 12 months"), chip: modified ? i18nc("@item filter", "Modified in the last 12 months") : i18nc("@item filter", "Added in the last 12 months") }
        ];
    }

    readonly property var sizePresets: [
        { query: "size:<=100K", label: i18nc("@item file size", "At most 100 KiB") },
        { query: "size:>=1M", label: i18nc("@item file size", "At least 1 MiB") },
        { query: "size:>=10M", label: i18nc("@item file size", "At least 10 MiB") },
        { query: "size:>=100M", label: i18nc("@item file size", "At least 100 MiB") }
    ]

    // A submenu of choices. The numbers are worked out when it opens. Each entry is checked
    // while its filter is on, and greyed out when adding it would leave nothing.
    component ChoiceMenu: QQC2.Menu {
        id: choiceMenu

        property string type
        // A function that returns the options: objects with `query`, `label` and maybe `chip`.
        property var options
        property var entries: []

        onAboutToShow: entries = root.choices(type, options())

        Instantiator {
            model: choiceMenu.entries

            delegate: QQC2.MenuItem {
                required property var modelData

                text: modelData.count < 0 ? modelData.label
                    : i18nc("@item:inmenu a choice and the number of documents it leaves", "%1 (%2)", modelData.label, modelData.count)
                checkable: true
                checked: modelData.active
                enabled: modelData.count !== 0 || modelData.active || root.emptyChoices
                onTriggered: root.toggle({ type: modelData.type, label: modelData.chip, query: modelData.query })
            }
            onObjectAdded: (index, object) => choiceMenu.insertItem(index, object)
            onObjectRemoved: (index, object) => choiceMenu.removeItem(object)
        }
    }

    QQC2.Button {
        id: addButton

        Layout.row: root.listed ? 1 : 0
        Layout.column: 0
        Layout.alignment: Qt.AlignTop | Qt.AlignLeft
        text: i18nc("@action:button", "Add Filter")
        icon.name: "list-add"
        down: addMenu.visible
        onClicked: addMenu.popup(addButton, 0, addButton.height)
    }

    EntryFrame {
        Layout.row: 0
        Layout.column: 0
        Layout.fillWidth: true
        visible: root.listed
        hint: root.hint
        empty: root.filters.length === 0
        implicitHeight: Math.max(lines.implicitHeight + 2, root.buttonHeight + Kirigami.Units.smallSpacing)

        contentItem: Column {
            id: lines

            Repeater {
                model: root.listed ? root.filters : []

                delegate: Column {
                    id: line

                    required property int index
                    required property var modelData

                    width: lines.width

                    Kirigami.Separator {
                        width: parent.width
                        visible: line.index > 0
                    }
                    EntryRow {
                        width: parent.width
                        index: line.index
                        count: root.filters.length
                        label: line.modelData.label
                        iconName: root.iconFor(line.modelData.type)
                        // Filters that were typed in can be changed where they are.
                        editable: line.modelData.data !== undefined
                        onEditRequested: editor.edit(line.index, line.modelData)
                        onRemoveRequested: root.removeAt(line.index)
                    }
                }
            }
        }
    }

    Flow {
        id: chips

        Layout.row: 0
        Layout.column: 1
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignVCenter
        visible: !root.listed
        spacing: Kirigami.Units.smallSpacing

        Repeater {
            model: root.listed ? [] : root.filters

            delegate: Kirigami.Chip {
                id: chip

                required property int index
                required property var modelData

                text: modelData.label
                closable: true
                checkable: false
                onRemoved: root.removeAt(index)
                // Filters that were typed in can be changed where they are.
                onClicked: {
                    if (modelData.data !== undefined) {
                        editor.edit(index, modelData);
                    }
                }

                QQC2.ToolTip.text: modelData.data !== undefined ? i18nc("@info:tooltip", "Click to change, × to remove") : i18nc("@info:tooltip", "× removes this filter")
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            }
        }

        QQC2.Label {
            visible: root.filters.length === 0
            height: addButton.height
            verticalAlignment: Text.AlignVCenter
            color: Kirigami.Theme.disabledTextColor
            text: root.hint
            elide: Text.ElideRight
            width: Math.min(implicitWidth, chips.width)
        }
    }


    // ---------------------------------------------------------------- the menu of filters

    QQC2.Menu {
        id: addMenu

        QQC2.MenuItem {
            text: i18nc("@action:inmenu filter", "Search Words…")
            icon.name: "search"
            visible: root.withWords
            height: visible ? implicitHeight : 0
            onTriggered: editor.create("words")
        }
        QQC2.MenuItem {
            text: i18nc("@action:inmenu filter", "Name…")
            icon.name: "edit-rename"
            onTriggered: editor.create("name")
        }
        QQC2.MenuItem {
            text: i18nc("@action:inmenu filter", "Text in the Document…")
            icon.name: "edit-find"
            onTriggered: editor.create("content")
        }
        QQC2.MenuItem {
            text: i18nc("@action:inmenu filter", "Text in Notes and Comments…")
            icon.name: "note"
            onTriggered: editor.create("notes")
        }
        QQC2.MenuItem {
            text: i18nc("@action:inmenu filter", "Folder…")
            icon.name: "folder"
            onTriggered: editor.create("folder")
        }
        QQC2.MenuItem {
            text: i18nc("@action:inmenu filter", "Extension…")
            icon.name: "document-properties"
            onTriggered: editor.create("extension")
        }
        QQC2.MenuSeparator {}
        ChoiceMenu {
            title: i18nc("@title:menu filter", "File Type")
            type: "kind"
            options: () => root.kinds
        }
        ChoiceMenu {
            title: i18nc("@title:menu filter", "Date Modified")
            type: "modified"
            options: () => root.datePresets("modified")
        }
        ChoiceMenu {
            title: i18nc("@title:menu filter", "Date Added")
            type: "added"
            options: () => root.datePresets("added")
        }
        ChoiceMenu {
            title: i18nc("@title:menu filter", "Size")
            type: "size"
            options: () => root.sizePresets
        }
        QQC2.MenuSeparator {}
        ChoiceMenu {
            title: i18nc("@title:menu filter", "Tag")
            type: "tag"
            options: () => App.library.tags().map(tag => ({ query: "tag:" + root.quoted(tag.name), label: tag.name, chip: i18nc("@item filter", "Tag: %1", tag.name) }))
        }
        ChoiceMenu {
            title: i18nc("@title:menu filter", "Group")
            type: "group"
            options: () => App.library.groups().map(group => ({ query: "groupid:" + group.id, label: group.name, chip: i18nc("@item filter", "Group: %1", group.name) }))
        }
        ChoiceMenu {
            title: i18nc("@title:menu filter", "Location")
            type: "source"
            options: () => App.library.sources().map(source => ({ query: "sourceid:" + source.id, label: source.name, chip: i18nc("@item filter", "Location: %1", source.name) }))
        }
        ChoiceMenu {
            title: i18nc("@title:menu filter", "State")
            type: "state"
            options: () => root.states
        }
        QQC2.MenuSeparator {}
        QQC2.MenuItem {
            text: i18nc("@action:inmenu filter by dates chosen by hand", "Modified Between Dates…")
            icon.name: "view-calendar"
            onTriggered: editor.create("modified")
        }
        QQC2.MenuItem {
            text: i18nc("@action:inmenu filter by dates chosen by hand", "Added Between Dates…")
            icon.name: "view-calendar"
            onTriggered: editor.create("added")
        }
        QQC2.MenuItem {
            text: i18nc("@action:inmenu filter by a size chosen by hand", "Other Size…")
            icon.name: "transform-scale"
            onTriggered: editor.create("size")
        }
    }

    // ---------------------------------------------------------------- typing a filter in

    Kirigami.Dialog {
        id: editor

        property string type
        // Position in the stack of the filter being changed; -1 for a new one.
        property int position: -1

        readonly property bool isText: ["words", "name", "content", "notes", "folder", "extension"].includes(type)
        readonly property bool isDate: type === "modified" || type === "added"
        readonly property string fromDate: root.isoDate(fromField.text)
        readonly property string toDate: root.isoDate(toField.text)
        readonly property bool canSubmit: isText ? textField.text.trim().length > 0
            : isDate ? fromDate.length > 0 && (dateModeBox.currentValue !== "between" || toDate.length > 0)
            : true

        function create(type: string): void {
            show(type, -1, {});
        }

        function edit(position: int, filter: var): void {
            show(filter.type, position, filter.data);
        }

        function show(type: string, position: int, data: var): void {
            editor.type = type;
            editor.position = position;
            textField.text = data.text ?? "";
            matchBox.currentIndex = Math.max(0, matchBox.indexOfValue(data.match ?? "all"));
            dateModeBox.currentIndex = Math.max(0, dateModeBox.indexOfValue(data.mode ?? "between"));
            fromField.text = data.from ?? "";
            toField.text = data.to ?? "";
            sizeModeBox.currentIndex = Math.max(0, sizeModeBox.indexOfValue(data.operator ?? ">="));
            sizeBox.value = data.amount ?? 5;
            sizeUnitBox.currentIndex = Math.max(0, sizeUnitBox.indexOfValue(data.unit ?? "M"));
            open();
        }

        // Also reached with Enter from the text fields.
        function submit(): void {
            if (!canSubmit) {
                return;
            }
            let filter;
            switch (type) {
            case "words":
                filter = root.wordsFilter(textField.text);
                break;
            case "name":
                filter = root.nameFilter(textField.text);
                break;
            case "content":
                filter = root.contentFilter(textField.text, matchBox.currentValue);
                break;
            case "notes":
                filter = root.notesFilter(textField.text);
                break;
            case "folder":
                filter = root.folderFilter(textField.text);
                break;
            case "extension":
                filter = root.extensionFilter(textField.text);
                break;
            case "size":
                filter = root.sizeFilter(sizeModeBox.currentValue, sizeBox.value, sizeUnitBox.currentValue);
                break;
            default:
                filter = root.dateFilter(type, dateModeBox.currentValue, fromDate, toDate);
            }
            if (position >= 0 && position < root.filters.length) {
                // Changed in place: the chip stays where it is.
                const list = Array.from(root.filters);
                list[position] = filter;
                root.filters = list;
            } else {
                root.add(filter);
            }
            close();
        }

        title: {
            switch (type) {
            case "words":
                return i18nc("@title:window", "Filter by Search Words");
            case "name":
                return i18nc("@title:window", "Filter by Name");
            case "content":
                return i18nc("@title:window", "Filter by Text in the Document");
            case "notes":
                return i18nc("@title:window", "Filter by Notes and Comments");
            case "folder":
                return i18nc("@title:window", "Filter by Folder");
            case "extension":
                return i18nc("@title:window", "Filter by Extension");
            case "size":
                return i18nc("@title:window", "Filter by Size");
            case "added":
                return i18nc("@title:window", "Filter by Date Added");
            }
            return i18nc("@title:window", "Filter by Date Modified");
        }
        padding: Kirigami.Units.largeSpacing
        preferredWidth: Kirigami.Units.gridUnit * 26
        standardButtons: Kirigami.Dialog.NoButton
        customFooterActions: [
            Kirigami.Action {
                text: editor.position >= 0 ? i18nc("@action:button", "Change Filter") : i18nc("@action:button", "Add Filter")
                icon.name: editor.position >= 0 ? "dialog-ok-apply" : "list-add"
                enabled: editor.canSubmit
                onTriggered: editor.submit()
            },
            Kirigami.Action {
                text: i18nc("@action:button", "Cancel")
                icon.name: "dialog-cancel"
                onTriggered: editor.close()
            }
        ]
        onOpened: {
            if (isText) {
                textField.forceActiveFocus();
                textField.selectAll();
            } else if (isDate) {
                fromField.forceActiveFocus();
            } else {
                sizeBox.forceActiveFocus();
            }
        }

        ColumnLayout {
            spacing: Kirigami.Units.largeSpacing

            QQC2.TextField {
                id: textField

                Layout.fillWidth: true
                Layout.preferredWidth: Kirigami.Units.gridUnit * 22
                visible: editor.isText
                placeholderText: {
                    switch (editor.type) {
                    case "name":
                        return i18nc("@info:placeholder examples of file name patterns", "report   *.pdf   scan_??.png");
                    case "folder":
                        return i18nc("@info:placeholder example of part of a folder path", "Invoices/2026");
                    case "extension":
                        return i18nc("@info:placeholder file name extensions", "md, txt");
                    }
                    return "";
                }
                onAccepted: editor.submit()
            }
            QQC2.ComboBox {
                id: matchBox

                Layout.fillWidth: true
                visible: editor.type === "content"
                textRole: "label"
                valueRole: "value"
                model: [
                    { value: "all", label: i18nc("@item:inlistbox how to match the text", "All of the words") },
                    { value: "any", label: i18nc("@item:inlistbox how to match the text", "Any of the words") },
                    { value: "phrase", label: i18nc("@item:inlistbox how to match the text", "The exact phrase") }
                ]
            }

            RowLayout {
                Layout.fillWidth: true
                visible: editor.isDate
                spacing: Kirigami.Units.smallSpacing

                QQC2.ComboBox {
                    id: dateModeBox

                    textRole: "label"
                    valueRole: "value"
                    model: [
                        { value: "between", label: i18nc("@item:inlistbox when; followed by two dates", "From") },
                        { value: "after", label: i18nc("@item:inlistbox when; followed by a date", "On or after") },
                        { value: "before", label: i18nc("@item:inlistbox when; followed by a date", "Before") }
                    ]
                }
                QQC2.TextField {
                    id: fromField

                    Layout.fillWidth: true
                    placeholderText: Qt.formatDate(new Date(), "yyyy-MM-dd")
                    // A date that cannot be read cannot be filtered by; say so.
                    color: text.trim().length > 0 && editor.fromDate.length === 0 ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.textColor
                    onAccepted: editor.submit()
                }
                QQC2.Label {
                    visible: dateModeBox.currentValue === "between"
                    text: i18nc("@label between two dates", "to")
                }
                QQC2.TextField {
                    id: toField

                    Layout.fillWidth: true
                    visible: dateModeBox.currentValue === "between"
                    placeholderText: Qt.formatDate(new Date(), "yyyy-MM-dd")
                    color: text.trim().length > 0 && editor.toDate.length === 0 ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.textColor
                    onAccepted: editor.submit()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                visible: editor.type === "size"
                spacing: Kirigami.Units.smallSpacing

                QQC2.ComboBox {
                    id: sizeModeBox

                    textRole: "label"
                    valueRole: "value"
                    model: [
                        { value: ">=", label: i18nc("@item:inlistbox file size; followed by a size", "At least") },
                        { value: "<=", label: i18nc("@item:inlistbox file size; followed by a size", "At most") }
                    ]
                }
                QQC2.SpinBox {
                    id: sizeBox

                    from: 0
                    to: 999999
                    editable: true
                }
                QQC2.ComboBox {
                    id: sizeUnitBox

                    textRole: "label"
                    valueRole: "value"
                    model: [
                        { value: "K", label: i18nc("@item:inlistbox unit of file size", "KiB") },
                        { value: "M", label: i18nc("@item:inlistbox unit of file size", "MiB") },
                        { value: "G", label: i18nc("@item:inlistbox unit of file size", "GiB") }
                    ]
                }
                Item {
                    Layout.fillWidth: true
                }
            }

            QQC2.Label {
                Layout.fillWidth: true
                Layout.preferredWidth: Kirigami.Units.gridUnit * 22
                wrapMode: Text.WordWrap
                font: Kirigami.Theme.smallFont
                color: Kirigami.Theme.disabledTextColor
                text: {
                    switch (editor.type) {
                    case "name":
                        return i18nc("@info", "Part of the name, or a pattern with * for any characters and ? for one. Of several, any may match.");
                    case "content":
                        return i18nc("@info", "Text is found by whole words and by their beginnings. It includes text recognized in pictures and scans.");
                    case "notes":
                        return i18nc("@info", "Looks in the comment and the notes you attached to documents.");
                    case "folder":
                        return i18nc("@info", "Part of the path of the folder the file is in; folders below it are included.");
                    case "extension":
                        return i18nc("@info", "One or several, separated by commas; any of them may match.");
                    case "size":
                        return "";
                    }
                    return i18nc("@info", "Dates as %1 or as %2.", Qt.formatDate(new Date(), "yyyy-MM-dd"), new Date().toLocaleDateString(Qt.locale(), Locale.ShortFormat));
                }
                visible: text.length > 0
            }
        }
    }
}
