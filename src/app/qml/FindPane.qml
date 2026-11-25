// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Searching within the document that is shown in the preview: a field for what to find, how
// many places it was found at, and those places as a list to step through. The places are
// marked in the document, and the one that is chosen is brought into view there.
//
// The finding itself is the preview's, see PreviewPane; this only asks and shows.
QQC2.Pane {
    id: root

    required property Actions actions
    /** The preview whose document is searched. */
    required property PreviewPane preview
    /** What is typed into the field: what the preview is to find while this pane is in view. */
    readonly property alias text: field.text

    // The list names this many places; the rest is reached with the arrows.
    readonly property int listed: 300
    readonly property int count: preview.findCount
    readonly property int index: preview.findIndex

    function focusField(): void {
        field.forceActiveFocus();
        field.selectAll();
    }

    // What was looked for in one document is not looked for in the next: another document
    // starts with an empty field and without marks.
    Connections {
        target: root.preview
        function onDocumentIdChanged(): void {
            field.clear();
        }
    }

    padding: 0
    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    contentItem: ColumnLayout {
        spacing: 0

        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.smallSpacing
            spacing: Kirigami.Units.smallSpacing

            Kirigami.SearchField {
                id: field

                Layout.fillWidth: true
                enabled: root.preview.findAvailable
                placeholderText: i18nc("@info:placeholder", "Find in this document…")
                // What is typed is found as it is typed; Enter goes on to the next place.
                autoAccept: false
                Keys.onPressed: event => {
                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                        if (event.modifiers & Qt.ShiftModifier) {
                            root.preview.findPrevious();
                        } else {
                            root.preview.findNext();
                        }
                        event.accepted = true;
                    } else if (event.key === Qt.Key_Down) {
                        root.preview.findNext();
                        event.accepted = true;
                    } else if (event.key === Qt.Key_Up) {
                        root.preview.findPrevious();
                        event.accepted = true;
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                QQC2.Label {
                    Layout.fillWidth: true
                    visible: root.preview.findAvailable
                    elide: Text.ElideRight
                    color: field.text.length > 0 && root.count === 0 ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.disabledTextColor
                    text: {
                        if (field.text.length === 0) {
                            return "";
                        }
                        if (root.count === 0) {
                            return i18nc("@info:status", "Not found");
                        }
                        const total = root.preview.findMore ? i18ncp("@info:status how many places a text was found at", "more than %1 place", "more than %1 places", root.count)
                                                            : i18ncp("@info:status how many places a text was found at", "%1 place", "%1 places", root.count);
                        return root.index >= 0 ? i18nc("@info:status which place of how many, e.g. 3 of 17 places", "%1 of %2", root.index + 1, total) : total;
                    }
                }
                Item {
                    Layout.fillWidth: true
                    visible: !root.preview.findAvailable
                }
                QQC2.ToolButton {
                    action: root.actions.findPrevious
                    display: QQC2.AbstractButton.IconOnly
                    enabled: root.count > 0

                    QQC2.ToolTip.text: i18nc("@info:tooltip", "To the place before this one (Shift+Enter)")
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                }
                QQC2.ToolButton {
                    action: root.actions.findNext
                    display: QQC2.AbstractButton.IconOnly
                    enabled: root.count > 0

                    QQC2.ToolTip.text: i18nc("@info:tooltip", "To the next place (Enter)")
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                }
            }
        }
        Kirigami.Separator {
            Layout.fillWidth: true
        }

        QQC2.ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.count > 0
            QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

            ListView {
                id: list

                // By number: what is said about a place is asked for when its line comes up.
                model: Math.min(root.count, root.listed)
                clip: true
                currentIndex: root.index
                onCurrentIndexChanged: {
                    if (currentIndex >= 0) {
                        positionViewAtIndex(currentIndex, ListView.Contain);
                    }
                }

                delegate: QQC2.ItemDelegate {
                    id: place

                    required property int index
                    // Asked for anew with every other search, also one that finds as many
                    // places as the one before.
                    readonly property var hit: root.preview.findText.length > 0 ? root.preview.findHit(index) : ({})
                    readonly property bool current: index === root.index

                    function escaped(text: string): string {
                        return (text ?? "").replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
                    }

                    width: ListView.view?.width ?? 0
                    highlighted: current
                    onClicked: root.preview.findGo(index)

                    Accessible.name: (hit.before ?? "") + (hit.match ?? "") + (hit.after ?? "")

                    contentItem: ColumnLayout {
                        spacing: 0

                        QQC2.Label {
                            Layout.fillWidth: true
                            visible: text.length > 0
                            text: place.hit.label ?? ""
                            font: Kirigami.Theme.smallFont
                            elide: Text.ElideRight
                            color: place.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.disabledTextColor
                        }
                        QQC2.Label {
                            Layout.fillWidth: true
                            // The words around the place, with what was found set off.
                            textFormat: Text.StyledText
                            text: place.escaped(place.hit.before) + "<b>" + place.escaped(place.hit.match) + "</b>" + place.escaped(place.hit.after)
                            wrapMode: Text.Wrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                            color: place.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                        }
                    }
                }

                footer: Item {
                    width: ListView.view?.width ?? 0
                    height: root.count > root.listed ? rest.implicitHeight : 0

                    QQC2.Label {
                        id: rest

                        width: parent.width
                        visible: root.count > root.listed
                        padding: Kirigami.Units.largeSpacing
                        wrapMode: Text.Wrap
                        font: Kirigami.Theme.smallFont
                        color: Kirigami.Theme.disabledTextColor
                        text: i18ncp("@info", "The first place is listed; the arrows above reach all of them.",
                                     "The first %1 places are listed; the arrows above reach all of them.", root.listed)
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.count === 0

            Kirigami.PlaceholderMessage {
                anchors.centerIn: parent
                width: parent.width - Kirigami.Units.gridUnit * 2
                // The small drawing, enlarged and in the colour of the text: asked for at
                // this size, some themes give a picture in colour (see DocumentList).
                readonly property string lineDrawing: App.lineIcon("search").toString()

                icon.name: "search"
                icon.source: lineDrawing
                icon.color: lineDrawing.length > 0 ? Kirigami.Theme.textColor : "transparent"
                text: root.preview.findAvailable ? i18nc("@info:placeholder", "Find in this document")
                    : root.preview.findPaused ? i18nc("@info:placeholder", "Not while editing")
                    : root.preview.documentId > 0 ? i18nc("@info:placeholder", "Nothing to search here")
                    : i18nc("@info:placeholder", "No document selected")
                explanation: root.preview.findAvailable
                        ? i18nc("@info:placeholder", "What you type is looked for in the document that is shown in the preview, whatever its case. The places it stands at are marked there and listed here.")
                    : root.preview.findPaused
                        ? i18nc("@info:placeholder", "Finish editing to search within the document.")
                    : root.preview.documentId > 0
                        ? i18nc("@info:placeholder", "The preview shows no text of this document that could be searched.")
                        : ""
            }
        }
    }
}
