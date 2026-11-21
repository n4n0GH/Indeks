// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Shapes
import org.kde.kirigami as Kirigami

// Shows an Obsidian canvas: its cards, groups and the connections between them. The board
// can be dragged around and zoomed. It is a viewer; the canvas file is not changed.
ColumnLayout {
    id: root

    required property real documentId
    property var canvas: ({})

    readonly property bool valid: canvas.valid ?? false
    readonly property real boardWidth: canvas.width ?? 1
    readonly property real boardHeight: canvas.height ?? 1

    /** 0 fits the whole board into the view, anything else is the scale. */
    property real zoom: 0
    // Measured against the scroll view, not the flickable inside it: the flickable changes size
    // when scroll bars come and go, which would make the fit depend on its own result.
    readonly property real fitScale: Math.max(0.02, Math.min(1, (scrollView.width - 2) / boardWidth, (scrollView.height - 2) / boardHeight))
    readonly property real effectiveScale: zoom > 0 ? zoom : fitScale

    // The six preset colours of a canvas. Like ink on a drawing they belong to the document:
    // a card marked red in Obsidian is red here too, under any colour scheme.
    readonly property list<color> presetColors: ["#fb464c", "#e9973f", "#e0de71", "#44cf6e", "#53dfdd", "#a882ff"]
    readonly property color plainColor: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.45)

    spacing: 0

    function cardColor(value: string): color {
        if (value.length === 1 && value >= "1" && value <= "6") {
            return presetColors[parseInt(value) - 1];
        }
        return value.startsWith("#") ? value : plainColor;
    }

    function hasColor(value: string): bool {
        return value.length > 0;
    }

    // Zooms while keeping the point under (viewX, viewY) where it is.
    function zoomTo(scale: real, viewX: real, viewY: real): void {
        const before = effectiveScale;
        const after = Math.max(0.02, Math.min(4, scale));
        const boardX = (flickable.contentX + viewX - frame.x) / before;
        const boardY = (flickable.contentY + viewY - frame.y) / before;
        zoom = after;
        flickable.contentX = Math.max(0, Math.min(boardWidth * after - flickable.width, boardX * after - viewX));
        flickable.contentY = Math.max(0, Math.min(boardHeight * after - flickable.height, boardY * after - viewY));
    }

    function zoomBy(factor: real): void {
        zoomTo(effectiveScale * factor, flickable.width / 2, flickable.height / 2);
    }

    // For the bar above the preview; see PreviewToolBar.
    readonly property string status: valid ? i18nc("@info:status number of cards and of connections on a canvas", "%1, %2",
                                                   i18ncp("@info:status", "%1 card", "%1 cards", (canvas.nodes ?? []).length),
                                                   i18ncp("@info:status", "%1 connection", "%1 connections", (canvas.edges ?? []).length)) : ""
    readonly property bool zoomable: valid
    readonly property real zoomFactor: effectiveScale
    readonly property bool zoomIsDefault: zoom === 0
    readonly property string zoomResetText: i18nc("@info:tooltip", "Show the whole canvas")
    function zoomIn(): void {
        zoomBy(1.25);
    }
    function zoomOut(): void {
        zoomBy(1 / 1.25);
    }
    function zoomReset(): void {
        zoom = 0;
    }
    readonly property list<Kirigami.Action> viewActions: [
        Kirigami.Action {
            icon.name: "zoom-original"
            text: i18nc("@action:intoolbar", "Real Size")
            displayHint: Kirigami.DisplayHint.IconOnly
            visible: root.valid
            enabled: root.effectiveScale !== 1
            onTriggered: root.zoomTo(1, flickable.width / 2, flickable.height / 2)
        }
    ]

    function openCard(card: var): void {
        if (card.type === "file" && (card.documentId ?? 0) > 0) {
            App.showDocument(card.documentId);
        } else if (card.type === "file" && card.url) {
            Qt.openUrlExternally(card.url);
        } else if (card.type === "link") {
            linkDialog.ask(card.url);
        }
    }

    Component.onCompleted: canvas = App.library.canvas(documentId)

    QQC2.ScrollView {
        id: scrollView

        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: root.valid

        Flickable {
            id: flickable

            contentWidth: frame.width
            contentHeight: frame.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            // The board at its shown size; the surface inside is laid out in canvas units and scaled.
            Item {
                id: frame

                x: Math.max(0, (flickable.width - width) / 2)
                y: Math.max(0, (flickable.height - height) / 2)
                width: root.boardWidth * root.effectiveScale
                height: root.boardHeight * root.effectiveScale

                Item {
                    id: surface

                    width: root.boardWidth
                    height: root.boardHeight
                    scale: root.effectiveScale
                    transformOrigin: Item.TopLeft

                    // Cards and groups, in the order of the file: later ones lie on top.
                    Repeater {
                        model: root.canvas.nodes ?? []

                        Item {
                            id: card

                            required property var modelData
                            readonly property bool group: modelData.type === "group"
                            readonly property color accent: root.cardColor(modelData.color)
                            readonly property bool coloured: root.hasColor(modelData.color)
                            readonly property bool openable: modelData.type === "link" || (modelData.type === "file" && modelData.fileKind !== "missing")

                            x: modelData.x
                            y: modelData.y
                            width: modelData.width
                            height: modelData.height
                            // Groups frame other cards and stay below the connections, cards above.
                            z: group ? 0 : 2

                            Rectangle {
                                id: body

                                anchors.fill: parent
                                radius: Kirigami.Units.cornerRadius * 1.5
                                color: card.group ? Qt.alpha(card.accent, card.coloured ? 0.1 : 0.04)
                                    : card.coloured ? Kirigami.ColorUtils.tintWithAlpha(Kirigami.Theme.backgroundColor, card.accent, 0.14)
                                    : Kirigami.Theme.backgroundColor
                                border.width: 2
                                border.color: card.coloured ? card.accent : Qt.alpha(root.plainColor, card.group ? 0.5 : 0.8)
                                clip: true

                                Loader {
                                    anchors.fill: parent
                                    anchors.margins: body.border.width
                                    sourceComponent: card.group ? null
                                        : card.modelData.type === "file" ? fileContent
                                        : card.modelData.type === "link" ? linkContent
                                        : textContent
                                }
                            }

                            // A group's caption sits above its frame.
                            Text {
                                visible: card.group && card.modelData.text.length > 0
                                anchors.bottom: body.top
                                anchors.bottomMargin: Kirigami.Units.smallSpacing
                                anchors.left: body.left
                                width: body.width
                                text: card.modelData.text
                                textFormat: Text.PlainText
                                elide: Text.ElideRight
                                font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.3
                                font.weight: Font.DemiBold
                                color: card.coloured ? card.accent : Kirigami.Theme.textColor
                                renderType: Text.QtRendering
                            }

                            TapHandler {
                                enabled: card.openable
                                onDoubleTapped: root.openCard(card.modelData)
                            }
                            HoverHandler {
                                enabled: card.openable
                                cursorShape: Qt.PointingHandCursor
                            }

                            Component {
                                id: textContent

                                Text {
                                    padding: Kirigami.Units.largeSpacing * 1.5
                                    text: card.modelData.text
                                    textFormat: Text.MarkdownText
                                    wrapMode: Text.Wrap
                                    color: Kirigami.Theme.textColor
                                    linkColor: Kirigami.Theme.linkColor
                                    font: Kirigami.Theme.defaultFont
                                    // Text is scaled with the board; this rendering stays sharp at any zoom.
                                    renderType: Text.QtRendering
                                    onLinkActivated: link => linkDialog.ask(link)
                                }
                            }

                            Component {
                                id: linkContent

                                ColumnLayout {
                                    spacing: Kirigami.Units.smallSpacing

                                    Item {
                                        Layout.fillHeight: true
                                    }
                                    Kirigami.Icon {
                                        Layout.alignment: Qt.AlignHCenter
                                        source: "internet-services"
                                        implicitWidth: Math.min(Kirigami.Units.iconSizes.huge, card.width / 3, card.height / 3)
                                        implicitHeight: implicitWidth
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        Layout.margins: Kirigami.Units.largeSpacing
                                        text: card.modelData.text
                                        textFormat: Text.PlainText
                                        horizontalAlignment: Text.AlignHCenter
                                        wrapMode: Text.WrapAnywhere
                                        maximumLineCount: 3
                                        elide: Text.ElideRight
                                        color: Kirigami.Theme.linkColor
                                        font: Kirigami.Theme.defaultFont
                                        renderType: Text.QtRendering
                                    }
                                    Item {
                                        Layout.fillHeight: true
                                    }
                                }
                            }

                            Component {
                                id: fileContent

                                ColumnLayout {
                                    id: fileCard

                                    readonly property string fileKind: card.modelData.fileKind ?? "missing"

                                    spacing: 0

                                    // Title bar with the file's name.
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Layout.margins: Kirigami.Units.smallSpacing
                                        visible: fileCard.fileKind !== "image"
                                        spacing: Kirigami.Units.smallSpacing

                                        Kirigami.Icon {
                                            source: fileCard.fileKind === "missing" ? "dialog-warning" : card.modelData.icon
                                            implicitWidth: Kirigami.Units.iconSizes.small
                                            implicitHeight: implicitWidth
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: card.modelData.text
                                            textFormat: Text.PlainText
                                            elide: Text.ElideMiddle
                                            color: Kirigami.Theme.disabledTextColor
                                            font: Kirigami.Theme.smallFont
                                            renderType: Text.QtRendering
                                        }
                                    }
                                    Kirigami.Separator {
                                        Layout.fillWidth: true
                                        visible: fileCard.fileKind === "text"
                                    }

                                    Image {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        visible: fileCard.fileKind === "image"
                                        source: visible ? card.modelData.url : ""
                                        asynchronous: true
                                        autoTransform: true
                                        mipmap: true
                                        fillMode: Image.PreserveAspectFit
                                        // Decoded no larger than twice the card: boards can hold many big pictures.
                                        sourceSize.width: card.width * 2
                                        sourceSize.height: card.height * 2
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        visible: fileCard.fileKind === "text"
                                        padding: Kirigami.Units.largeSpacing
                                        text: card.modelData.fileText ?? ""
                                        textFormat: (card.modelData.markdown ?? false) ? Text.MarkdownText : Text.PlainText
                                        wrapMode: Text.Wrap
                                        clip: true
                                        color: Kirigami.Theme.textColor
                                        linkColor: Kirigami.Theme.linkColor
                                        font: Kirigami.Theme.defaultFont
                                        renderType: Text.QtRendering
                                    }
                                    Item {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        visible: fileCard.fileKind === "other" || fileCard.fileKind === "missing"

                                        ColumnLayout {
                                            anchors.centerIn: parent
                                            width: parent.width - Kirigami.Units.largeSpacing * 2
                                            spacing: Kirigami.Units.smallSpacing

                                            Kirigami.Icon {
                                                Layout.alignment: Qt.AlignHCenter
                                                source: card.modelData.icon
                                                opacity: fileCard.fileKind === "missing" ? 0.5 : 1
                                                implicitWidth: Math.min(Kirigami.Units.iconSizes.huge, card.width / 3, card.height / 3)
                                                implicitHeight: implicitWidth
                                            }
                                            Text {
                                                Layout.fillWidth: true
                                                visible: fileCard.fileKind === "missing"
                                                text: i18nc("@info a file a canvas card points to", "File not found")
                                                horizontalAlignment: Text.AlignHCenter
                                                wrapMode: Text.Wrap
                                                color: Kirigami.Theme.disabledTextColor
                                                font: Kirigami.Theme.defaultFont
                                                renderType: Text.QtRendering
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Connections: one curve each, with arrow heads and an optional label.
                    Repeater {
                        model: root.canvas.edges ?? []

                        Item {
                            id: edge

                            required property var modelData
                            readonly property color accent: root.cardColor(modelData.color)

                            anchors.fill: parent
                            z: 1

                            Shape {
                                anchors.fill: parent
                                preferredRendererType: Shape.CurveRenderer

                                ShapePath {
                                    strokeColor: edge.accent
                                    strokeWidth: 3
                                    fillColor: "transparent"
                                    capStyle: ShapePath.RoundCap
                                    startX: edge.modelData.start.x
                                    startY: edge.modelData.start.y

                                    PathCubic {
                                        x: edge.modelData.end.x
                                        y: edge.modelData.end.y
                                        control1X: edge.modelData.control1.x
                                        control1Y: edge.modelData.control1.y
                                        control2X: edge.modelData.control2.x
                                        control2Y: edge.modelData.control2.y
                                    }
                                }
                                ShapePath {
                                    strokeWidth: -1
                                    fillColor: edge.accent

                                    PathPolyline {
                                        path: edge.modelData.endArrow
                                    }
                                }
                                ShapePath {
                                    strokeWidth: -1
                                    fillColor: edge.accent

                                    PathPolyline {
                                        path: edge.modelData.startArrow
                                    }
                                }
                            }

                            Rectangle {
                                visible: edge.modelData.label.length > 0
                                x: edge.modelData.middle.x - width / 2
                                y: edge.modelData.middle.y - height / 2
                                width: Math.min(edgeLabel.implicitWidth, Kirigami.Units.gridUnit * 14) + Kirigami.Units.largeSpacing
                                height: edgeLabel.implicitHeight + Kirigami.Units.smallSpacing
                                radius: Kirigami.Units.cornerRadius
                                color: Kirigami.Theme.backgroundColor

                                Text {
                                    id: edgeLabel

                                    anchors.centerIn: parent
                                    width: Math.min(implicitWidth, Kirigami.Units.gridUnit * 14)
                                    text: edge.modelData.label
                                    textFormat: Text.PlainText
                                    elide: Text.ElideRight
                                    color: Kirigami.Theme.textColor
                                    font: Kirigami.Theme.defaultFont
                                    renderType: Text.QtRendering
                                }
                            }
                        }
                    }
                }
            }

            WheelHandler {
                acceptedModifiers: Qt.ControlModifier
                onWheel: event => root.zoomTo(root.effectiveScale * (event.angleDelta.y > 0 ? 1.15 : 1 / 1.15), point.position.x, point.position.y)
            }
        }
    }

    Kirigami.PlaceholderMessage {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.margins: Kirigami.Units.gridUnit
        visible: !root.valid
        icon.name: "dialog-error"
        text: i18nc("@info:placeholder", "This canvas cannot be shown")
        explanation: i18nc("@info:placeholder", "The file does not contain a canvas in the JSON Canvas format that Obsidian writes.")
    }

    // A canvas can come from anyone: show where a link leads before opening it.
    Kirigami.PromptDialog {
        id: linkDialog

        property url link

        function ask(target: url): void {
            link = target;
            open();
        }

        title: i18nc("@title:window", "Open Link?")
        subtitle: i18nc("@info", "This card leads to:\n%1", link.toString())
        standardButtons: Kirigami.Dialog.Cancel
        customFooterActions: [
            Kirigami.Action {
                text: i18nc("@action:button", "Open Link")
                icon.name: "internet-services"
                onTriggered: {
                    Qt.openUrlExternally(linkDialog.link);
                    linkDialog.close();
                }
            }
        ]
    }
}
