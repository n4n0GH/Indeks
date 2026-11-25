// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQml.Models
import org.kde.kirigami as Kirigami

// A file of comma or tab separated values as the table it stands for: the columns under their
// names, numbers aligned, the rows numbered as in the file. Only what is in view is laid out,
// so a file of a hundred thousand rows scrolls like a small one.
Item {
    id: root

    /** The file to show, and the document's name, which may tell what separates the values. */
    required property string path
    required property string fileName
    property real zoom: 1
    property list<string> searchTerms
    /** What is known about the table: rows, columns, whether it is still being read. */
    readonly property TableModel table: tableModel

    signal zoomRequested(bool larger)

    readonly property font cellFont: Qt.font({ family: Kirigami.Theme.defaultFont.family, pointSize: Kirigami.Theme.defaultFont.pointSize * zoom })
    readonly property font headFont: Qt.font({ family: Kirigami.Theme.defaultFont.family, pointSize: Kirigami.Theme.defaultFont.pointSize * zoom, weight: Font.DemiBold })
    readonly property real cellPadding: Math.round(Kirigami.Units.smallSpacing * 1.5)
    readonly property real rowHeight: Math.ceil(metrics.height + Kirigami.Units.smallSpacing * 2)
    readonly property color gridColor: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, Kirigami.Theme.frameContrast)
    // The cells that are marked: those with what is searched for within this document, or,
    // while nothing is, those with a word the list of documents was searched for.
    readonly property list<string> wanted: findText.length > 0 ? [findText.toLowerCase()] : searchTerms.map(term => term.toLowerCase()).filter(term => term.length > 0)

    // ------------------------------------------------------------ searching within the table
    // What the preview asks of a view that can be searched; see PreviewPane.

    property string findText
    // The cells that hold the text: points with the column as x and the row as y.
    property var findCells: []
    readonly property int findCount: findCells.length
    property bool findMore: false
    property int findIndex: -1
    readonly property int findMost: 5000

    function runFind(): void {
        findIndex = -1;
        if (findText.length === 0 || tableModel.loading) {
            findCells = [];
            findMore = false;
            return;
        }
        const cells = tableModel.findAll(findText, findMost + 1);
        findMore = cells.length > findMost;
        findCells = findMore ? cells.slice(0, findMost) : cells;
    }

    function findHit(at: int): var {
        if (at < 0 || at >= findCells.length) {
            return ({});
        }
        // The row by the number it has at the left of the table, which is its line in the file.
        const cell = findCells[at];
        const text = tableModel.cellText(cell.y, cell.x);
        const position = Math.max(0, text.toLowerCase().indexOf(findText.toLowerCase()));
        return {
            label: i18nc("@info a cell of a table: number of the row, name of the column", "Row %1, %2", tableModel.headerData(cell.y, Qt.Vertical), tableModel.columnName(cell.x)),
            before: text.slice(Math.max(0, position - 40), position).replace(/\s+/g, " "),
            match: text.slice(position, position + findText.length),
            after: text.slice(position + findText.length, position + findText.length + 80).replace(/\s+/g, " ")
        };
    }

    function showHit(at: int): void {
        if (at < 0 || at >= findCells.length) {
            return;
        }
        findIndex = at;
        const cell = findCells[at];
        view.positionViewAtCell(cell, TableView.AlignVCenter | TableView.AlignLeft);
        selection.setCurrentIndex(tableModel.index(cell.y, cell.x), ItemSelectionModel.NoUpdate);
    }

    onFindTextChanged: runFind()

    Connections {
        target: tableModel
        function onLoadingChanged(): void {
            root.runFind();
        }
        function onLayoutReset(): void {
            root.runFind();
        }
    }

    // Columns start out as wide as what their first rows hold, within reason; dragging the
    // line between two column names changes that.
    function naturalWidth(column: int): real {
        const characters = Math.max(3, Math.min(40, tableModel.widthHint(column)));
        return Math.ceil(characters * metrics.advanceWidth("0") * 1.08) + cellPadding * 2 + 1;
    }

    function copyCurrent(): void {
        if (view.currentRow >= 0 && view.currentColumn >= 0) {
            App.copyText(tableModel.cellText(view.currentRow, view.currentColumn));
        }
    }

    // Brings the first cell that holds a search word into view.
    function showFirstHit(): void {
        const hit = tableModel.find(searchTerms);
        if (hit.x >= 0) {
            view.positionViewAtCell(hit, TableView.AlignVCenter | TableView.AlignLeft);
            selection.setCurrentIndex(tableModel.index(hit.y, hit.x), ItemSelectionModel.NoUpdate);
        }
    }

    onZoomChanged: view.forceLayout()

    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false

    FontMetrics {
        id: metrics

        font: root.cellFont
    }

    TableModel {
        id: tableModel

        path: root.path
        fileName: root.fileName
        onLayoutReset: {
            view.clearColumnWidths();
            view.contentX = 0;
            view.contentY = 0;
            Qt.callLater(root.showFirstHit);
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Kirigami.Theme.backgroundColor
    }

    component HeadCell: Rectangle {
        property alias text: label.text
        property alias alignment: label.horizontalAlignment

        implicitHeight: root.rowHeight
        color: Kirigami.Theme.alternateBackgroundColor

        QQC2.Label {
            id: label

            anchors.fill: parent
            anchors.leftMargin: root.cellPadding
            anchors.rightMargin: root.cellPadding
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            textFormat: Text.PlainText
            font: root.headFont
        }
        Rectangle {
            anchors.right: parent.right
            width: 1
            height: parent.height
            color: root.gridColor
        }
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: root.gridColor
        }
    }

    // The corner between the two headers.
    HeadCell {
        width: rowNumbers.width
        height: columnNames.height
        visible: view.rows > 0
    }

    QQC2.HorizontalHeaderView {
        id: columnNames

        anchors.left: view.left
        anchors.right: view.right
        anchors.top: parent.top
        syncView: view
        clip: true
        // Dragging means making a column wider, not moving the table.
        interactive: false

        delegate: HeadCell {
            required property int column
            required property string display

            text: display
            alignment: tableModel.isNumeric(column) ? Text.AlignRight : Text.AlignLeft

            QQC2.ToolTip.text: display
            QQC2.ToolTip.visible: nameHover.hovered && display.length > 0
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

            HoverHandler {
                id: nameHover
            }
        }
    }

    QQC2.VerticalHeaderView {
        id: rowNumbers

        anchors.left: parent.left
        anchors.top: view.top
        anchors.bottom: view.bottom
        syncView: view
        clip: true
        interactive: false
        resizableRows: false

        delegate: HeadCell {
            required property string display

            implicitWidth: Math.ceil(metrics.advanceWidth(String(tableModel.rows + 1))) + root.cellPadding * 2 + 1
            text: display
            alignment: Text.AlignRight
            opacity: 0.75
        }
    }

    TableView {
        id: view

        anchors.left: rowNumbers.right
        anchors.top: columnNames.bottom
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        // The scroll bars stand beside the table, not on it: rows end where the bar begins.
        anchors.rightMargin: verticalBar.visible ? verticalBar.width : 0
        anchors.bottomMargin: horizontalBar.visible ? horizontalBar.height : 0
        clip: true
        model: tableModel
        boundsBehavior: Flickable.StopAtBounds
        reuseItems: true
        resizableColumns: true
        // A click makes a cell the current one, the arrow keys move on from it.
        selectionModel: ItemSelectionModel {
            id: selection

            model: tableModel
        }
        selectionBehavior: TableView.SelectionDisabled
        columnWidthProvider: column => {
            const chosen = view.explicitColumnWidth(column);
            return chosen >= 0 ? chosen : root.naturalWidth(column);
        }
        rowHeightProvider: () => root.rowHeight

        QQC2.ScrollBar.vertical: verticalBar
        QQC2.ScrollBar.horizontal: horizontalBar

        Keys.onShortcutOverride: event => {
            if (event.matches(StandardKey.Copy) && view.currentRow >= 0) {
                event.accepted = true;
            }
        }
        Keys.onPressed: event => {
            if (event.matches(StandardKey.Copy) && view.currentRow >= 0) {
                root.copyCurrent();
                event.accepted = true;
            }
        }

        delegate: Rectangle {
            id: cell

            required property int row
            required property int column
            required property string display
            required property bool numeric
            required property bool current

            readonly property bool hit: root.wanted.length > 0 && root.wanted.some(term => cell.display.toLowerCase().includes(term))

            implicitHeight: root.rowHeight
            color: current ? Qt.alpha(Kirigami.Theme.highlightColor, view.activeFocus ? 0.35 : 0.2)
                : hit ? Qt.alpha(Kirigami.Theme.neutralTextColor, 0.3)
                : row % 2 === 1 ? Kirigami.Theme.alternateBackgroundColor : Kirigami.Theme.backgroundColor

            QQC2.Label {
                id: content

                anchors.fill: parent
                anchors.leftMargin: root.cellPadding
                anchors.rightMargin: root.cellPadding
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: cell.numeric ? Text.AlignRight : Text.AlignLeft
                elide: Text.ElideRight
                textFormat: Text.PlainText
                font: root.cellFont
                text: cell.display
            }
            Rectangle {
                anchors.right: parent.right
                width: 1
                height: parent.height
                color: root.gridColor
                opacity: 0.6
            }

            // A cell that is cut short tells the rest when asked.
            QQC2.ToolTip.text: cellHover.hovered && content.truncated ? tableModel.cellText(cell.row, cell.column).slice(0, 1500) : ""
            QQC2.ToolTip.visible: cellHover.hovered && content.truncated
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

            HoverHandler {
                id: cellHover
            }
            TapHandler {
                onTapped: view.forceActiveFocus()
            }
        }

        WheelHandler {
            acceptedModifiers: Qt.ControlModifier
            onWheel: event => {
                if (event.angleDelta.y !== 0) {
                    root.zoomRequested(event.angleDelta.y > 0);
                }
            }
        }
    }

    // Beside the table and below it, in room of their own.
    QQC2.ScrollBar {
        id: verticalBar

        anchors.top: view.top
        anchors.bottom: view.bottom
        anchors.right: parent.right
    }
    QQC2.ScrollBar {
        id: horizontalBar

        anchors.left: view.left
        anchors.right: view.right
        anchors.bottom: parent.bottom
    }

    QQC2.BusyIndicator {
        anchors.centerIn: parent
        running: tableModel.loading
        visible: running
    }

    Kirigami.PlaceholderMessage {
        anchors.centerIn: parent
        width: parent.width - Kirigami.Units.gridUnit * 4
        visible: !tableModel.loading && tableModel.rows === 0 && tableModel.columns === 0
        icon.name: "table"
        text: i18nc("@info:placeholder", "This file holds no rows")
    }
}
