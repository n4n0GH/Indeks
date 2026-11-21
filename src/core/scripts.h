// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

/**
 * The scripts of a database: Python files in the folder `scripts` inside the database folder,
 * so that they travel with the database and can be written in any editor.
 *
 * A database that is not this installation's own may be shared, on a NAS for instance, and
 * then anyone who can write to it could have a script run on this computer. Scripts of such a
 * database therefore only run once their text, exactly as it is, was allowed here: by saving
 * it in Indeks on this computer, or by saying so after reading it. The list of what was
 * allowed is kept with the settings of this computer, never in the database.
 */
namespace Indeks::Scripts
{
struct Script {
    QString file; ///< the file name, which is what rules know a script by
    QString name; ///< the same without ".py", for showing
    QString description; ///< the first line of the text the script begins with, if it has one
    QDateTime modified;
    bool allowed = false; ///< may run on this computer
};

/** The folder the scripts of the open database are in. It need not exist. */
QString dir();

/** Whether @p file can be the name of a script file: something.py, in no other folder. */
bool isFileName(const QString &file);
/** The file name for a script the user calls @p name; empty if the name will not do. */
QString fileNameFor(const QString &name);

QList<Script> list();
/** The text of a script; false if there is no such script or it cannot be read. */
bool read(const QString &file, QString *source);
/** Writes a script, new or not, and allows it on this computer. */
bool write(const QString &file, const QString &source, QString *problem = nullptr);
bool rename(const QString &file, const QString &newFile, QString *problem = nullptr);
/** Moves the script file to the system's trash. */
bool remove(const QString &file);

/** What the script says it does: the first line of its docstring. */
QString description(const QString &source);

/** Whether scripts of the open database have to be allowed here before they run. */
bool needApproval();
bool isAllowed(const QString &source);
void allow(const QString &source);

/** What a new script starts out as. */
QString starter();
}
