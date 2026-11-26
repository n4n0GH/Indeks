// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "app.h"
#include "indeks-version.h"
#include "indekssettings.h"
#include "thumbnailprovider.h"
#include "welcome.h"

#include <KAboutData>
#include <KCrash>
#include <KDBusService>
#include <KIconTheme>
#include <KLocalizedQmlContext>
#include <KLocalizedString>
#include <KWindowSystem>

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QNetworkAccessManager>
#include <QQmlApplicationEngine>
#include <QThreadPool>
#include <QQmlNetworkAccessManagerFactory>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>

#include <algorithm>
#include <cstdlib>

using namespace Qt::StringLiterals;

namespace
{
/** How long the indexer gets to let go of the file it is reading when Indeks is closed. */
constexpr int ShutdownGraceMs = 3000;

/**
 * Indeks shows content from files it did not write, such as HTML documents and e-mails.
 * Nothing in them may make the application talk to the network, so the QML engine only gets
 * to load local resources.
 */
class LocalOnlyAccessManager : public QNetworkAccessManager
{
public:
    using QNetworkAccessManager::QNetworkAccessManager;

protected:
    QNetworkReply *createRequest(Operation operation, const QNetworkRequest &request, QIODevice *outgoingData) override
    {
        static const QStringList allowed = {u"file"_s, u"qrc"_s, u"data"_s};
        if (allowed.contains(request.url().scheme())) {
            return QNetworkAccessManager::createRequest(operation, request, outgoingData);
        }
        // An empty URL fails right away with "protocol unknown".
        return QNetworkAccessManager::createRequest(operation, QNetworkRequest(QUrl()), nullptr);
    }
};

class LocalOnlyAccessManagerFactory : public QQmlNetworkAccessManagerFactory
{
public:
    QNetworkAccessManager *create(QObject *parent) override
    {
        return new LocalOnlyAccessManager(parent);
    }
};

/** Whether the icon theme called @p name is in one of the folders icon themes are looked for in. */
bool iconThemeInReach(const QString &name)
{
    QString own = qEnvironmentVariable("XDG_DATA_HOME");
    if (own.isEmpty()) {
        own = QDir::homePath() + "/.local/share"_L1;
    }
    QString shared = qEnvironmentVariable("XDG_DATA_DIRS");
    if (shared.isEmpty()) {
        shared = u"/usr/local/share:/usr/share"_s;
    }
    QStringList folders{own + "/icons"_L1};
    const QStringList listed = shared.split(u':', Qt::SkipEmptyParts);
    for (const QString &folder : listed) {
        folders.append(folder + "/icons"_L1);
    }
    return std::any_of(folders.cbegin(), folders.cend(), [&name](const QString &folder) {
        return QFileInfo::exists(folder + u'/' + name + "/index.theme"_L1);
    });
}

/**
 * In a Flatpak, has KDE's libraries read the settings of Plasma's global theme, the icon
 * theme for one, as they do outside.
 *
 * What a global theme sets is not written into `kdeglobals` but into a folder of defaults,
 * `kdedefaults` in the folder of settings, which Plasma lists in `XDG_CONFIG_DIRS` for its
 * session. Flatpak sets that variable anew, so in the sandbox only `kdeglobals` is read,
 * and where the icon theme is not named there, Breeze is drawn whatever the desktop uses.
 * The manifest lets the folder be read; this puts it back at the front of the list. The
 * icon themes themselves Flatpak shows of its own accord, those of the system and those
 * in the user's data folder.
 *
 * Not if the defaults name an icon theme that cannot be seen from in here: KDE's libraries
 * then draw no icons at all, where without the defaults they draw Breeze's.
 *
 * To be called before anything reads a setting.
 */
void readPlasmaDefaultsInFlatpak()
{
    if (!QFileInfo::exists(u"/.flatpak-info"_s)) {
        return;
    }
    QString settings = qEnvironmentVariable("XDG_CONFIG_HOME");
    if (settings.isEmpty()) {
        settings = QDir::homePath() + "/.config"_L1;
    }
    const QString defaults = QDir::cleanPath(settings + "/kdedefaults"_L1);
    if (!QFileInfo(defaults).isDir()) {
        return;
    }
    const QString iconTheme = QSettings(defaults + "/kdeglobals"_L1, QSettings::IniFormat).value(u"Icons/Theme"_s).toString();
    if (!iconTheme.isEmpty() && !iconThemeInReach(iconTheme)) {
        qWarning("The icon theme of the desktop, %s, cannot be seen from inside the Flatpak; Breeze is used instead.", qPrintable(iconTheme));
        return;
    }
    QString listed = qEnvironmentVariable("XDG_CONFIG_DIRS");
    if (listed.isEmpty()) {
        listed = u"/etc/xdg"_s;
    }
    QStringList folders = listed.split(u':', Qt::SkipEmptyParts);
    folders.removeAll(defaults);
    folders.prepend(defaults);
    qputenv("XDG_CONFIG_DIRS", QFile::encodeName(folders.join(u':')));
}
}

