// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The documents a document is linked to: what belongs together with it although it is a file
// of its own. Shows the links there are, takes them away, and finds documents to add. A link
// has no direction: it shows on both documents.
Kirigami.Dialog {
    id: root

    property real documentId: 0
    property string documentName
    property var links: []

    readonly property var linkedIds: links.map(link => link.id)
    /** What is typed finds documents; nothing typed, nothing looked for. */
    readonly property bool searching: searchField.text.trim().length > 0

    function ask(id: real): void {
        const info = App.library.documentInfo(id);
        if (!(info.valid ?? false)) {
            return;
        }
        documentId = id;
        documentName = info.name;
        searchField.text = "";
        reload();
        open();
    }

    function reload(): void {
        links = App.library.linkedDocuments(documentId);
    }

    function search(): void {
        typingPause.stop();
        const text = searchField.text.trim();
        if (text.length === 0) {
            App.linkCandidates.active = false;
            App.linkCandidates.filter = "";
        } else {
            App.linkCandidates.filter = text;
            App.linkCandidates.active = true;
        }
    }

    function link(otherId: real): void {
        App.library.linkDocuments([documentId, otherId]);
        reload();
        searchField.forceActiveFocus();
    }

    function unlink(otherId: real): void {
        App.library.unlinkDocuments(documentId, otherId);
        reload();
        // The button that was clicked is gone, and the focus would be gone with it.
        searchField.forceActiveFocus();
    }

    // Not in ask(): a dialog that closes at the same moment would take the focus back.
    onOpened: searchField.forceActiveFocus()
    onClosed: {
        // The list of what was found is switched off again: nobody looks at it.
        searchField.text = "";
        search();
    }

    title: i18nc("@title:window", "Linked Documents")
    padding: Kirigami.Units.largeSpacing
    preferredWidth: Kirigami.Units.gridUnit * 32
    standardButtons: Kirigami.Dialog.Close

    Connections {
        target: App.library
        enabled: root.visible
        function onChanged(): void {
            root.reload();
        }
    }

    component DocumentEntry: RowLayout {
        property alias iconName: entryIcon.source
        property alias name: entryName.text
        property alias detail: entryDetail.text

        spacing: Kirigami.Units.smallSpacing

        Kirigami.Icon {
            id: entryIcon

            implicitWidth: Kirigami.Units.iconSizes.smallMedium
            implicitHeight: implicitWidth
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            QQC2.Label {
                id: entryName

                Layout.fillWidth: true
                textFormat: Text.PlainText
                elide: Text.ElideMiddle
            }
            QQC2.Label {
                id: entryDetail

                Layout.fillWidth: true
                visible: text.length > 0
                textFormat: Text.PlainText
                elide: Text.ElideMiddle
                font: Kirigami.Theme.smallFont
                color: Kirigami.Theme.disabledTextColor
            }
        }
    }

    ColumnLayout {
        spacing: Kirigami.Units.smallSpacing

        QQC2.Label {
            Layout.fillWidth: true
            textFormat: Text.PlainText
            elide: Text.ElideMiddle
            text: root.links.length > 0 ? i18nc("@label %1 is the name of a document", "“%1” is linked to:", root.documentName)
                                        : i18nc("@label %1 is the name of a document", "“%1” is not linked to any document yet.", root.documentName)
        }

        QQC2.ScrollView {
            Layout.fillWidth: true
            // As high as its entries, up to a few of them; more are scrolled to.
            Layout.preferredHeight: Math.min(linkList.contentHeight, Kirigami.Units.gridUnit * 9)
            visible: root.links.length > 0

            ListView {
                id: linkList

                clip: true
                model: root.links

                delegate: QQC2.ItemDelegate {
                    id: linkEntry

                    required property var modelData

                    width: ListView.view.width
                    // The buttons are what is clicked here, not the row.
                    hoverEnabled: false
                    down: false

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.smallSpacing

                        DocumentEntry {
                            Layout.fillWidth: true
                            iconName: linkEntry.modelData.icon
                            name: linkEntry.modelData.name
                            detail: linkEntry.modelData.trashed ? i18nc("@info where a linked document is", "In the trash")
                                                                : linkEntry.modelData.location
                        }
                        QQC2.ToolButton {
                            icon.name: "go-jump"
                            text: i18nc("@action:button", "Show in the List")
                            display: QQC2.AbstractButton.IconOnly
                            onClicked: {
                                const id = linkEntry.modelData.id;
                                root.close();
                                App.showDocument(id);
                            }
                            QQC2.ToolTip.text: text
                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                        }
                        QQC2.ToolButton {
                            icon.name: "remove-link"
                            text: i18nc("@action:button", "Remove Link")
                            display: QQC2.AbstractButton.IconOnly
                            onClicked: root.unlink(linkEntry.modelData.id)
                            QQC2.ToolTip.text: text
                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                        }
                    }
                }
            }
        }

        Kirigami.Separator {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.smallSpacing
            Layout.bottomMargin: Kirigami.Units.smallSpacing
        }

        Kirigami.SearchField {
            id: searchField

            Layout.fillWidth: true
            placeholderText: i18nc("@info:placeholder", "Find a document to link…")
            // As in the search field of the window: it looks by itself a moment after the
            // last key, and a single letter waits for a real pause.
            autoAccept: false
            onAccepted: root.search()
            onTextChanged: {
                if (text.trim().length === 0) {
                    root.search();
                } else {
                    typingPause.interval = text.trim().length < 2 ? Kirigami.Units.humanMoment / 2 : Kirigami.Units.humanMoment / 8;
                    typingPause.restart();
                }
            }

            Timer {
                id: typingPause

                onTriggered: root.search()
            }
        }

        QQC2.ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.gridUnit * 12

            ListView {
                id: candidateList

                clip: true
                model: App.linkCandidates

                delegate: QQC2.ItemDelegate {
                    id: candidate

                    required property real documentId
                    required property string name
                    required property string iconName
                    required property string location

                    readonly property bool itself: documentId === root.documentId
                    readonly property bool linked: root.linkedIds.includes(documentId)

                    width: ListView.view.width
                    enabled: !itself
                    onClicked: {
                        if (!linked) {
                            root.link(documentId);
                        }
                    }

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.smallSpacing

                        DocumentEntry {
                            Layout.fillWidth: true
                            iconName: candidate.iconName
                            name: candidate.name
                            detail: candidate.itself ? i18nc("@info the document whose links are shown", "This is the document itself")
                                                     : candidate.location
                        }
                        Kirigami.Icon {
                            visible: candidate.linked
                            source: "checkmark"
                            implicitWidth: Kirigami.Units.iconSizes.small
                            implicitHeight: implicitWidth
                        }
                        QQC2.Label {
                            visible: candidate.linked
                            text: i18nc("@info the document is linked already", "Linked")
                            color: Kirigami.Theme.disabledTextColor
                        }
                        QQC2.Button {
                            visible: !candidate.linked && !candidate.itself
                            icon.name: "insert-link"
                            text: i18nc("@action:button", "Link")
                            onClicked: root.link(candidate.documentId)
                        }
                    }
                }

                QQC2.BusyIndicator {
                    anchors.centerIn: parent
                    visible: root.searching && App.linkCandidates.loading && candidateList.count === 0
                    running: visible
                }
                Kirigami.PlaceholderMessage {
                    anchors.centerIn: parent
                    width: parent.width - Kirigami.Units.gridUnit * 2
                    visible: candidateList.count === 0 && !(root.searching && App.linkCandidates.loading)
                    icon.name: root.searching ? "edit-none" : "search"
                    text: root.searching ? i18nc("@info:placeholder", "No document found") : ""
                    explanation: root.searching ? ""
                                                : i18nc("@info:placeholder", "Type part of a name or words from a document. The filters of the search field work here as well, tag:name or kind:pdf for instance.")
                }
            }
        }

        QQC2.Label {
            Layout.fillWidth: true
            visible: App.linkCandidates.truncated
            wrapMode: Text.WordWrap
            font: Kirigami.Theme.smallFont
            color: Kirigami.Theme.disabledTextColor
            text: i18nc("@info", "There are more documents than are listed. Type more to narrow them down.")
        }
    }
}
