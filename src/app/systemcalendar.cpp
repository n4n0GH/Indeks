// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "systemcalendar.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QDBusConnection>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDir>
#include <QTimer>

using namespace Qt::StringLiterals;

namespace
{
/** Akonadi's kind of agent that shows a calendar file. Its agents are called by it, with a number. */
const QString FileAgent = u"akonadi_ical_resource"_s;
const QString ManagerPath = u"/AgentManager"_s;
const QString ManagerInterface = u"org.freedesktop.Akonadi.AgentManager"_s;
const QString SettingsPath = u"/Settings"_s;
const QString SettingsInterface = u"org.kde.Akonadi.ICal.Settings"_s;
const QString BusService = u"org.freedesktop.DBus"_s;
const QString BusPath = u"/org/freedesktop/DBus"_s;

/** How long an agent that was just made is waited for, in steps of AttemptMs. */
constexpr int Attempts = 80;
constexpr int AttemptMs = 250;
/** Akonadi may have to be started before it answers. */
constexpr int StartTimeoutMs = 60 * 1000;

/** Akonadi can be run more than once by one user; each run has a name that its services end in. */
QString named(const QString &service)
{
    const QString run = qEnvironmentVariable("AKONADI_INSTANCE");
    return run.isEmpty() ? service : service + u'.' + run;
}

QString control()
{
    return named(u"org.freedesktop.Akonadi.Control"_s);
}

QString resource(const QString &instance)
{
    return named(u"org.freedesktop.Akonadi.Resource."_s + instance);
}

QString agent(const QString &instance)
{
    return named(u"org.freedesktop.Akonadi.Agent."_s + instance);
}

bool failed(const QDBusMessage &reply)
{
    return reply.type() != QDBusMessage::ReplyMessage;
}

/** What is kept from one start to the next, with the rest of how the program was left. */
KConfigGroup stateGroup()
{
    return KSharedConfig::openStateConfig()->group(u"SystemCalendar"_s);
}

/**
 * Calendars that were to be removed while Akonadi was not running. It is not started to be
 * told to forget something; it is told when it runs.
 */
QStringList toForget()
{
    return stateGroup().readEntry("Forget", QStringList());
}

/** The calendar files that Akonadi showed when it was last asked. */
QStringList lastShown()
{
    return stateGroup().readEntry("Shown", QStringList());
}

void setLastShown(const QString &file, bool shown)
{
    KConfigGroup group = stateGroup();
    QStringList files = lastShown();
    if (shown == files.contains(file)) {
        return;
    }
    if (shown) {
        files.append(file);
    } else {
        files.removeAll(file);
    }
    if (files.isEmpty()) {
        group.deleteEntry("Shown");
    } else {
        group.writeEntry("Shown", files);
    }
    group.sync();
}

void setToForget(const QStringList &files)
{
    KConfigGroup group = stateGroup();
    if (files.isEmpty()) {
        group.deleteEntry("Forget");
    } else {
        group.writeEntry("Forget", files);
    }
    group.sync();
}
}

SystemCalendar::SystemCalendar(QObject *parent)
    : QObject(parent)
{
}

void SystemCalendar::call(const QString &service,
                          const QString &path,
                          const QString &interface,
                          const QString &method,
                          const QVariantList &arguments,
                          const Reply &then,
                          int timeoutMs)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        if (then) {
            QTimer::singleShot(0, this, [then] {
                then(QDBusMessage::createError(QDBusError::Disconnected, QString()));
            });
        }
        return;
    }
    QDBusMessage message = QDBusMessage::createMethodCall(service, path, interface, method);
    message.setArguments(arguments);
    auto watcher = new QDBusPendingCallWatcher(bus.asyncCall(message, timeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [watcher, then] {
        watcher->deleteLater();
        if (then) {
            then(watcher->reply());
        }
    });
}

void SystemCalendar::look(const std::function<void(bool, bool)> &then)
{
    call(BusService, BusPath, BusService, u"NameHasOwner"_s, {control()}, [this, then](const QDBusMessage &reply) {
        if (!failed(reply) && reply.arguments().value(0).toBool()) {
            then(true, true);
            return;
        }
        call(BusService, BusPath, BusService, u"ListActivatableNames"_s, {}, [then](const QDBusMessage &reply) {
            then(false, !failed(reply) && reply.arguments().value(0).toStringList().contains(control()));
        });
    });
}