int main(int argc, char *argv[])
{
    readPlasmaDefaultsInFlatpak();
    KIconTheme::initTheme();
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain(QByteArrayLiteral("indeks"));

    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(u"org.kde.desktop"_s);
    }

    KAboutData about(u"indeks"_s,
                     i18nc("@title", "Indeks"),
                     QStringLiteral(INDEKS_VERSION_STRING),
                     i18nc("@info", "Find, organize and annotate documents wherever they are stored"),
                     KAboutLicense::GPL_V2,
                     i18nc("@info:credit", "© 2026 n4n0"));
    about.setLicense(KAboutLicense::GPL_V2, KAboutLicense::OrLaterVersions);
    about.setOrganizationDomain(QByteArrayLiteral("n4n0gh.github.io"));
    about.setDesktopFileName(u"io.github.n4n0gh.indeks"_s);
    about.addAuthor(u"n4n0"_s, i18nc("@info:credit", "Author"));
    about.setTranslator(i18nc("NAME OF TRANSLATORS", "Your names"), i18nc("EMAIL OF TRANSLATORS", "Your emails"));
    // The installed icon where there is one, so that an icon theme can bring its own; the
    // logo that is built in otherwise.
    const QIcon logo = QIcon::fromTheme(u"io.github.n4n0gh.indeks"_s, QIcon(u":/io.github.n4n0gh.indeks.svg"_s));
    about.setProgramLogo(logo);
    KAboutData::setApplicationData(about);
    QGuiApplication::setWindowIcon(logo);

    KCrash::initialize();

    QCommandLineParser parser;
    about.setupCommandLine(&parser);
    parser.addPositionalArgument(u"folders"_s, i18nc("@info:shell", "Folders to index"), u"[folders…]"_s);
    parser.process(app);
    about.processCommandLine(&parser);

    // A second start, for instance through "Send to Indeks" in Dolphin, hands its folders to
    // the running instance and quits.
    KDBusService service(KDBusService::Unique);

    // A new installation is asked where its database is to be, or which one to open, before
    // anything is made. Closing the window without an answer is leaving.
    if (Welcome::isNeeded()) {
        Welcome welcome;
        QQmlApplicationEngine asking;
        KLocalization::setupLocalizedContext(&asking);
        asking.setInitialProperties({{u"backend"_s, QVariant::fromValue(&welcome)}});
        asking.loadFromModule("io.github.n4n0gh.indeks", "WelcomeWindow");
        auto window = asking.rootObjects().isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(asking.rootObjects().constFirst());
        if (!window) {
            return 1;
        }
        // The window that closes here is not the last one of this run.
        const bool quitsWithLastWindow = QGuiApplication::quitOnLastWindowClosed();
        QGuiApplication::setQuitOnLastWindowClosed(false);
        QEventLoop loop;
        QObject::connect(&welcome, &Welcome::decided, &loop, &QEventLoop::quit);
        QObject::connect(window, &QWindow::visibleChanged, &loop, [&loop, window] {
            if (!window->isVisible()) {
                loop.quit();
            }
        });
        loop.exec();
        window->hide();
        QGuiApplication::setQuitOnLastWindowClosed(quitsWithLastWindow);
        if (!welcome.isDecided()) {
            return 0;
        }
    }

    App indeks(nullptr);

    QQmlApplicationEngine engine;
    LocalOnlyAccessManagerFactory accessManagerFactory;
    engine.setNetworkAccessManagerFactory(&accessManagerFactory);
    KLocalization::setupLocalizedContext(&engine);
    // The engine owns it.
    engine.addImageProvider(QStringLiteral("thumbnail"), new ThumbnailProvider);
    QThreadPool::globalInstance()->start(&ThumbnailProvider::prune);
    engine.loadFromModule("io.github.n4n0gh.indeks", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    auto window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());

    QObject::connect(&service, &KDBusService::activateRequested, &indeks, [&indeks, window](const QStringList &arguments, const QString &workingDirectory) {
        indeks.handleArguments(arguments, workingDirectory);
        if (window) {
            KWindowSystem::updateStartupId(window);
            KWindowSystem::activateWindow(window);
        }
    });

    if (!parser.positionalArguments().isEmpty()) {
        indeks.handleArguments(app.arguments(), QDir::currentPath());
    }

    // Closing the window ends Indeks, whatever it is busy with. Without this, running transfers
    // would keep the event loop, and with it the indexing, going with no window in sight.
    QObject::connect(&app, &QGuiApplication::lastWindowClosed, &app, &QCoreApplication::quit, Qt::QueuedConnection);

    const int result = app.exec();

    // Call the indexer off. It gives up the file it is reading and keeps what is indexed so
    // far; the next start finds the rest. Should a file format library not come back from a
    // file within the grace period, leave anyway: everything in the index is written in
    // transactions, so the unfinished one is simply undone when the index is opened again.
    // The same goes for a search through the index that is still running.
    QElapsedTimer leaving;
    leaving.start();
    indeks.documents()->stopSearching(0); // asked to end now, waited for below
    indeks.linkCandidates()->stopSearching(0);
    indeks.sidebar()->stopCounting(0);
    const bool indexerStopped = indeks.library()->shutdown(ShutdownGraceMs);
    const bool searchStopped = indeks.documents()->stopSearching(qMax(0, ShutdownGraceMs - int(leaving.elapsed())))
        && indeks.linkCandidates()->stopSearching(qMax(0, ShutdownGraceMs - int(leaving.elapsed())))
        && indeks.sidebar()->stopCounting(qMax(0, ShutdownGraceMs - int(leaving.elapsed())));
    if (!indexerStopped || !searchStopped) {
        qWarning("The indexer or a search did not come back in time; leaving without it");
        Config::self()->save();
        std::_Exit(result);
    }
    return result;
}
