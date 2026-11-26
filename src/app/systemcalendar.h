// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDBusMessage>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <functional>

/**
 * Puts a calendar file among the calendars of the system: those of Akonadi, which
 * KOrganizer, Merkuro and the calendar of the Plasma clock show.
 *
 * Akonadi shows a calendar file through an agent of its own (akonadi_ical_resource) that
 * reads the file and follows its changes. Such an agent is made, told the file and removed
 * here by talking to Akonadi over D-Bus, which takes no library of Akonadi's: where there
 * is no Akonadi, there is nothing to talk to, and this says that it is not available.
 *
 * Whether the file is shown is found out by asking the agents for calendar files which
 * file they have, so it stays right when the calendar is removed in a calendar program.
 * Akonadi is not started to be asked: while it does not run, what was found the last time
 * is taken to be still so.
 *
 * Everything here is asked without waiting for the answer; `changed` says when something
 * is known.
 */
class SystemCalendar : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by App")
    /** There is a system calendar to add to. It may have to be started first. */
    Q_PROPERTY(bool available READ isAvailable NOTIFY changed)
    /** The file is one of its calendars, or is just being made one. */
    Q_PROPERTY(bool shown READ isShown NOTIFY changed)
    /** The file is being added or removed. */
    Q_PROPERTY(bool busy READ isBusy NOTIFY changed)
    /** Why adding it did not work; empty if it did. */
    Q_PROPERTY(QString problem READ problem NOTIFY changed)
public:
    explicit SystemCalendar(QObject *parent = nullptr);

    bool isAvailable() const
    {
        return m_available;
    }
    bool isShown() const
    {
        return m_shown || m_adding;
    }
    bool isBusy() const
    {
        return m_busy > 0;
    }
    QString problem() const
    {
        return m_problem;
    }

    /**
     * The file this is about and what its calendar is to be called; an empty @p file for
     * none. A calendar that is shown under another name is renamed.
     */
    void setCalendar(const QString &file, const QString &name);
    /** Finds out anew how things are. */
    void refresh();
    /** Has the file shown as a calendar. Starts Akonadi if it is not running. */
    void add();
    /**
     * Takes the calendar of @p file away again, the file itself stays, and calls @p then
     * when that is done or cannot be done. Akonadi is not started for this: what it does not
     * show while it is not running, it is asked to forget the next time (see refresh()).
     */
    void remove(const QString &file, const std::function<void()> &then = {});

Q_SIGNALS:
    void changed();

private:
    using Reply = std::function<void(const QDBusMessage &)>;
    void call(const QString &service, const QString &path, const QString &interface, const QString &method, const QVariantList &arguments, const Reply &then, int timeoutMs = 5000);
    /** Whether Akonadi runs, and if not, whether it can be started. */
    void look(const std::function<void(bool running, bool startable)> &then);
    /** The agent that shows @p file, or nothing. Only to be asked while Akonadi runs. */
    void find(const QString &file, const std::function<void(const QString &instance)> &then);
    void probe(const QString &file, const QStringList &instances, const std::function<void(const QString &instance)> &then);
    void create(const QString &file, const QString &name);
    /** Waits for the agent that was made to answer, and tells it the file. */
    void configure(const QString &instance, const QString &file, const QString &name, int attempt);
    void rename(const QString &instance, const QString &name);
    void finish(const QString &problem = QString());
    void setBusy(bool busy);

    QString m_file;
    QString m_name;
    QString m_problem;
    bool m_available = false;
    bool m_shown = false;
    bool m_adding = false;
    int m_busy = 0;
    quint64 m_ticket = 0; ///< goes up with every refresh: answers to an older one do not count
};
