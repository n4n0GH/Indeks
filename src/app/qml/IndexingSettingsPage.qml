// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

FormCard.FormCardPage {
    title: i18nc("@title:window", "Indexing")

    FormCard.FormHeader {
        title: i18nc("@title:group", "What to index")
    }
    FormCard.FormCard {
        FormCard.FormSwitchDelegate {
            text: i18nc("@option:check", "Index the content of files")
            description: i18nc("@info", "Makes the text inside documents searchable. Without this, documents are found by name, tags and notes only.")
            checked: Config.indexContent
            onToggled: {
                Config.indexContent = checked;
                App.applySettings();
            }
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormSwitchDelegate {
            text: i18nc("@option:check", "Include hidden files and folders")
            checked: Config.indexHidden
            onToggled: {
                Config.indexHidden = checked;
                App.applySettings();
            }
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormSwitchDelegate {
            text: i18nc("@option:check", "Read old Office documents with LibreOffice")
            description: App.library.officeViewAvailable
                ? i18nc("@info", "Word, Excel and PowerPoint files in the formats from before 2007 cannot be read otherwise. This takes about a second per document.")
                : i18nc("@info", "LibreOffice is not installed. Without it, Word, Excel and PowerPoint files in the formats from before 2007 are found by name only.")
            enabled: App.library.officeViewAvailable
            checked: Config.officeConversion
            onToggled: {
                Config.officeConversion = checked;
                App.applySettings();
            }
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormSpinBoxDelegate {
            label: i18nc("@label:spinbox", "Files read at the same time")
            description: i18nc("@info", "Files are read by helper programs, several at once. A file one of them cannot cope with does not hold up the others; it goes into the index by its name.")
            from: 0
            to: Math.min(16, App.library.processorCount)
            value: Config.indexJobs
            // Nought leaves the choice to Indeks.
            textFromValue: (value, locale) => value === 0 ? i18nc("@item:valuesuffix number of files read at once, chosen automatically", "Automatic (%1)", App.library.indexDefaultJobs) : Number(value).toLocaleString(locale, "f", 0)
            valueFromText: (text, locale) => {
                const number = Number.fromLocaleString(locale, text);
                return isNaN(number) ? 0 : number;
            }
            onValueModified: {
                Config.indexJobs = value;
                App.applySettings();
            }
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormSpinBoxDelegate {
            label: i18nc("@label:spinbox", "Read content of files up to (MB)")
            from: 1
            to: 4096
            value: Config.maxContentSizeMb
            onValueModified: {
                Config.maxContentSizeMb = value;
                App.applySettings();
            }
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormSpinBoxDelegate {
            label: i18nc("@label:spinbox", "Download remote files up to (MB)")
            description: i18nc("@info", "Larger files on servers are indexed by name only.")
            from: 1
            to: 4096
            value: Config.maxRemoteSizeMb
            onValueModified: {
                Config.maxRemoteSizeMb = value;
                App.applySettings();
            }
        }
    }

    FormCard.FormHeader {
        title: i18nc("@title:group", "When to index")
    }
    FormCard.FormCard {
        FormCard.FormSwitchDelegate {
            text: i18nc("@option:check", "Look for changes when Indeks starts")
            description: i18nc("@info", "Folders on this computer are also watched while Indeks runs.")
            checked: Config.rescanOnStart
            onToggled: {
                Config.rescanOnStart = checked;
                App.applySettings();
            }
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormButtonDelegate {
            text: i18nc("@action:button", "Update Index Now")
            icon.name: "view-refresh"
            onClicked: App.action("rescan").trigger()
        }
    }
}
