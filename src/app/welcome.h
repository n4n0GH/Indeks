// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QUrl>
#include <QVariantMap>
#include <QtQmlIntegration/qqmlintegration.h>

/**
 * What a new installation is asked before anything is made: whether to start a new
 * database, and where, or to open one that exists. WelcomeWindow.qml is its window.
 *
 * It comes before the App and the Library, which cannot be without an open database. What
 * it does is note the choice in the settings; they then start as on any other day.
 */
class Welcome : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Made by the application")
    /** Where Indeks keeps its database unless told otherwise; what the choice starts with. */
    Q_PROPERTY(QString standardFolder READ standardFolder CONSTANT)
    Q_PROPERTY(QUrl standardFolderUrl READ standardFolderUrl CONSTANT)
public:
    using QObject::QObject;

    /**
     * Whether this is a new installation: there is no local database yet and none was ever
     * added. Looks without making anything.
     */
    static bool isNeeded();
    /**
     * The folder a database is to be started in because the user chose it here; empty if
     * none. It is the one folder other than the local database's that may be without an
     * index when Indeks starts.
     */
    static QString folderToStartIn();

    QString standardFolder() const;
    QUrl standardFolderUrl() const;
    bool isDecided() const
    {
        return m_decided;
    }

    Q_INVOKABLE QString localPath(const QString &input) const;
    /** See DatabaseChoice::inspectNewFolder(); the folder of the local database is free here. */
    Q_INVOKABLE QVariantMap inspectNewFolder(const QString &folder) const;
    /** See DatabaseChoice::inspectIndexFile(). */
    Q_INVOKABLE QVariantMap inspectIndexFile(const QString &file) const;
    /** Decides for a new database in @p folder. Returns what is wrong with that, if anything. */
    Q_INVOKABLE QString startNew(const QString &folder);
    /** Decides for the database whose index is @p file. Returns what is wrong with that, if anything. */
    Q_INVOKABLE QString openExisting(const QString &file);

Q_SIGNALS:
    void decided();

private:
    bool m_decided = false;
};
