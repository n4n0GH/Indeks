// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The file browser in the middle of the window: the documents of the place selected in the
// sidebar, narrowed down by the search field.
QQC2.Pane {
    id: root

    required property Actions actions

    // ------------------------------------------------------------------------ list or grid

    /** Whether the documents are shown as thumbnails in a grid rather than as rows of a list. */
    readonly property bool grid: Config.documentsAsGrid
    /**
     * How high the bar above the documents is to be, so that it is as high as the bars beside
     * it; what the bar would take by itself is naturalBarHeight.
     */
    property real barHeight: 0
    readonly property real naturalBarHeight: viewBar.implicitHeight

    // The thumbnails are asked for in few sizes, so that sliding the size does not have
    // every one of them made again; see ThumbnailProvider.
    readonly property int thumbnailPixels: {
        const wanted = Config.gridIconSize * Screen.devicePixelRatio;
        return wanted <= 128 ? 128 : wanted <= 256 ? 256 : 512;
    }

    // One of the application's commands as an entry of a menu of the bar, under a shorter
    // name: there it is plain what is sorted.
    component Choice: Kirigami.Action {
        required property string name
        readonly property var command: App.action(name)

        checkable: true
        checked: command?.checked ?? false
        enabled: command?.enabled ?? false
        onTriggered: {
            command.trigger();
            // Which is chosen is the command's to say, not the entry's.
            checked = Qt.binding(() => command?.checked ?? false);
        }
    }

    // ------------------------------------------------------------------------ columns

    // What the list can show of a document beside its name. `width` is what a column starts
    // out with, in units of the grid; `sort` what the
    // list is sorted by with a click on its title, nothing where there is no order to it.
    // Which of them are shown, and in which order, is the user's choice for every view by
    // itself: the inbox may show other columns than a group or a saved search. A view the
    // user has not chosen for shows the standard columns (Config.listColumns). How wide each
    // column is (Config.listColumnWidths) and how tall the rows are (Config.listRowSize) is
    // the same everywhere.
    readonly property var columns: [
        { name: "kind", title: i18nc("@title:column", "Type"), width: 9, sort: "kind", right: false },
        { name: "tags", title: i18nc("@title:column", "Tags"), width: 9, sort: "", right: false },
        { name: "groups", title: i18nc("@title:column", "Groups"), width: 9, sort: "", right: false },
        { name: "sender", title: i18nc("@title:column who an e-mail is from", "From"), width: 11, sort: "sender", right: false },
        { name: "location", title: i18nc("@title:column the folder a file is in", "Location"), width: 12, sort: "location", right: false },
        { name: "comment", title: i18nc("@title:column", "Comment"), width: 12, sort: "comment", right: false },
        { name: "pages", title: i18nc("@title:column", "Pages"), width: 4, sort: "pages", right: true },
        { name: "modified", title: i18nc("@title:column", "Modified"), width: 8, sort: "modified", right: false },
        { name: "added", title: i18nc("@title:column", "Added"), width: 8, sort: "added", right: false },
        { name: "size", title: i18nc("@title:column", "Size"), width: 5, sort: "size", right: true }
    ]
    readonly property list<string> defaultColumns: ["kind", "tags", "modified", "added", "size"]
    // Where the list is too narrow for all the chosen columns and a name beside them, columns
    // give way in this order, to come back when there is room again.
    readonly property list<string> givingWay: ["comment", "groups", "pages", "location", "sender", "added", "tags", "kind", "size", "modified"]
    readonly property real nameWidth: Kirigami.Units.gridUnit * 10

    readonly property var storedWidths: {
        const widths = {};
        for (const entry of Config.listColumnWidths) {
            const parts = entry.split("=");
            if (parts.length === 2 && Number(parts[1]) > 0) {
                widths[parts[0]] = Number(parts[1]);
            }
        }
        return widths;
    }
    // The width of the column that is being dragged, until it is let go of.
    property string draggedColumn
    property real draggedUnits: 0

    function columnUnits(name: string): real {
        if (name === draggedColumn) {
            return draggedUnits;
        }
        return storedWidths[name] ?? columns.find(column => column.name === name)?.width ?? 8;
    }

    // As wide as the user made the column, or as it starts out.
    function wantedWidth(name: string): real {
        return columnUnits(name) * Kirigami.Units.gridUnit;
    }

    // As wide as it is shown: with more columns than the list has room for, all of them
    // give a little.
    function columnWidth(name: string): real {
        return Math.round(wantedWidth(name) * fit.scale);
    }

    function storeColumnWidth(name: string, units: real): void {
        const widths = Object.assign({}, storedWidths);
        widths[name] = Math.round(units * 100) / 100;
        Config.listColumnWidths = Object.keys(widths).map(key => key + "=" + widths[key]);
        Config.save();
    }

    // The view the list shows: what is selected in the sidebar. Its columns are kept with
    // the index (see Library::viewColumns), under the key the sidebar has for it.
    readonly property string viewKey: App.sidebar.currentKey
    // Counted up whenever the columns of some view change, for what is read from the library.
    property int columnsRevision: 0
    /** Whether this view has columns of its own, or shows the standard ones. */
    readonly property bool ownColumns: {
        columnsRevision;
        return App.library.hasViewColumns(viewKey);
    }
    /** The names of the columns this view shows, in the order the user put them in. */
    readonly property list<string> viewColumns: {
        columnsRevision;
        return App.library.hasViewColumns(viewKey) ? App.library.viewColumns(viewKey) : Config.listColumns;
    }

    Connections {
        target: App.library
        function onViewColumnsChanged(): void {
            ++root.columnsRevision;
        }
    }

    // A change is this view's alone, and from then on the view has columns of its own.
    function storeColumns(names: var): void {
        App.library.setViewColumns(viewKey, names);
    }
    // The view goes back to the standard columns.
    function useStandardColumns(): void {
        App.library.clearViewColumns(viewKey);
    }
    // What this view shows becomes the standard, and every view shows it.
    function useColumnsEverywhere(): void {
        Config.listColumns = viewColumns;
        Config.save();
        App.library.clearAllViewColumns();
    }

    readonly property var chosenColumns: viewColumns.map(name => columns.find(column => column.name === name)).filter(column => column !== undefined)

    function setColumnShown(name: string, shown: bool): void {
        // A column that is added goes to the end; from there it is dragged to its place.
        const chosen = viewColumns.filter(column => column !== name);
        if (shown) {
            chosen.push(name);
        }
        storeColumns(chosen);
    }

    // Puts a column before another one in the order; at the end if @p before is empty.
    function moveColumn(name: string, before: string): void {
        if (name === before) {
            return;
        }
        const chosen = viewColumns.filter(column => column !== name);
        const index = before.length > 0 ? chosen.indexOf(before) : -1;
        chosen.splice(index >= 0 ? index : chosen.length, 0, name);
        storeColumns(chosen);
    }

    // A column that is being dragged to another place among the columns: which one, how far
    // it has come, the place it had among the shown columns and the place it would have if it
    // were let go of now. While that goes on, the columns between the two places stand aside,
    // title and rows alike, so that the list always looks the way it is about to be.
    property string movingColumn
    property real movingOffset: 0
    property int movingFrom: -1
    property int movingTo: -1

    // How far the column at @p index of the shown ones stands from its place meanwhile.
    function columnShift(index: int, name: string): real {
        if (movingColumn.length === 0) {
            return 0;
        }
        if (name === movingColumn) {
            return movingOffset;
        }
        const step = columnWidth(movingColumn) + Kirigami.Units.smallSpacing;
        if (movingFrom < index && index <= movingTo) {
            return -step;
        }
        if (movingTo <= index && index < movingFrom) {
            return step;
        }
        return 0;
    }

    // Everything as it was at first, in every view.
    function resetColumns(): void {
        App.library.clearAllViewColumns();
        Config.listColumns = defaultColumns;
        Config.listColumnWidths = [];
        Config.listRowSize = 1;
        Config.save();
    }

    function showColumnMenu(): void {
        columnMenu.popup(headerRow, headerRow.width - columnMenu.width, headerRow.height);
    }

    // The chosen columns as they fit beside the name. Where there are more than there is
    // room for, they are first made narrower, all alike, down to a little over half their
    // width; only then do columns give way, to come back when there is room again.
    readonly property real tightest: 0.55
    readonly property var fit: {
        const shown = Array.from(chosenColumns);
        const room = Math.max(0, width - nameWidth - Kirigami.Units.largeSpacing * 2 - Kirigami.Units.gridUnit);
        const scale = () => {
            const spacing = shown.length * Kirigami.Units.smallSpacing;
            const wanted = shown.reduce((sum, column) => sum + wantedWidth(column.name), 0);
            return wanted > 0 ? Math.min(1, Math.max(0, room - spacing) / wanted) : 1;
        };
        for (const name of givingWay) {
            if (scale() >= tightest) {
                break;
            }
            const index = shown.findIndex(column => column.name === name);
            if (index >= 0) {
                shown.splice(index, 1);
            }
        }
        return { columns: shown, scale: Math.max(tightest, scale()) };
    }
    readonly property var shownColumns: fit.columns
    /** Chosen columns that are left out for want of room. */
    readonly property int hiddenColumns: chosenColumns.length - shownColumns.length

    // How tall the rows are: how much room there is above and below, and how large the icons.
    readonly property real rowPadding: Config.listRowSize === 0 ? 1 : Config.listRowSize === 2 ? Kirigami.Units.largeSpacing : Kirigami.Units.smallSpacing
    readonly property real rowIcon: Config.listRowSize === 0 ? Kirigami.Units.iconSizes.small
                                  : Config.listRowSize === 2 ? Kirigami.Units.iconSizes.medium : Kirigami.Units.iconSizes.smallMedium

    // The list is being read, or a search through the text of all documents is under way,
    // and has been for a moment. Most are over before anybody could look; saying so each
    // time would only flicker.
    property bool searching: false
    readonly property bool loading: App.documents.loading

    onLoadingChanged: {
        if (loading) {
            searchingDelay.restart();
        } else {
            searchingDelay.stop();
            searching = false;
        }
    }

    Timer {
        id: searchingDelay

        interval: Kirigami.Units.humanMoment / 8
        onTriggered: root.searching = root.loading
    }

    padding: 0
    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false

    function selectRow(row: int, modifiers: int): void {
        const mode = modifiers & Qt.ShiftModifier ? DocumentModel.Range : modifiers & Qt.ControlModifier ? DocumentModel.Toggle : DocumentModel.Single;
        App.documents.select(row, mode);
    }

    // The arrow keys to the side open and close what opens like a folder: a mailbox. From
    // one of its messages, the left arrow goes to the mailbox.
    function expandCurrent(open: bool): void {
        const row = App.documents.currentRow;
        if (row < 0) {
            return;
        }
        const id = App.documents.idAt(row);
        const parent = App.documents.parentRow(row);
        if (!open && parent >= 0) {
            App.documents.select(parent, DocumentModel.Single);
        } else {
            App.documents.setExpanded(id, open);
        }
    }

    function moveCurrent(delta: int, modifiers: int): void {
        if (App.documents.count === 0) {
            return;
        }
        const current = App.documents.currentRow;
        const row = current < 0 ? 0 : Math.max(0, Math.min(App.documents.count - 1, current + delta));
        App.documents.select(row, modifiers & Qt.ShiftModifier ? DocumentModel.Range : DocumentModel.Single);
    }

    // A plain button rather than a ToolButton: the title has to line up with the column below it.
    component ColumnHeader: QQC2.AbstractButton {
        id: header

        required property string column
        property int alignment: Qt.AlignLeft
        readonly property bool sorted: column.length > 0 && App.documents.sortColumn === column
        /**
         * How far the room of the title reaches beyond the title itself, to the left and to
         * the right: up to the line that parts it from the next title, or to the edge of the
         * list for the first and the last.
         */
        property real reachLeft: 0
        property real reachRight: 0

        enabled: column.length > 0
        padding: Kirigami.Units.smallSpacing
        hoverEnabled: true
        focusPolicy: Qt.TabFocus

        // What marks the title under the pointer fills all of its room, square, as the
        // rows below fill theirs.
        leftInset: -reachLeft
        rightInset: -reachRight
        background: Rectangle {
            color: header.down ? Kirigami.Theme.highlightColor : Kirigami.Theme.hoverColor
            opacity: header.down ? 0.3 : header.hovered || header.visualFocus ? 0.2 : 0
        }

        contentItem: RowLayout {
            spacing: Kirigami.Units.smallSpacing
            layoutDirection: header.alignment === Qt.AlignRight ? Qt.RightToLeft : Qt.LeftToRight

            QQC2.Label {
                text: header.text
                font: Kirigami.Theme.smallFont
                elide: Text.ElideRight
                color: header.sorted ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
            }
            Kirigami.Icon {
                visible: header.sorted
                source: App.documents.sortDescending ? "arrow-down" : "arrow-up"
                implicitWidth: Kirigami.Units.iconSizes.small
                implicitHeight: implicitWidth
            }
            Item {
                Layout.fillWidth: true
            }
        }

        Accessible.name: sorted
            ? (App.documents.sortDescending ? i18nc("@info accessible name of a column header", "%1, sorted descending", text)
                                            : i18nc("@info accessible name of a column header", "%1, sorted ascending", text))
            : text

        onClicked: {
            if (App.documents.sortColumn === column) {
                App.documents.sortDescending = !App.documents.sortDescending;
            } else {
                App.documents.sortColumn = column;
                App.documents.sortDescending = column !== "name" && column !== "kind";
            }
        }
    }

    contentItem: Item {
        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // Across the top, like the bar of the preview: how many documents there are, what
            // they are sorted by, how large they are shown, and whether as a list or a grid.
            QQC2.ToolBar {
                id: viewBar

                Layout.fillWidth: true
                Layout.preferredHeight: root.barHeight > 0 ? root.barHeight : implicitHeight
                position: QQC2.ToolBar.Header

                contentItem: RowLayout {
                    spacing: Kirigami.Units.mediumSpacing

                    QQC2.Label {
                        Layout.leftMargin: Kirigami.Units.smallSpacing
                        Layout.maximumWidth: Math.max(0, Math.round(viewBar.width * 0.4))
                        Layout.minimumHeight: Kirigami.Units.iconSizes.smallMedium + Kirigami.Units.smallSpacing * 2
                        verticalAlignment: Text.AlignVCenter
                        text: App.documents.selectionCount > 1
                            ? i18ncp("@info:status how many of the listed documents are selected", "%2 of %1 selected", "%2 of %1 selected", App.documents.count, App.documents.selectionCount)
                            : i18ncp("@info:status", "%1 document", "%1 documents", App.documents.count)
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                        color: Kirigami.Theme.disabledTextColor
                    }

                    Kirigami.ActionToolBar {
                        Layout.fillWidth: true
                        alignment: Qt.AlignRight
                        actions: [sortMenu, asList, asGrid]

                        Kirigami.Action {
                            id: sortMenu

                            text: i18nc("@action:intoolbar", "Sort By")
                            icon.name: "view-sort"
                            tooltip: i18nc("@info:tooltip", "What the documents are sorted by")

                            Choice {
                                name: "sort_by_name"
                                text: i18nc("@item:inmenu sort by", "Name")
                            }
                            Choice {
                                name: "sort_by_kind"
                                text: i18nc("@item:inmenu sort by", "Type")
                            }
                            Choice {
                                name: "sort_by_modified"
                                text: i18nc("@item:inmenu sort by", "Date Modified")
                            }
                            Choice {
                                name: "sort_by_added"
                                text: i18nc("@item:inmenu sort by", "Date Added")
                            }
                            Choice {
                                name: "sort_by_size"
                                text: i18nc("@item:inmenu sort by", "Size")
                            }
                            Choice {
                                name: "sort_by_sender"
                                text: i18nc("@item:inmenu sort by who an e-mail is from", "From")
                            }
                            Choice {
                                name: "sort_by_location"
                                text: i18nc("@item:inmenu sort by", "Location")
                            }
                            Choice {
                                name: "sort_by_pages"
                                text: i18nc("@item:inmenu sort by", "Pages")
                            }
                            Choice {
                                name: "sort_by_comment"
                                text: i18nc("@item:inmenu sort by", "Comment")
                            }
                            Kirigami.Action {
                                separator: true
                            }
                            Choice {
                                name: "sort_descending"
                                text: i18nc("@item:inmenu sort order", "Descending")
                            }
                        }
                        Choice {
                            id: asList

                            name: "view_as_list"
                            text: i18nc("@action:intoolbar show the documents as a list", "List")
                            icon.name: "view-list-details"
                            tooltip: i18nc("@info:tooltip", "Show the documents as a list with columns")
                            displayHint: Kirigami.DisplayHint.IconOnly
                        }
                        Choice {
                            id: asGrid

                            name: "view_as_grid"
                            text: i18nc("@action:intoolbar show the documents as thumbnails in a grid", "Thumbnails")
                            icon.name: "view-list-icons"
                            tooltip: i18nc("@info:tooltip", "Show the documents as thumbnails in a grid")
                            displayHint: Kirigami.DisplayHint.IconOnly
                        }
                    }

                    // How large: the thumbnails of the grid, the rows of the list. Left out
                    // where the bar is too narrow for it; the commands and Ctrl with the
                    // wheel do the same.
                    QQC2.Slider {
                        id: sizeSlider

                        Layout.preferredWidth: Kirigami.Units.gridUnit * 6
                        Layout.rightMargin: Kirigami.Units.smallSpacing
                        visible: viewBar.width > Kirigami.Units.gridUnit * 26
                        from: root.grid ? 48 : 0
                        to: root.grid ? 256 : 2
                        // The three heights of a row are steps, with a mark each; the
                        // thumbnails go by steps too small to mark.
                        stepSize: root.grid ? 0 : 1
                        snapMode: root.grid ? QQC2.Slider.NoSnap : QQC2.Slider.SnapAlways
                        value: root.grid ? Config.gridIconSize : Config.listRowSize
                        onMoved: {
                            if (root.grid) {
                                Config.gridIconSize = Math.round(value / 16) * 16;
                            } else {
                                Config.listRowSize = Math.round(value);
                            }
                        }
                        onPressedChanged: {
                            if (!pressed) {
                                Config.save();
                            }
                        }

                        Accessible.name: root.grid ? i18nc("@label:slider", "Size of the thumbnails") : i18nc("@label:slider", "Height of the rows")

                        QQC2.ToolTip.text: Accessible.name
                        QQC2.ToolTip.visible: hovered && !pressed
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    }
                }
            }

            RowLayout {
                id: headerRow

                visible: !root.grid
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.largeSpacing - Kirigami.Units.smallSpacing
                Layout.rightMargin: Kirigami.Units.largeSpacing - Kirigami.Units.smallSpacing + (scrollView.QQC2.ScrollBar.vertical.visible ? scrollView.QQC2.ScrollBar.vertical.width : 0)
                spacing: Kirigami.Units.smallSpacing

                // Which columns there are, and how tall the rows: a right-click anywhere on the titles.
                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: eventPoint => columnMenu.popup(headerRow, eventPoint.position.x, headerRow.height)
                }

                ColumnHeader {
                    Layout.fillWidth: true
                    column: "name"
                    text: i18nc("@title:column", "Name")
                    reachLeft: headerRow.Layout.leftMargin
                    // The only title there is reaches the other edge too.
                    reachRight: titles.count > 0 ? headerRow.spacing / 2 : headerRow.Layout.rightMargin
                }
                Repeater {
                    id: titles

                    model: root.shownColumns

                    delegate: Item {
                        id: title

                        required property int index
                        required property var modelData
                        readonly property bool moving: root.movingColumn === modelData.name

                        Layout.preferredWidth: root.columnWidth(modelData.name)
                        implicitHeight: titleButton.implicitHeight
                        z: moving ? 2 : 0
                        opacity: moving ? 0.75 : 1

                        // Carried along while it is dragged to another place; the others
                        // glide out of its way.
                        transform: Translate {
                            x: root.columnShift(title.index, title.modelData.name)

                            Behavior on x {
                                enabled: root.movingColumn.length > 0 && !title.moving
                                NumberAnimation {
                                    duration: Kirigami.Units.longDuration
                                    easing.type: Easing.OutCubic
                                }
                            }
                        }

                        ColumnHeader {
                            id: titleButton

                            anchors.fill: parent
                            column: title.modelData.sort
                            alignment: title.modelData.right ? Qt.AlignRight : Qt.AlignLeft
                            text: title.modelData.title
                            reachLeft: headerRow.spacing / 2
                            reachRight: title.index === titles.count - 1 ? headerRow.Layout.rightMargin : headerRow.spacing / 2
                        }

                        // Dragging a title sideways moves its column among the others. Its
                        // place is where its middle has come to: before every column whose
                        // own middle lies to the right of it.
                        DragHandler {
                            id: moveDrag

                            target: null
                            yAxis.enabled: false
                            enabled: !widthHover.hovered && !widthDrag.active
                            onActiveChanged: {
                                if (active) {
                                    root.movingOffset = 0;
                                    root.movingFrom = title.index;
                                    root.movingTo = title.index;
                                    root.movingColumn = title.modelData.name;
                                } else if (root.movingColumn === title.modelData.name) {
                                    const name = root.movingColumn;
                                    const others = root.shownColumns.map(column => column.name).filter(column => column !== name);
                                    // Behind the last one shown come the columns that are
                                    // left out for want of room, if there are any.
                                    const before = others[root.movingTo] ?? root.viewColumns.find(column => column !== name && !others.includes(column)) ?? "";
                                    const moved = root.movingTo !== root.movingFrom;
                                    root.movingColumn = "";
                                    root.movingFrom = -1;
                                    root.movingTo = -1;
                                    if (moved) {
                                        root.moveColumn(name, before);
                                    }
                                }
                            }
                            onActiveTranslationChanged: {
                                if (!active) {
                                    return;
                                }
                                // No further than the first and the last column.
                                const first = titles.itemAt(0);
                                const last = titles.itemAt(titles.count - 1);
                                const offset = Math.max(first.x - title.x, Math.min(last.x + last.width - title.x - title.width, activeTranslation.x));
                                const middle = title.x + title.width / 2 + offset;
                                let place = 0;
                                for (let i = 0; i < titles.count; ++i) {
                                    const other = titles.itemAt(i);
                                    if (other !== title && other.x + other.width / 2 < middle) {
                                        ++place;
                                    }
                                }
                                root.movingOffset = offset;
                                root.movingTo = place;
                            }
                        }

                        // The line between this title and the one before it: dragging it
                        // makes the column wider or narrower.
                        Item {
                            x: -Math.round((width + headerRow.spacing) / 2)
                            width: Kirigami.Units.largeSpacing
                            height: parent.height

                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: 1
                                height: parent.height
                                color: Kirigami.Theme.textColor
                                opacity: widthDrag.active ? 0.6 : widthHover.hovered ? 0.35 : 0.12
                            }
                            HoverHandler {
                                id: widthHover

                                cursorShape: Qt.SplitHCursor
                            }
                            DragHandler {
                                id: widthDrag

                                property real startUnits: 0

                                target: null
                                yAxis.enabled: false
                                cursorShape: Qt.SplitHCursor
                                onActiveChanged: {
                                    if (active) {
                                        startUnits = root.columnUnits(title.modelData.name);
                                        root.draggedUnits = startUnits;
                                        root.draggedColumn = title.modelData.name;
                                    } else {
                                        const units = root.draggedUnits;
                                        root.draggedColumn = "";
                                        root.storeColumnWidth(title.modelData.name, units);
                                    }
                                }
                                onActiveTranslationChanged: {
                                    if (active) {
                                        // The column begins at this line, so it grows as the line goes left.
                                        root.draggedUnits = Math.max(3, Math.min(40, startUnits - activeTranslation.x / Kirigami.Units.gridUnit / root.fit.scale));
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Kirigami.Separator {
                visible: !root.grid
                Layout.fillWidth: true
            }

            // The list takes so many rows and no more. Where it stops short it says so, in
            // sight, not at an end nobody scrolls to.
            QQC2.Label {
                Layout.fillWidth: true
                visible: App.documents.truncated
                padding: Kirigami.Units.smallSpacing
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                font: Kirigami.Theme.smallFont
                color: Kirigami.Theme.disabledTextColor
                text: i18nc("@info", "There is more than the list shows. A search or a filter finds the rest.")
                background: Rectangle {
                    color: Kirigami.Theme.alternateBackgroundColor
                }
            }

            QQC2.ScrollView {
                id: scrollView

                visible: !root.grid
                Layout.fillWidth: true
                Layout.fillHeight: true
                QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

                ListView {
                    id: list

                    // Rows are only made while they are looked at.
                    model: root.grid ? null : App.documents
                    clip: true
                    activeFocusOnTab: true
                    keyNavigationEnabled: false
                    reuseItems: true
                    currentIndex: App.documents.currentRow
                    highlightFollowsCurrentItem: false

                    Keys.onUpPressed: event => root.moveCurrent(-1, event.modifiers)
                    Keys.onDownPressed: event => root.moveCurrent(1, event.modifiers)
                    Keys.onRightPressed: root.expandCurrent(true)
                    Keys.onLeftPressed: root.expandCurrent(false)
                    Keys.onPressed: event => {
                        const page = Math.max(1, Math.floor(height / Kirigami.Units.gridUnit / 2));
                        if (event.key === Qt.Key_PageUp) {
                            root.moveCurrent(-page, event.modifiers);
                        } else if (event.key === Qt.Key_PageDown) {
                            root.moveCurrent(page, event.modifiers);
                        } else if (event.key === Qt.Key_Home) {
                            root.moveCurrent(-App.documents.count, event.modifiers);
                        } else if (event.key === Qt.Key_End) {
                            root.moveCurrent(App.documents.count, event.modifiers);
                        } else if (event.key === Qt.Key_Escape) {
                            App.documents.clearSelection();
                        } else {
                            return;
                        }
                        event.accepted = true;
                    }
                    Keys.onReturnPressed: root.actions.open.trigger()
                    Keys.onEnterPressed: root.actions.open.trigger()

                    // Reloads replace the whole list; stay where the user was.
                    property real savedContentY: 0
                    Connections {
                        target: App.documents
                        function onModelAboutToBeReset(): void {
                            list.savedContentY = list.contentY;
                        }
                        function onModelReset(): void {
                            list.contentY = Math.min(list.savedContentY, Math.max(0, list.contentHeight - list.height));
                        }
                        function onCurrentChanged(): void {
                            if (App.documents.currentRow >= 0) {
                                list.positionViewAtIndex(App.documents.currentRow, ListView.Contain);
                            }
                        }
                        function onScopeChanged(): void {
                            list.savedContentY = 0;
                            list.positionViewAtBeginning();
                        }
                    }

                    delegate: QQC2.ItemDelegate {
                        id: row

                        required property int index
                        required property real documentId
                        required property string name
                        required property string iconName
                        required property string typeName
                        required property string sizeText
                        required property string modifiedText
                        required property string addedText
                        required property list<string> tags
                        required property string snippet
                        required property bool selected
                        required property bool missing
                        // No longer belongs into this list, and stays until the list is left.
                        required property bool lingering
                        required property bool offline
                        required property bool flagged
                        required property bool annotated
                        required property int depth
                        required property bool expandable
                        required property bool expanded
                        required property string parentName
                        required property list<string> groups
                        required property string sender
                        required property string location
                        required property string pagesText
                        required property string comment

                        width: ListView.view.width
                        highlighted: selected
                        topPadding: root.rowPadding
                        bottomPadding: root.rowPadding
                        hoverEnabled: true

                        // The whole row, from one edge of the list to the other and square: a
                        // row is a line of a table with columns, and the style's own marking,
                        // rounded and set in from the edges, covers only part of them.
                        background: Rectangle {
                            color: row.highlighted ? Kirigami.Theme.highlightColor
                                : row.down ? Qt.alpha(Kirigami.Theme.highlightColor, 0.35)
                                : row.hovered ? Qt.alpha(Kirigami.Theme.highlightColor, 0.2)
                                : "transparent"
                        }

                        // What the row says in a column.
                        function cell(column: string): string {
                            switch (column) {
                            case "kind":
                                return typeName;
                            case "tags":
                                return tags.join(", ");
                            case "groups":
                                return groups.join(", ");
                            case "sender":
                                return sender;
                            case "location":
                                return location;
                            case "comment":
                                return comment;
                            case "pages":
                                return pagesText;
                            case "modified":
                                return modifiedText;
                            case "added":
                                return addedText;
                            case "size":
                                return sizeText;
                            }
                            return "";
                        }
                        leftPadding: Kirigami.Units.largeSpacing
                        rightPadding: Kirigami.Units.largeSpacing

                        readonly property color textColor: highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                        readonly property color dimColor: highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.disabledTextColor

                        Accessible.name: name
                        Accessible.role: Accessible.ListItem

                        contentItem: ColumnLayout {
                            spacing: 0
                            // Faded: on its way out of this list.
                            opacity: row.lingering ? 0.55 : 1

                            RowLayout {
                                spacing: Kirigami.Units.smallSpacing

                                // The arrow that opens a mailbox like a folder. Every row
                                // keeps room for it while the list has one, and what is
                                // inside stands a step further in.
                                Item {
                                    visible: App.documents.tree
                                    implicitWidth: Kirigami.Units.iconSizes.small + row.depth * (root.rowIcon + Kirigami.Units.smallSpacing)
                                    implicitHeight: Kirigami.Units.iconSizes.small

                                    Kirigami.Icon {
                                        anchors.left: parent.left
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: Kirigami.Units.iconSizes.small
                                        height: width
                                        visible: row.expandable
                                        source: row.expanded ? "go-down-symbolic" : (Application.layoutDirection === Qt.RightToLeft ? "go-previous-symbolic" : "go-next-symbolic")
                                        color: row.textColor
                                        isMask: true
                                        opacity: expanderHover.hovered ? 1 : 0.7

                                        Accessible.role: Accessible.Button
                                        Accessible.name: row.expanded ? i18nc("@action:button", "Close") : i18nc("@action:button", "Open")

                                        HoverHandler {
                                            id: expanderHover
                                        }
                                        TapHandler {
                                            // A larger target than the arrow itself.
                                            margin: Kirigami.Units.smallSpacing
                                            onTapped: App.documents.setExpanded(row.documentId, !row.expanded)
                                        }
                                    }
                                }
                                Kirigami.Icon {
                                    source: row.iconName
                                    opacity: row.missing || row.offline ? 0.5 : 1
                                    implicitWidth: root.rowIcon
                                    implicitHeight: implicitWidth
                                }
                                QQC2.Label {
                                    Layout.fillWidth: true
                                    // A message that is listed away from its mailbox says which it is in.
                                    text: row.parentName.length > 0 ? i18nc("@item a message and the mailbox it is in", "%1 — in %2", row.name, row.parentName) : row.name
                                    textFormat: Text.PlainText
                                    // File names are cut in the middle, to keep their endings; subjects at the end.
                                    elide: row.parentName.length > 0 || row.depth > 0 ? Text.ElideRight : Text.ElideMiddle
                                    color: row.missing ? row.dimColor : row.textColor
                                    font.strikeout: row.missing
                                    font.italic: row.lingering
                                }
                                Kirigami.Icon {
                                    visible: row.flagged
                                    source: "pin"
                                    implicitWidth: Kirigami.Units.iconSizes.small
                                    implicitHeight: implicitWidth
                                }
                                Kirigami.Icon {
                                    visible: row.annotated
                                    source: "note"
                                    implicitWidth: Kirigami.Units.iconSizes.small
                                    implicitHeight: implicitWidth
                                }
                                Kirigami.Icon {
                                    visible: row.missing || row.offline
                                    source: row.missing ? "dialog-warning" : "network-disconnect"
                                    implicitWidth: Kirigami.Units.iconSizes.small
                                    implicitHeight: implicitWidth
                                }
                                Repeater {
                                    model: root.shownColumns

                                    delegate: QQC2.Label {
                                        id: cellLabel

                                        required property int index
                                        required property var modelData

                                        // With its title while a column is dragged elsewhere.
                                        transform: Translate {
                                            x: root.columnShift(cellLabel.index, cellLabel.modelData.name)

                                            Behavior on x {
                                                enabled: root.movingColumn.length > 0 && cellLabel.modelData.name !== root.movingColumn
                                                NumberAnimation {
                                                    duration: Kirigami.Units.longDuration
                                                    easing.type: Easing.OutCubic
                                                }
                                            }
                                        }
                                        opacity: cellLabel.modelData.name === root.movingColumn ? 0.75 : 1

                                        Layout.preferredWidth: root.columnWidth(modelData.name)
                                        text: row.cell(modelData.name)
                                        textFormat: Text.PlainText
                                        horizontalAlignment: modelData.right ? Text.AlignRight : Text.AlignLeft
                                        elide: Text.ElideRight
                                        maximumLineCount: 1
                                        color: row.dimColor
                                    }
                                }
                            }

                            // Where the search words were found, across the full width of the row.
                            QQC2.Label {
                                Layout.fillWidth: true
                                Layout.leftMargin: root.rowIcon + Kirigami.Units.smallSpacing
                                visible: row.snippet.length > 0
                                text: row.snippet
                                textFormat: Text.StyledText
                                elide: Text.ElideRight
                                maximumLineCount: 1
                                color: row.dimColor
                                font: Kirigami.Theme.smallFont
                            }
                        }

                        TapHandler {
                            acceptedButtons: Qt.LeftButton
                            onTapped: (eventPoint, button) => {
                                list.forceActiveFocus();
                                root.selectRow(row.index, point.modifiers);
                            }
                            onDoubleTapped: App.openExternally(row.documentId)
                        }
                        TapHandler {
                            acceptedButtons: Qt.RightButton
                            onTapped: {
                                list.forceActiveFocus();
                                if (!row.selected) {
                                    App.documents.select(row.index, DocumentModel.Single);
                                }
                                contextMenu.popup();
                            }
                        }
                        DragHandler {
                            target: null
                            onActiveChanged: {
                                if (active) {
                                    if (!row.selected) {
                                        App.documents.select(row.index, DocumentModel.Single);
                                    }
                                    dragProxy.start();
                                } else {
                                    dragProxy.Drag.active = false;
                                }
                            }
                        }
                    }

                    // What is listed while a search runs is what was found before it.
                    Binding {
                        target: list.contentItem
                        property: "opacity"
                        value: root.searching ? 0.5 : 1
                    }
                }
            }

            QQC2.ScrollView {
                id: gridScroll

                visible: root.grid
                Layout.fillWidth: true
                Layout.fillHeight: true
                QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

                // The documents as thumbnails: pictures as themselves, PDFs as their first
                // page, texts as their beginning; whatever has no thumbnail, and every one
                // until its thumbnail is made, as the icon of its type. What can be done
                // with a row of the list can be done with a tile.
                GridView {
                    id: tiles

                    readonly property real picture: Config.gridIconSize
                    // Two lines of a name.
                    readonly property real nameHeight: Math.ceil(nameMetrics.lineSpacing * 2) + 2
                    // Small pictures still leave room for a name to be read.
                    readonly property real leastCell: Math.max(picture, Kirigami.Units.gridUnit * 6) + Kirigami.Units.largeSpacing * 2
                    readonly property int columns: Math.max(1, Math.floor(width / leastCell))

                    model: root.grid ? App.documents : null
                    clip: true
                    activeFocusOnTab: true
                    keyNavigationEnabled: false
                    reuseItems: true
                    currentIndex: App.documents.currentRow
                    highlightFollowsCurrentItem: false
                    // The tiles share the width among them.
                    cellWidth: Math.floor(width / columns)
                    cellHeight: picture + nameHeight + Kirigami.Units.smallSpacing * 5
                    topMargin: Kirigami.Units.smallSpacing
                    bottomMargin: Kirigami.Units.smallSpacing

                    FontMetrics {
                        id: nameMetrics
                    }

                    Keys.onLeftPressed: event => root.moveCurrent(Application.layoutDirection === Qt.RightToLeft ? 1 : -1, event.modifiers)
                    Keys.onRightPressed: event => root.moveCurrent(Application.layoutDirection === Qt.RightToLeft ? -1 : 1, event.modifiers)
                    Keys.onUpPressed: event => root.moveCurrent(-columns, event.modifiers)
                    Keys.onDownPressed: event => root.moveCurrent(columns, event.modifiers)
                    Keys.onPressed: event => {
                        const page = Math.max(1, Math.floor(height / cellHeight)) * columns;
                        if (event.key === Qt.Key_PageUp) {
                            root.moveCurrent(-page, event.modifiers);
                        } else if (event.key === Qt.Key_PageDown) {
                            root.moveCurrent(page, event.modifiers);
                        } else if (event.key === Qt.Key_Home) {
                            root.moveCurrent(-App.documents.count, event.modifiers);
                        } else if (event.key === Qt.Key_End) {
                            root.moveCurrent(App.documents.count, event.modifiers);
                        } else if (event.key === Qt.Key_Escape) {
                            App.documents.clearSelection();
                        } else {
                            return;
                        }
                        event.accepted = true;
                    }
                    Keys.onReturnPressed: root.actions.open.trigger()
                    Keys.onEnterPressed: root.actions.open.trigger()

                    // Ctrl and the wheel make the thumbnails larger and smaller, as in a
                    // file manager.
                    WheelHandler {
                        acceptedModifiers: Qt.ControlModifier
                        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                        onWheel: wheel => App.action(wheel.angleDelta.y > 0 ? "grid_larger" : "grid_smaller")?.trigger()
                    }

                    // Reloads replace the whole grid; stay where the user was.
                    property real savedContentY: 0
                    Connections {
                        target: App.documents
                        enabled: root.grid
                        function onModelAboutToBeReset(): void {
                            tiles.savedContentY = tiles.contentY;
                        }
                        function onModelReset(): void {
                            tiles.contentY = Math.max(-tiles.topMargin, Math.min(tiles.savedContentY, tiles.contentHeight - tiles.height));
                        }
                        function onCurrentChanged(): void {
                            if (App.documents.currentRow >= 0) {
                                tiles.positionViewAtIndex(App.documents.currentRow, GridView.Contain);
                            }
                        }
                        function onScopeChanged(): void {
                            tiles.savedContentY = 0;
                            tiles.positionViewAtBeginning();
                        }
                    }

                    delegate: Item {
                        id: tile

                        required property int index
                        required property real documentId
                        required property string name
                        required property string iconName
                        required property string thumbnail
                        required property bool selected
                        required property bool missing
                        required property bool lingering
                        required property bool offline
                        required property bool flagged
                        required property bool annotated
                        required property bool expandable
                        required property bool expanded
                        required property string parentName

                        readonly property color textColor: selected ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor

                        width: tiles.cellWidth
                        height: tiles.cellHeight
                        // Faded: on its way out of this list.
                        opacity: lingering ? 0.55 : 1

                        Accessible.name: name
                        Accessible.role: Accessible.ListItem

                        HoverHandler {
                            id: tileHover
                        }

                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: Kirigami.Units.smallSpacing / 2
                            radius: Kirigami.Units.cornerRadius
                            color: tile.selected ? Kirigami.Theme.highlightColor
                                : tileHover.hovered ? Qt.alpha(Kirigami.Theme.highlightColor, 0.2)
                                : "transparent"
                        }

                        Item {
                            id: face

                            anchors.top: parent.top
                            anchors.topMargin: Kirigami.Units.smallSpacing * 1.5
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: tiles.picture
                            height: tiles.picture

                            Kirigami.Icon {
                                anchors.fill: parent
                                visible: picture.status !== Image.Ready
                                source: tile.iconName
                                opacity: tile.missing || tile.offline ? 0.5 : 1
                            }
                            Image {
                                id: picture

                                // As large as there is room for, less a frame of a point;
                                // a picture that is smaller than that is not blown up.
                                readonly property real room: tiles.picture - 2
                                readonly property real fit: implicitWidth > 0 && implicitHeight > 0 ? Math.min(1, room / implicitWidth, room / implicitHeight) : 1

                                anchors.centerIn: parent
                                width: Math.max(1, Math.round(implicitWidth * fit))
                                height: Math.max(1, Math.round(implicitHeight * fit))
                                source: tile.thumbnail
                                sourceSize: Qt.size(root.thumbnailPixels, root.thumbnailPixels)
                                asynchronous: true
                                mipmap: true
                            }
                            // A fine line around the thumbnail: a white page on a white
                            // ground has an edge.
                            Rectangle {
                                anchors.centerIn: picture
                                visible: picture.status === Image.Ready
                                width: picture.width + 2
                                height: picture.height + 2
                                color: "transparent"
                                border.width: 1
                                border.color: Qt.alpha(Kirigami.Theme.textColor, 0.25)
                            }

                            // What the list says with small icons after the name.
                            Row {
                                anchors.right: parent.right
                                anchors.top: parent.top
                                spacing: 1

                                Repeater {
                                    model: [
                                        { shown: tile.flagged, icon: "pin" },
                                        { shown: tile.annotated, icon: "note" },
                                        { shown: tile.missing || tile.offline, icon: tile.missing ? "dialog-warning" : "network-disconnect" }
                                    ].filter(mark => mark.shown)

                                    delegate: Rectangle {
                                        id: badge

                                        required property var modelData

                                        width: Kirigami.Units.iconSizes.small + Kirigami.Units.smallSpacing
                                        height: width
                                        radius: Kirigami.Units.cornerRadius
                                        color: Kirigami.Theme.backgroundColor
                                        border.width: 1
                                        border.color: Qt.alpha(Kirigami.Theme.textColor, 0.2)

                                        Kirigami.Icon {
                                            anchors.centerIn: parent
                                            width: Kirigami.Units.iconSizes.small
                                            height: width
                                            source: badge.modelData.icon
                                        }
                                    }
                                }
                            }

                            // A mailbox opens like a folder: its messages follow it.
                            QQC2.ToolButton {
                                anchors.left: parent.left
                                anchors.top: parent.top
                                visible: tile.expandable
                                icon.name: tile.expanded ? "go-down-symbolic" : (Application.layoutDirection === Qt.RightToLeft ? "go-previous-symbolic" : "go-next-symbolic")
                                text: tile.expanded ? i18nc("@action:button", "Close") : i18nc("@action:button", "Open")
                                display: QQC2.AbstractButton.IconOnly
                                onClicked: App.documents.setExpanded(tile.documentId, !tile.expanded)

                                QQC2.ToolTip.text: tile.expanded ? i18nc("@info:tooltip", "Hide the messages of this mailbox") : i18nc("@info:tooltip", "Show the messages of this mailbox")
                                QQC2.ToolTip.visible: hovered
                                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                            }
                        }

                        QQC2.Label {
                            id: nameLabel

                            anchors.top: face.bottom
                            anchors.topMargin: Kirigami.Units.smallSpacing
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.leftMargin: Kirigami.Units.smallSpacing * 1.5
                            anchors.rightMargin: Kirigami.Units.smallSpacing * 1.5
                            height: tiles.nameHeight
                            text: tile.name
                            textFormat: Text.PlainText
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignTop
                            wrapMode: Text.Wrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                            color: tile.missing && !tile.selected ? Kirigami.Theme.disabledTextColor : tile.textColor
                            font.strikeout: tile.missing
                            font.italic: tile.lingering
                        }

                        // The whole name where it is cut short, and the mailbox of a message
                        // that is listed away from it.
                        QQC2.ToolTip.text: tile.parentName.length > 0 ? i18nc("@item a message and the mailbox it is in", "%1 — in %2", tile.name, tile.parentName) : tile.name
                        QQC2.ToolTip.visible: tileHover.hovered && (nameLabel.truncated || tile.parentName.length > 0) && !contextMenu.visible
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

                        TapHandler {
                            acceptedButtons: Qt.LeftButton
                            onTapped: (eventPoint, button) => {
                                tiles.forceActiveFocus();
                                root.selectRow(tile.index, point.modifiers);
                            }
                            onDoubleTapped: App.openExternally(tile.documentId)
                        }
                        TapHandler {
                            acceptedButtons: Qt.RightButton
                            onTapped: {
                                tiles.forceActiveFocus();
                                if (!tile.selected) {
                                    App.documents.select(tile.index, DocumentModel.Single);
                                }
                                contextMenu.popup();
                            }
                        }
                        DragHandler {
                            target: null
                            onActiveChanged: {
                                if (active) {
                                    if (!tile.selected) {
                                        App.documents.select(tile.index, DocumentModel.Single);
                                    }
                                    dragProxy.start();
                                } else {
                                    dragProxy.Drag.active = false;
                                }
                            }
                        }
                    }

                    // What is shown while a search runs is what was found before it.
                    Binding {
                        target: tiles.contentItem
                        property: "opacity"
                        value: root.searching ? 0.5 : 1
                    }
                }
            }
        }

        Kirigami.LoadingPlaceholder {
            // In the middle of the room below the bar.
            anchors.centerIn: parent
            anchors.verticalCenterOffset: viewBar.height / 2
            width: parent.width - Kirigami.Units.gridUnit * 4
            visible: App.documents.count === 0 && root.searching
            text: App.documents.opening ? i18nc("@info:progress", "Opening database…")
                : App.documents.effectiveFilter.length > 0 ? i18nc("@info:progress", "Searching…")
                : i18nc("@info:progress", "Listing documents…")
        }

        Kirigami.PlaceholderMessage {
            // In the middle of the room below the bar.
            anchors.centerIn: parent
            anchors.verticalCenterOffset: viewBar.height / 2
            width: parent.width - Kirigami.Units.gridUnit * 4
            visible: App.documents.count === 0 && !root.loading

            readonly property bool searching: App.documents.effectiveFilter.length > 0
            readonly property bool nothingIndexed: !searching && App.sidebar.currentKey === "place:all"

            // What is empty is the place that is selected in the sidebar: its icon says
            // which, and it is the very drawing the sidebar has. Asked for at this
            // size, the theme would give pictures in colour for some icons and line
            // drawings for others; the small one, enlarged and in the colour of the
            // text, is of a kind for all of them.
            readonly property string iconName: searching ? "search" : App.sidebar.currentIcon
            readonly property string lineDrawing: App.lineIcon(iconName).toString()

            icon.name: iconName
            icon.source: lineDrawing
            icon.color: lineDrawing.length > 0 ? Kirigami.Theme.textColor : "transparent"
            text: searching ? i18nc("@info:placeholder", "No documents found")
                : nothingIndexed ? i18nc("@info:placeholder", "Nothing indexed yet")
                : App.sidebar.currentKey === "place:inbox" ? i18nc("@info:placeholder", "The inbox is empty")
                : i18nc("@info:placeholder", "No documents here")
            explanation: searching ? i18nc("@info:placeholder", "Nothing in “%1” matches the search.", App.sidebar.currentLabel)
                : nothingIndexed ? i18nc("@info:placeholder", "Add a folder, or drop one here, to index its files where they are.")
                : App.sidebar.currentKey === "place:inbox" ? i18nc("@info:placeholder", "New files wait here until you work with them: put one into a group, tag it, pin it or write a note on it, and it leaves the inbox.")
                : ""
            helpfulAction: nothingIndexed ? root.actions.addFolder : null
        }

        // A single drag source for the whole selection, shared by all rows.
        Item {
            id: dragProxy

            function start(): void {
                const ids = App.documents.selectedIds;
                Drag.mimeData = {
                    "application/x-indeks-documents": ids.join(","),
                    "text/uri-list": App.documents.selectedUrls().join("\n")
                };
                Drag.active = true;
            }

            Drag.dragType: Drag.Automatic
            Drag.supportedActions: Qt.CopyAction | Qt.LinkAction
        }

        // Runs along the top of the list while a search through the text of the documents
        // is under way, so that it is plain that Indeks is at it and what is listed is not
        // the answer yet.
        QQC2.ProgressBar {
            // On the line between the column titles and the rows, covering neither.
            y: (root.grid ? gridScroll.y : scrollView.y) - height / 2
            width: parent.width
            z: 1
            visible: root.searching
            indeterminate: true

            Accessible.name: i18nc("@info:progress", "Searching…")
        }

        // Folders dropped from elsewhere become indexed locations. Files are not taken, and
        // Indeks says so: a drop that is simply refused would leave the user guessing.
        DropArea {
            anchors.fill: parent
            onEntered: drag => {
                if (!drag.hasUrls || drag.formats.includes("application/x-indeks-documents")) {
                    drag.accepted = false;
                }
            }
            onDropped: drop => {
                if (drop.hasUrls && !drop.formats.includes("application/x-indeks-documents")) {
                    App.addFolders(drop.urls);
                    drop.accept(Qt.LinkAction);
                }
            }
        }
    }

    // What the list shows and how: columns to add and to take away, and the height of the rows.
    QQC2.Menu {
        id: columnMenu

        // Which view the choice is for: every view has its own.
        QQC2.MenuItem {
            enabled: false
            text: i18nc("@info:inmenu heading of the list of columns; %1 is the name of a place, group, tag or search", "Columns in “%1”", App.sidebar.currentLabel)
        }
        Instantiator {
            model: root.columns

            delegate: QQC2.MenuItem {
                required property var modelData

                text: modelData.title
                checkable: true
                checked: root.viewColumns.includes(modelData.name)
                onTriggered: root.setColumnShown(modelData.name, checked)
            }
            // Below the heading.
            onObjectAdded: (index, object) => columnMenu.insertItem(index + 1, object)
            onObjectRemoved: (index, object) => columnMenu.removeItem(object)
        }
        QQC2.MenuItem {
            // Chosen and not there: the reason is not to be guessed at.
            visible: root.hiddenColumns > 0
            height: visible ? implicitHeight : 0
            enabled: false
            text: i18ncp("@info:inmenu", "%1 chosen column does not fit at this width", "%1 chosen columns do not fit at this width", root.hiddenColumns)
        }
        QQC2.MenuSeparator {}
        QQC2.Menu {
            title: i18nc("@title:menu how tall the rows of the list are", "Row Height")

            QQC2.MenuItem {
                text: i18nc("@item:inmenu height of rows", "Compact")
                checkable: true
                checked: Config.listRowSize === 0
                onTriggered: {
                    Config.listRowSize = 0;
                    Config.save();
                }
            }
            QQC2.MenuItem {
                text: i18nc("@item:inmenu height of rows", "Normal")
                checkable: true
                checked: Config.listRowSize === 1
                onTriggered: {
                    Config.listRowSize = 1;
                    Config.save();
                }
            }
            QQC2.MenuItem {
                text: i18nc("@item:inmenu height of rows", "Tall")
                checkable: true
                checked: Config.listRowSize === 2
                onTriggered: {
                    Config.listRowSize = 2;
                    Config.save();
                }
            }
        }
        QQC2.MenuSeparator {}
        QQC2.MenuItem {
            text: i18nc("@action:inmenu", "Use Standard Columns Here")
            icon.name: "edit-undo"
            // Only a view that has columns of its own can give them up.
            enabled: root.ownColumns
            onTriggered: root.useStandardColumns()
        }
        QQC2.MenuItem {
            text: i18nc("@action:inmenu", "Use These Columns in All Views")
            icon.name: "view-list-details"
            onTriggered: root.useColumnsEverywhere()
        }
        QQC2.MenuSeparator {}
        QQC2.MenuItem {
            text: i18nc("@action:inmenu", "Reset Columns and Rows in All Views")
            icon.name: "edit-reset"
            onTriggered: root.resetColumns()
        }
    }

    QQC2.Menu {
        id: contextMenu

        onAboutToShow: scriptMenu.scripts = App.library.scripts()

        QQC2.MenuItem { action: root.actions.open }
        QQC2.MenuItem { action: root.actions.openWith }
        QQC2.MenuItem { action: root.actions.showInFileManager }
        QQC2.MenuItem { action: root.actions.copyLocation }
        QQC2.MenuItem {
            // For a website only.
            action: root.actions.copyAddress
            visible: enabled
            height: visible ? implicitHeight : 0
        }
        QQC2.MenuSeparator {}
        QQC2.MenuItem { action: root.actions.tag }
        QQC2.MenuItem { action: root.actions.group }
        QQC2.MenuItem { action: root.actions.flag }
        QQC2.MenuItem { action: root.actions.note }
        QQC2.MenuItem {
            action: root.actions.docTodo
            text: i18nc("@action:inmenu", "Create To-Do…")
        }
        QQC2.MenuItem { action: root.actions.links }
        QQC2.MenuItem {
            // For several documents at once; one alone has nothing to be linked with here.
            action: root.actions.linkSelected
            visible: enabled
            height: visible ? implicitHeight : 0
        }
        QQC2.MenuSeparator {}
        QQC2.MenuItem { action: root.actions.edit }
        QQC2.MenuItem { action: root.actions.rename }
        QQC2.MenuItem { action: root.actions.ocr }
        QQC2.MenuItem { action: root.actions.reindex }
        QQC2.Menu {
            id: scriptMenu

            // The scripts of the database, looked up when the menu opens.
            property var scripts: []

            title: i18nc("@title:menu", "Run Script")
            icon.name: "system-run"
            enabled: root.actions.runScript.enabled

            Instantiator {
                model: scriptMenu.scripts

                delegate: QQC2.MenuItem {
                    required property var modelData

                    text: modelData.name
                    // One that came from elsewhere has to be read and allowed first.
                    enabled: modelData.allowed
                    onTriggered: App.runScript(modelData.file)
                }
                onObjectAdded: (index, object) => scriptMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => scriptMenu.removeItem(object)
            }
            QQC2.MenuSeparator {
                visible: scriptMenu.scripts.length > 0
                height: visible ? implicitHeight : 0
            }
            QQC2.MenuItem { action: root.actions.newScript }
            QQC2.MenuItem { action: root.actions.scripts }
        }
        QQC2.MenuItem {
            action: root.actions.revert
            visible: enabled
            height: visible ? implicitHeight : 0
        }
        QQC2.MenuSeparator {}
        QQC2.MenuItem {
            action: root.actions.toInbox
            visible: enabled
            height: visible ? implicitHeight : 0
        }
        QQC2.MenuItem {
            action: root.actions.fromInbox
            visible: enabled
            height: visible ? implicitHeight : 0
        }
        QQC2.MenuItem {
            action: root.actions.fromGroup
            visible: enabled
            height: visible ? implicitHeight : 0
        }
        QQC2.MenuItem {
            action: root.actions.restore
            visible: enabled
            height: visible ? implicitHeight : 0
        }
        QQC2.MenuItem {
            action: root.actions.trash
            visible: enabled
            height: visible ? implicitHeight : 0
        }
    }
}