void SystemCalendar::find(const QString &file, const std::function<void(const QString &)> &then)
{
    call(
        control(),
        ManagerPath,
        ManagerInterface,
        u"agentInstances"_s,
        {},
        [this, file, then](const QDBusMessage &reply) {
            QStringList instances;
            if (!failed(reply)) {
                const QStringList all = reply.arguments().value(0).toStringList();
                for (const QString &instance : all) {
                    if (instance.startsWith(FileAgent)) {
                        instances.append(instance);
                    }
                }
            }
            probe(file, instances, then);
        },
        StartTimeoutMs);
}

void SystemCalendar::probe(const QString &file, const QStringList &instances, const std::function<void(const QString &)> &then)
{
    if (instances.isEmpty()) {
        then(QString());
        return;
    }
    const QString instance = instances.constFirst();
    call(
        resource(instance),
        SettingsPath,
        SettingsInterface,
        u"path"_s,
        {},
        [this, file, instances, instance, then](const QDBusMessage &reply) {
            if (!failed(reply) && QDir::cleanPath(reply.arguments().value(0).toString()) == file) {
                then(instance);
            } else {
                probe(file, instances.mid(1), then);
            }
        },
        3000);
}

void SystemCalendar::setCalendar(const QString &file, const QString &name)
{
    const QString cleaned = file.isEmpty() ? QString() : QDir::cleanPath(file);
    if (cleaned == m_file && name == m_name) {
        return;
    }
    if (cleaned != m_file) {
        m_problem.clear();
    }
    m_file = cleaned;
    m_name = name;
    refresh();
}

void SystemCalendar::refresh()
{
    const quint64 ticket = ++m_ticket;
    const QString file = m_file;
    const QString name = m_name;
    const auto apply = [this](bool available, bool shown) {
        if (available != m_available || shown != m_shown) {
            m_available = available;
            m_shown = shown;
            Q_EMIT changed();
        }
    };
    look([this, ticket, file, name, apply](bool running, bool startable) {
        if (ticket != m_ticket) {
            return;
        }
        if (!running) {
            // Not started to be asked: as it was when it was last asked.
            apply(startable, startable && !file.isEmpty() && lastShown().contains(file));
            return;
        }
        // What was to be forgotten while Akonadi was not there to be told.
        QStringList forget = toForget();
        if (!forget.isEmpty()) {
            setToForget({});
            forget.removeAll(file);
            for (const QString &old : std::as_const(forget)) {
                remove(old);
            }
        }
        if (file.isEmpty()) {
            apply(true, false);
            return;
        }
        find(file, [this, ticket, file, name, apply](const QString &instance) {
            if (ticket != m_ticket) {
                return;
            }
            setLastShown(file, !instance.isEmpty());
            apply(true, !instance.isEmpty());
            if (instance.isEmpty() || name.isEmpty()) {
                return;
            }
            // The database may have been given another name since.
            call(control(), ManagerPath, ManagerInterface, u"agentInstanceName"_s, {instance}, [this, instance, name](const QDBusMessage &reply) {
                if (!failed(reply) && reply.arguments().value(0).toString() != name) {
                    rename(instance, name);
                }
            });
        });
    });
}

void SystemCalendar::add()
{
    if (m_file.isEmpty() || isBusy()) {
        return;
    }
    const QString file = m_file;
    const QString name = m_name;
    m_problem.clear();
    m_adding = true;
    setBusy(true);
    QStringList forget = toForget();
    if (forget.removeAll(file) > 0) {
        setToForget(forget);
    }
    look([this, file, name](bool running, bool startable) {
        if (!running && !startable) {
            finish(i18nc("@info", "There is no system calendar to add the to-dos to. Calendar programs can show the calendar file all the same."));
            return;
        }
        // Asking for its agents is what starts Akonadi if it is not running.
        find(file, [this, file, name](const QString &instance) {
            if (instance.isEmpty()) {
                create(file, name);
            } else {
                configure(instance, file, name, 0);
            }
        });
    });
}

