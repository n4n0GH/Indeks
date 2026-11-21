// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

FormCard.FormCardPage {
    id: root

    title: i18nc("@title:window", "To-Do List")

    FormCard.FormHeader {
        title: i18nc("@title:group", "Deadlines")
    }
    FormCard.FormCard {
        FormCard.FormSpinBoxDelegate {
            label: i18nc("@label:spinbox", "Highlight to-dos that are due within")
            from: 0
            to: 365
            value: Config.todoWarningDays
            textFromValue: (value, locale) => value === 0 ? i18nc("@item:valuesuffix to-dos are only highlighted on the day they are due", "On the day only")
                : i18ncp("@item:valuesuffix how many days before it is due a to-do is highlighted", "%1 day", "%1 days", value)
            valueFromText: (text, locale) => {
                const number = parseInt(text);
                return isNaN(number) ? 0 : number;
            }
            onValueModified: {
                Config.todoWarningDays = value;
                App.applySettings();
            }
        }
    }
    FormCard.FormSectionText {
        text: i18nc("@info", "A to-do that is due today or within this many days is shown in orange in the list. One that is past its day is shown in red, whatever is set here.")
    }
}
