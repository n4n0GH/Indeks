// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

FormCard.FormCardPage {
    id: root

    readonly property list<string> installed: App.library.ocrLanguages
    property list<string> chosen: Config.ocrLanguages.split("+").filter(language => language.length > 0)

    function toggle(language: string, on: bool): void {
        let languages = chosen.filter(entry => entry !== language);
        if (on) {
            languages.push(language);
        }
        chosen = languages;
        Config.ocrLanguages = languages.join("+");
        App.applySettings();
    }

    title: i18nc("@title:window", "Text Recognition")

    FormCard.FormHeader {
        title: i18nc("@title:group", "Recognizing text in images and scans")
    }
    FormCard.FormCard {
        FormCard.FormTextDelegate {
            visible: !App.library.ocrAvailable
            icon.name: "dialog-warning"
            text: i18nc("@info", "Tesseract is not installed")
            description: i18nc("@info", "Text recognition uses the program “tesseract”. Install it with your distribution's package manager, together with the data for the languages of your documents.")
        }
        FormCard.FormSwitchDelegate {
            enabled: App.library.ocrAvailable
            text: i18nc("@option:check", "Recognize text in images and scanned PDF documents")
            description: i18nc("@info", "Runs in the background after indexing. The recognized text is stored in the index; the files stay as they are.")
            checked: Config.ocrEnabled
            onToggled: {
                Config.ocrEnabled = checked;
                App.applySettings();
            }
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormSpinBoxDelegate {
            enabled: App.library.ocrAvailable && Config.ocrEnabled
            label: i18nc("@label:spinbox", "Pages read at the same time")
            description: i18nc("@info", "Each uses one processor core while text is being recognized, at low priority. More is faster; fewer leaves more of the computer to other things.")
            from: 0
            to: App.library.processorCount
            value: Config.ocrJobs
            // Nought leaves the choice to Indeks, which takes most of the cores.
            textFromValue: (value, locale) => value === 0 ? i18nc("@item:valuesuffix number of pages read at once, chosen automatically", "Automatic (%1)", App.library.ocrDefaultJobs) : Number(value).toLocaleString(locale, "f", 0)
            valueFromText: (text, locale) => {
                const number = Number.fromLocaleString(locale, text);
                return isNaN(number) ? 0 : number;
            }
            onValueModified: {
                Config.ocrJobs = value;
                App.applySettings();
            }
        }
    }

    FormCard.FormHeader {
        visible: App.library.ocrAvailable
        title: i18nc("@title:group", "Languages to look for")
    }
    FormCard.FormCard {
        visible: App.library.ocrAvailable

        FormCard.FormTextDelegate {
            visible: root.installed.length === 0
            icon.name: "dialog-warning"
            text: i18nc("@info", "No language data is installed for Tesseract")
        }
        Repeater {
            model: root.installed

            FormCard.FormCheckDelegate {
                required property string modelData

                enabled: Config.ocrEnabled
                text: modelData
                checked: root.chosen.includes(modelData)
                onToggled: root.toggle(modelData, checked)
            }
        }
    }

    FormCard.FormSectionText {
        visible: App.library.ocrAvailable
        text: i18nc("@info", "Languages are listed by Tesseract's codes, for example “eng” for English or “deu” for German. More can be installed as packages named like tesseract-ocr-traineddata-deu. Choosing fewer languages makes recognition faster and more accurate.")
    }
}