void SystemCalendar::create(const QString &file, const QString &name)
{
    call(
        control(),
        ManagerPath,
        ManagerInterface,
        u"createAgentInstance"_s,
        {FileAgent},
        [this, file, name](const QDBusMessage &reply) {
            const QString instance = failed(reply) ? QString() : reply.arguments().value(0).toString();
            if (instance.isEmpty()) {
                finish(failed(reply) ? i18nc("@info %1 is what the system said", "The system calendar could not be reached: %1", reply.errorMessage())
                                     : i18nc("@info", "The system calendar cannot show calendar files: what it needs for that is not installed."));
                return;
            }
            configure(instance, file, name, 0);
        },
        StartTimeoutMs);
}

void SystemCalendar::configure(const QString &instance, const QString &file, const QString &name, int attempt)
{
    call(
        resource(instance),
        SettingsPath,
        SettingsInterface,
        u"setPath"_s,
        {file},
        [this, instance, file, name, attempt](const QDBusMessage &reply) {
            if (failed(reply)) {
                // An agent that was just made takes a moment until it can be talked to.
                if (attempt < Attempts) {
                    QTimer::singleShot(AttemptMs, this, [this, instance, file, name, attempt] {
                        configure(instance, file, name, attempt + 1);
                    });
                    return;
                }
                // Not to be left behind as a calendar of nothing.
                call(control(), ManagerPath, ManagerInterface, u"removeAgentInstance"_s, {instance}, {});
                finish(i18nc("@info %1 is what the system said", "The system calendar did not take the calendar file: %1", reply.errorMessage()));
                return;
            }
            // The file is Indeks' to write: the calendar programs are to show it, and to
            // look at it again whenever it changes, but not to write into it.
            const QString service = resource(instance);
            call(service, SettingsPath, SettingsInterface, u"setDisplayName"_s, {name}, {});
            call(service, SettingsPath, SettingsInterface, u"setReadOnly"_s, {true}, {});
            call(service, SettingsPath, SettingsInterface, u"setMonitorFile"_s, {true}, {});
            call(service, SettingsPath, SettingsInterface, u"save"_s, {}, [this, instance, name](const QDBusMessage &) {
                call(agent(instance), u"/"_s, u"org.freedesktop.Akonadi.Agent.Control"_s, u"reconfigure"_s, {}, [this, instance, name](const QDBusMessage &) {
                    call(control(), ManagerPath, ManagerInterface, u"setAgentInstanceName"_s, {instance, name}, [this, instance](const QDBusMessage &) {
                        call(control(), ManagerPath, ManagerInterface, u"agentInstanceSynchronize"_s, {instance}, {});
                        finish();
                    });
                });
            });
        },
        3000);
}

void SystemCalendar::rename(const QString &instance, const QString &name)
{
    const QString service = resource(instance);
    call(service, SettingsPath, SettingsInterface, u"setDisplayName"_s, {name}, {});
    call(service, SettingsPath, SettingsInterface, u"save"_s, {}, [this, instance, name](const QDBusMessage &) {
        call(agent(instance), u"/"_s, u"org.freedesktop.Akonadi.Agent.Control"_s, u"reconfigure"_s, {}, {});
        call(control(), ManagerPath, ManagerInterface, u"setAgentInstanceName"_s, {instance, name}, {});
    });
}

void SystemCalendar::remove(const QString &file, const std::function<void()> &then)
{
    const QString cleaned = QDir::cleanPath(file);
    const auto done = [this, then] {
        setBusy(false);
        if (then) {
            then();
        }
        refresh();
    };
    if (file.isEmpty()) {
        if (then) {
            then();
        }
        return;
    }
    setBusy(true);
    look([this, cleaned, done](bool running, bool startable) {
        if (!running) {
            if (startable) {
                QStringList forget = toForget();
                if (!forget.contains(cleaned)) {
                    forget.append(cleaned);
                    setToForget(forget);
                }
                setLastShown(cleaned, false);
            }
            done();
            return;
        }
        find(cleaned, [this, done](const QString &instance) {
            if (instance.isEmpty()) {
                done();
                return;
            }
            call(control(), ManagerPath, ManagerInterface, u"removeAgentInstance"_s, {instance}, [done](const QDBusMessage &) {
                done();
            });
        });
    });
}

void SystemCalendar::finish(const QString &problem)
{
    m_problem = problem;
    m_adding = false;
    setBusy(false);
    refresh();
}

void SystemCalendar::setBusy(bool busy)
{
    m_busy = qMax(0, m_busy + (busy ? 1 : -1));
    Q_EMIT changed();
}

#include "moc_systemcalendar.cpp"
