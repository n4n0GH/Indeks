// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The bar across the top of the preview. It is always there, so that what can be done with
// the document shown is always looked for in the same place: on the left a word about the
// document, on the right editing and taking a note, what its viewer offers, then the switch
// to the file as it is, then the zoom.
//
// A viewer takes part by having, under these names, whatever of this applies to it:
//   status         what to say on the left
//   viewActions    its own actions, a list of Kirigami.Action
//   zoomable       whether it can show its document larger and smaller; if so also
//                  zoomFactor (1 is 100 %), zoomIsDefault, zoomResetText, and the functions
//                  zoomIn(), zoomOut(), zoomReset()
//   stickyAvailable  whether notes can be stuck to a place on what it shows; if so also the
//                  property placingNote, which has the next click put one down, and drawing
//   rawAvailable   whether it lays the document out for reading and can show it as it is in
//                  the file instead; if so also the property raw and rawText, what to call that
QQC2.ToolBar {
    id: root

    required property Actions actions
    /** The viewer that shows the document; none while there is nothing to show. */
    property Item view: null

    readonly property bool zoomable: view?.zoomable ?? false
    readonly property var viewActions: view?.viewActions ?? []
    readonly property bool stickyAvailable: view?.stickyAvailable ?? false
    readonly property bool placingNote: stickyAvailable && (view?.placingNote ?? false)

    position: QQC2.ToolBar.Header

    contentItem: RowLayout {
        spacing: Kirigami.Units.mediumSpacing

        QQC2.Label {
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.maximumWidth: Math.max(0, Math.round(root.width * 0.55))
            // The bar is as high with nothing in it as with buttons.
            Layout.minimumHeight: Kirigami.Units.iconSizes.smallMedium + Kirigami.Units.smallSpacing * 2
            verticalAlignment: Text.AlignVCenter
            // While a note waits to be put down, what to do about it.
            text: root.placingNote ? i18nc("@info:status", "Click where the note is to go") : root.view?.status ?? ""
            textFormat: Text.PlainText
            elide: Text.ElideRight
            color: Kirigami.Theme.disabledTextColor
        }

        Kirigami.ActionToolBar {
            Layout.fillWidth: true
            alignment: Qt.AlignRight
            // What does not fit goes into a menu at the end; the preview can be narrow.
            // Editing and noting are done here, in the preview, so this is where they are
            // asked for. They are the application's own actions, shortcuts and all.
            // Where notes can be stuck to a place, the one button for notes asks which kind.
            actions: [root.actions.edit, root.stickyAvailable ? noteMenu : root.actions.note].concat(root.viewActions, [rawAction, divider, zoomOutAction, zoomLevelAction, zoomInAction])
        }
    }

    Kirigami.Action {
        id: noteMenu

        text: i18nc("@action:intoolbar opens a menu of the kinds of notes", "Add Note")
        icon.name: "note"
        enabled: root.actions.note.enabled

        Kirigami.Action {
            // The application's own action does the work; it is not handed over as it is,
            // because it has its place in the menus and keeps its shortcut there.
            text: i18nc("@action:inmenu a note on the document as a whole", "Add Note…")
            icon.name: "note"
            onTriggered: root.actions.note.trigger()
        }
        Kirigami.Action {
            // Chosen once it waits for the click that puts the note down; chosen again, it
            // lets that be.
            text: root.placingNote ? i18nc("@action:inmenu", "Cancel Adding Sticky")
                                   : i18nc("@action:inmenu a note that is stuck to a place on a page or picture", "Add Sticky")
            icon.name: root.placingNote ? "dialog-cancel" : "view-pim-notes"
            tooltip: i18nc("@info:tooltip", "Then click where the note is to go")
            enabled: !(root.view?.drawing ?? false)
            onTriggered: root.view.placingNote = !root.view.placingNote
        }
    }
    Kirigami.Action {
        id: rawAction

        fromQAction: App.action("view_raw")
        text: root.view?.rawText ?? i18nc("@action:intoolbar", "Show Unformatted")
        visible: root.view?.rawAvailable ?? false
    }
    Kirigami.Action {
        id: divider

        separator: true
        // Editing and noting are always before it.
        visible: true
    }
    Kirigami.Action {
        id: zoomOutAction

        fromQAction: App.action("view_zoom_out")
        displayHint: Kirigami.DisplayHint.IconOnly | Kirigami.DisplayHint.KeepVisible
    }
    Kirigami.Action {
        id: zoomLevelAction

        // Says how large things are shown; a click goes back to how they are shown at first.
        text: root.zoomable ? i18nc("@action:intoolbar the zoom level in percent", "%1%", Math.round(root.view.zoomFactor * 100))
                            : i18nc("@action:intoolbar the zoom level in percent", "%1%", 100)
        tooltip: root.zoomable ? root.view.zoomResetText : ""
        enabled: root.zoomable && !root.view.zoomIsDefault
        displayHint: Kirigami.DisplayHint.KeepVisible
        onTriggered: root.actions.zoomReset.trigger()
    }
    Kirigami.Action {
        id: zoomInAction

        fromQAction: App.action("view_zoom_in")
        displayHint: Kirigami.DisplayHint.IconOnly | Kirigami.DisplayHint.KeepVisible
    }
}
