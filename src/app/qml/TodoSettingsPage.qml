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

    // The to-dos that have a deadline as a calendar file, and that file among the calendars
    // of the system. Both are commands (see App), so that the command bar has them too;
    // the switches say how things are, which may be a moment after they were clicked.
    FormCard.FormHeader {
        title: i18nc("@title:group", "Calendar")
    }
    FormCard.FormCard {
        FormCard.FormSwitchDelegate {
            id: keep

            text: i18nc("@option:check", "Keep the to-dos that have a deadline in a calendar")
            description: i18nc("@info %1 is the name of the database that is open", "For the database “%1”. A to-do is in the calendar on the day it is due, and leaves it when it is done.", App.databaseName)
            checked: App.todoCalendar
            onToggled: {
                App.action("todos_calendar")?.trigger();
                checked = Qt.binding(() => App.todoCalendar);
            }
        }
        FormCard.FormDelegateSeparator {
            visible: system.visible
        }
        FormCard.FormSwitchDelegate {
            id: system

            readonly property SystemCalendar calendar: App.systemCalendar

            visible: App.todoCalendar && calendar.available
            enabled: !calendar.busy
            text: i18nc("@option:check", "Show the calendar in the system calendar")
            description: calendar.problem !== "" ? calendar.problem
                : calendar.busy ? i18nc("@info", "Talking to the system calendar…")
                : calendar.shown ? i18nc("@info %1 is the name of a calendar", "KOrganizer, Merkuro and the calendar of the clock in the panel have it as “%1”.", App.todoCalendarName)
                : i18nc("@info", "Adds the calendar to those that KOrganizer, Merkuro and the calendar of the clock in the panel show.")
            checked: calendar.shown
            onToggled: {
                App.action("todos_calendar_system")?.trigger();
                checked = Qt.binding(() => calendar.shown);
            }
        }
        FormCard.FormDelegateSeparator {
            visible: file.visible
        }
        FormCard.FormButtonDelegate {
            id: file

            visible: App.todoCalendar
            text: i18nc("@action:button", "Show the Calendar File")
            description: App.todoCalendarProblem !== "" ? App.todoCalendarProblem : App.todoCalendarFile
            icon.name: "system-file-manager"
            onClicked: App.action("todos_calendar_file")?.trigger()
        }
    }
    FormCard.FormSectionText {
        text: i18nc("@info", "The calendar is a file in the folder of the database. Any calendar program can show it: add the file there as a calendar. Indeks writes the file anew whenever the to-dos change, so what is changed in a calendar program does not last. Change and finish to-dos in Indeks.")
    }
}
