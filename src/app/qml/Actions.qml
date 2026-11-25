// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import org.kde.kirigami as Kirigami

// One Kirigami action for each action the application defines in C++. Toolbar and menus all use
// these same objects, and they carry the keyboard shortcuts, so each must exist exactly once.
Item {
    id: root

    visible: false

    component Wrapped: Kirigami.Action {
        required property string name
        fromQAction: App.action(name)
    }

    // Adding and creating
    readonly property Kirigami.Action addFolder: Wrapped { name: "add_folder" }
    readonly property Kirigami.Action addRemote: Wrapped { name: "add_remote" }
    readonly property Kirigami.Action newText: Wrapped { name: "new_text" }
    readonly property Kirigami.Action newMarkdown: Wrapped { name: "new_markdown" }
    readonly property Kirigami.Action newRichText: Wrapped { name: "new_richtext" }
    readonly property Kirigami.Action newFromClipboard: Wrapped { name: "new_clipboard" }
    readonly property Kirigami.Action copyAddress: Wrapped { name: "doc_copy_address" }
    readonly property Kirigami.Action rescan: Wrapped { name: "rescan" }
    readonly property Kirigami.Action databases: Wrapped { name: "db_manage" }
    readonly property Kirigami.Action addDatabase: Wrapped { name: "db_add" }
    readonly property Kirigami.Action newDatabase: Wrapped { name: "db_new" }
    readonly property Kirigami.Action renameDatabase: Wrapped { name: "db_rename" }

    // Documents
    readonly property Kirigami.Action open: Wrapped { name: "doc_open" }
    readonly property Kirigami.Action openWith: Wrapped { name: "doc_open_with" }
    readonly property Kirigami.Action showInFileManager: Wrapped { name: "doc_show" }
    readonly property Kirigami.Action copyLocation: Wrapped { name: "doc_copy_location" }
    readonly property Kirigami.Action rename: Wrapped { name: "doc_rename" }
    readonly property Kirigami.Action tag: Wrapped { name: "doc_tag" }
    readonly property Kirigami.Action group: Wrapped { name: "doc_group" }
    readonly property Kirigami.Action note: Wrapped { name: "doc_note" }
    readonly property Kirigami.Action links: Wrapped { name: "doc_links" }
    readonly property Kirigami.Action linkSelected: Wrapped { name: "doc_link_selected" }
    readonly property Kirigami.Action edit: Wrapped { name: "doc_edit" }
    readonly property Kirigami.Action clearDrawing: Wrapped { name: "doc_clear_drawing" }
    readonly property Kirigami.Action flag: Wrapped { name: "doc_flag" }
    readonly property Kirigami.Action fromInbox: Wrapped { name: "doc_from_inbox" }
    readonly property Kirigami.Action toInbox: Wrapped { name: "doc_to_inbox" }
    readonly property Kirigami.Action fromGroup: Wrapped { name: "doc_from_group" }
    readonly property Kirigami.Action ocr: Wrapped { name: "doc_ocr" }
    readonly property Kirigami.Action reindex: Wrapped { name: "doc_reindex" }
    readonly property Kirigami.Action revert: Wrapped { name: "doc_revert" }
    readonly property Kirigami.Action trash: Wrapped { name: "doc_trash" }
    readonly property Kirigami.Action restore: Wrapped { name: "doc_restore" }
    readonly property Kirigami.Action emptyTrash: Wrapped { name: "empty_trash" }
    readonly property Kirigami.Action selectAll: Wrapped { name: "select_all" }

    // Organizing
    readonly property Kirigami.Action newGroup: Wrapped { name: "new_group" }
    readonly property Kirigami.Action newTag: Wrapped { name: "new_tag" }
    readonly property Kirigami.Action saveSearch: Wrapped {
        name: "save_search"
        // In the toolbar by its icon alone, like the others that belong to the search field
        // and the panels.
        displayHint: Kirigami.DisplayHint.IconOnly
    }
    readonly property Kirigami.Action newRule: Wrapped { name: "new_rule" }
    readonly property Kirigami.Action editSidebarItem: Wrapped { name: "edit_sidebar_item" }
    readonly property Kirigami.Action deleteSidebarItem: Wrapped { name: "delete_sidebar_item" }
    readonly property Kirigami.Action applyRule: Wrapped { name: "apply_rule" }
    readonly property Kirigami.Action goRules: Wrapped { name: "go_rules" }
    readonly property Kirigami.Action automationDelete: Wrapped { name: "automation_delete" }
    readonly property Kirigami.Action newTodo: Wrapped { name: "new_todo" }
    readonly property Kirigami.Action docTodo: Wrapped { name: "doc_todo" }
    readonly property Kirigami.Action clearDoneTodos: Wrapped { name: "todos_clear_done" }

    // Scripts
    readonly property Kirigami.Action scripts: Wrapped { name: "scripts_manage" }
    readonly property Kirigami.Action newScript: Wrapped { name: "new_script" }
    readonly property Kirigami.Action runScript: Wrapped { name: "doc_run_script" }
    readonly property Kirigami.Action scriptOutput: Wrapped { name: "scripts_output" }
    readonly property Kirigami.Action stopScripts: Wrapped { name: "scripts_stop" }

    // Going places
    readonly property Kirigami.Action goInbox: Wrapped { name: "go_inbox" }
    readonly property Kirigami.Action goAll: Wrapped { name: "go_all" }
    readonly property Kirigami.Action goFlagged: Wrapped { name: "go_flagged" }
    readonly property Kirigami.Action goTrash: Wrapped { name: "go_trash" }
    readonly property Kirigami.Action focusSearch: Wrapped { name: "focus_search" }
    readonly property Kirigami.Action advancedSearch: Wrapped {
        name: "search_advanced"
        displayHint: Kirigami.DisplayHint.IconOnly
        tooltip: i18nc("@info:tooltip", "Filters: narrow the list down by name, content, type, date, size, tags and more")
    }

    // View
    readonly property Kirigami.Action leftSidebar: Wrapped { name: "view_left_sidebar" }
    readonly property Kirigami.Action rightSidebar: Wrapped { name: "view_right_sidebar" }
    readonly property Kirigami.Action preview: Wrapped {
        name: "view_preview"
        displayHint: Kirigami.DisplayHint.IconOnly
    }
    readonly property Kirigami.Action previewBeside: Wrapped {
        name: "view_preview_beside"
        displayHint: Kirigami.DisplayHint.IconOnly
    }
    readonly property Kirigami.Action showInfo: Wrapped { name: "view_info" }
    readonly property Kirigami.Action showTodos: Wrapped { name: "view_todos" }
    readonly property Kirigami.Action find: Wrapped { name: "doc_find" }
    readonly property Kirigami.Action findNext: Wrapped { name: "find_next" }
    readonly property Kirigami.Action findPrevious: Wrapped { name: "find_previous" }
    readonly property Kirigami.Action activity: Wrapped { name: "view_activity" }
    readonly property Kirigami.Action listColumns: Wrapped { name: "view_columns" }
    readonly property Kirigami.Action zoomIn: Wrapped { name: "view_zoom_in" }
    readonly property Kirigami.Action zoomOut: Wrapped { name: "view_zoom_out" }
    readonly property Kirigami.Action zoomReset: Wrapped { name: "view_zoom_reset" }
    readonly property Kirigami.Action rawView: Wrapped { name: "view_raw" }

    // Application
    readonly property Kirigami.Action configure: Wrapped { name: "options_configure" }
    readonly property Kirigami.Action configureToolbar: Wrapped { name: "options_configure_toolbars" }
    readonly property Kirigami.Action configureShortcuts: Wrapped { name: "options_configure_keybinding" }
    readonly property Kirigami.Action commandBar: Wrapped { name: "open_kcommand_bar" }
    readonly property Kirigami.Action manual: Wrapped { name: "help_manual" }
    readonly property Kirigami.Action about: Wrapped { name: "open_about_page" }
    readonly property Kirigami.Action quit: Wrapped { name: "file_quit" }

    /** The action with the given name as the application knows it, e.g. "doc_tag"; null if none. */
    function named(name: string): Kirigami.Action {
        if (Object.keys(byName).length === 0) {
            for (const key in root) {
                const candidate = root[key];
                if (candidate instanceof Kirigami.Action && typeof candidate.name === "string") {
                    byName[candidate.name] = candidate;
                }
            }
        }
        return byName[name] ?? null;
    }
    property var byName: ({})
}
