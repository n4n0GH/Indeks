// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "types.h"

#include <QElapsedTimer>
#include <QObject>
#include <QSet>
#include <QThread>
#include <QVariantMap>

#include <atomic>

namespace Indeks
{

/** Work for the thread that runs scripts. */
struct ScriptRequest {
    quint64 ticket = 0;
    /** The script to run on `documents`: the name of its file. */
    QString script;
    /**
     * With `trial` set, the text to run instead of what is stored: for trying out what is in
     * the editor. A trial changes nothing; it only reports what the script would do.
     */
    QString source;
    bool trial = false;
    /** Rules to apply, in this order, instead of running one script. */
    QList<qint64> rules;
    /** The documents to run on. For rules: the ones to look at; empty for all. */
    QList<qint64> documents;
};

/** Lives in the thread that runs scripts; see ScriptHost. */
class ScriptWorker : public QObject
{
    Q_OBJECT
public:
    explicit ScriptWorker(const std::atomic<quint64> *cancelUpTo);

public Q_SLOTS:
    void run(const Indeks::ScriptRequest &request);
    /** Closes this thread's connection to the index. */
    void close();

Q_SIGNALS:
    void progressed(quint64 ticket, const QString &script, const QString &document, int done, int total);
    void documentsChanged(const QList<qint64> &ids);
    void reported(const QVariantMap &report);
    void finished(quint64 ticket, int documents, bool took);

private:
    bool cancelled(quint64 ticket) const;
    int applyRule(const ScriptRequest &request, qint64 ruleId, bool *took);
    void runScript(quint64 ticket, const QString &file, const QString &source, bool trial, const QList<qint64> &documents, const QString &rule, bool *took);
    void noteChanged(const QList<qint64> &ids, bool now = false);

    const std::atomic<quint64> *const m_cancelUpTo;
    QList<qint64> m_changed;
    QElapsedTimer m_sinceChanged;
};

/**
 * Runs the scripts stored in a database, and the rules that use them, away from the user
 * interface.
 *
 * A script is Python, run by the Python of the system in a process of its own and inside a
 * sandbox: no network, nothing to write to, and of the files only the system's and the
 * documents themselves to read. It is handed the documents one by one and answers with what it
 * wants done to each (see Rules); doing it is left to Indeks, so a script can do no more than
 * a rule can. A script that hangs is stopped after a time limit, one that crashes takes nothing
 * with it.
 *
 * One request is worked on at a time, in the order they were made: a rule that looks at what
 * an earlier one did has to come after it.
 */
class ScriptHost : public QObject
{
    Q_OBJECT
public:
    explicit ScriptHost(QObject *parent = nullptr);
    ~ScriptHost() override;

    /** Why scripts cannot run on this computer, in words for the user; empty if they can. */
    static QString problem();
    /** How long a script may take over one document, in milliseconds. */
    static void setTimeLimit(int ms);

    quint64 runScript(const QString &file, const QList<qint64> &documents);
    quint64 tryScript(const QString &name, const QString &source, const QList<qint64> &documents);
    quint64 applyRules(const QList<qint64> &rules, const QList<qint64> &documents);

    /** Calls off everything that runs or waits. Reports still come, saying so. */
    void cancel();
    /**
     * Calls everything off and has the thread let go of the index, before another database is
     * opened or the program ends. Returns false if the thread did not come to rest in time.
     */
    bool rest(int timeoutMs);
    /** Ends the thread for good. */
    bool shutdown(int timeoutMs);

    bool isBusy() const
    {
        return !m_pending.isEmpty();
    }
    /** What is running, for the status display; empty if nothing is. */
    QString status() const;
    QString statusDetail() const;
    double progress() const;
    /** The same as a line for the list of activity. */
    QueueItem queueItem() const;

Q_SIGNALS:
    void busyChanged();
    void statusChanged();
    void documentsChanged(const QList<qint64> &ids);
    /**
     * A script was run, by itself or for a rule. The map holds
     *   ticket     the request it belongs to
     *   script     the file name, `name` the same for showing
     *   rule       the name of the rule it ran for, if any
     *   trial      whether this only showed what would be done
     *   time       when it ended
     *   documents  how many documents it was run on
     *   changed    for how many of them it asked for something
     *   failed     on how many it raised an error
     *   stopped    whether it was called off
     *   problem    why it did not run or did not get through, in words for the user
     *   entries    for documents with something to tell: `id`, `name`, `output` (what the
     *              script printed), `error`, and `actions` (what it asked for)
     *   more       how many such documents are left out to keep this short
     */
    void reported(const QVariantMap &report);
    /** A request is done with. @p took tells that something was taken off documents. */
    void finished(quint64 ticket, int documents, bool took);

    // To the thread.
    void requested(const Indeks::ScriptRequest &request);

private:
    quint64 submit(ScriptRequest request);

    QThread m_thread;
    ScriptWorker *m_worker = nullptr;
    std::atomic<quint64> m_cancelUpTo = 0;
    quint64 m_lastTicket = 0;
    QSet<quint64> m_pending;
    QString m_script;
    QString m_document;
    int m_done = 0;
    int m_total = 0;
};

}

Q_DECLARE_METATYPE(Indeks::ScriptRequest)
