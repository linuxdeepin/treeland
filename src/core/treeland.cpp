// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "treeland.h"
#include "session/sessionmanager.h"

#include "core/qmlengine.h"
#include "dconfig/dconfigmanager.h"
#include "greeter/usermodel.h"
#ifndef DISABLE_DDM
#include "greeter/greeterproxy.h"
#include "greeter/loginstatequery.h"
#endif
#include "interfaces/multitaskviewinterface.h"
#include "interfaces/plugininterface.h"
#include "core/waylandserver.h"
#include "seatuserconfig.hpp"
#include "seat/seatmanager.h"
#include "session/session.h"
#include "treelanduserconfig.hpp"
#include "utils/cmdline.h"
#include "utils/scriptrunner.h"
#include "common/treelandlogging.h"
#include "common/constants.h"

#include <qqml.h>

#if !defined(DISABLE_DDM) || defined(EXT_SESSION_LOCK_V1)
#include "interfaces/lockscreeninterface.h"
#endif

#include <wsocket.h>
#include <woutputrenderwindow.h>
#ifndef DISABLE_XWAYLAND
#include <wxwayland.h>
#endif

#include <QCoreApplication>
#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusServiceWatcher>
#include <QDebug>
#include <QLocalSocket>
#include <QLoggingCategory>
#include <QMetaMethod>
#include <QTranslator>
#include <QDir>
#include <QStandardPaths>

#include <cerrno>
#include <fcntl.h>
#include <functional>
#include <memory>
#include <pwd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

DCORE_USE_NAMESPACE

namespace Treeland {

class TreelandPrivate : public QObject
{
    Q_OBJECT
    Q_DECLARE_PUBLIC(Treeland)
public:
    explicit TreelandPrivate(Treeland *parent)
        : QObject(parent)
        , q_ptr(parent)
#ifndef DISABLE_DDM
        , socket(new QLocalSocket(this))
#endif
    {
    }

    void init(std::function<void()> onInitialized)
    {
        qmlEngine = new QmlEngine(this);
        qmlEngine->addImportPath(QString("%1/qt/qml").arg(QCoreApplication::applicationDirPath()));
        for (const auto &item :
             QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)) {
            qmlEngine->addImportPath(item + "/treeland/qml");
        }

        W_Q(Treeland);
        // Use QueuedConnection to avoid
        // assert(wlroots: assert(wl_list_empty(&cur->events.button.listener_list)))
        // failed during quit(If the quit call is from the cursor's button press/release event)
        connect(qmlEngine, &QQmlEngine::quit, q, &Treeland::quit, Qt::QueuedConnection);

#ifndef DISABLE_DDM
        auto userModel = qmlEngine->singletonInstance<UserModel *>("Treeland", "UserModel");
        const QString initialUserName = userModel && !userModel->currentUserName().isEmpty()
            ? userModel->currentUserName()
            : QStringLiteral("dde");
#else
        const QString initialUserName = QStringLiteral("dde");
#endif
        auto initializeTreeland = [this, q, onInitialized = std::move(onInitialized)] {
            server = qmlEngine->singletonInstance<WaylandServer *>("Treeland", "WaylandServer");
            connect(server, &WaylandServer::requestQuit, q, &Treeland::quit, Qt::QueuedConnection);

#ifndef DISABLE_XWAYLAND
            if (qEnvironmentVariableIsEmpty("WLR_XWAYLAND"))
                qputenv("WLR_XWAYLAND", QByteArray(LIBEXEC_DIR) + "/treeland-xwayland");
#endif
            server->init(q);

#ifndef DISABLE_DDM
            connect(server,
                    &WaylandServer::configChanged,
                    this,
                    &TreelandPrivate::onCurrentUserChanged);
            onCurrentUserChanged();
#endif
            onInitialized();
        };

        auto *configManager = DConfigManager::instance();
        if (!configManager || configManager->initializeUserConfigs(initialUserName)) {
            qCInfo(lcTlConfig) << "Initial user DConfig is ready for" << initialUserName;
            initializeTreeland();
            return;
        }

        qCInfo(lcTlConfig) << "Waiting for initial user DConfig initialization for"
                           << initialUserName;

        auto *userConfig = configManager->initialUserConfig();
        auto *seatConfig = configManager->userSeatConfig(initialUserName, QStringLiteral(DEFAULT_SEAT_NAME));
        auto initialized = std::make_shared<bool>(false);
        auto initializeWhenReady = [initializeTreeland,
                                    userConfig,
                                    seatConfig,
                                    initialized,
                                    initialUserName] {
            if (*initialized) {
                return;
            }

            const bool userSucceeded = userConfig && userConfig->isInitializeSucceeded();
            const bool seatSucceeded = seatConfig && seatConfig->isInitializeSucceeded();
            const bool userFinished = !userConfig || userSucceeded
                || userConfig->isInitializeFailed();
            const bool seatFinished = !seatConfig || seatSucceeded
                || seatConfig->isInitializeFailed();
            if (!userFinished || !seatFinished) {
                return;
            }

            *initialized = true;
            if (!userSucceeded || !seatSucceeded) {
                qCWarning(lcTlConfig)
                    << "Initial user DConfig initialization failed; Treeland will continue with generated defaults.";
            } else {
                qCInfo(lcTlConfig) << "Initial user DConfig initialization completed for"
                                   << initialUserName;
            }
            initializeTreeland();
        };

        if (userConfig) {
            connect(userConfig,
                    &TreelandUserConfig::configInitializeSucceed,
                    this,
                    initializeWhenReady,
                    Qt::SingleShotConnection);
            connect(userConfig,
                    &TreelandUserConfig::configInitializeFailed,
                    this,
                    initializeWhenReady,
                    Qt::SingleShotConnection);
        }
        if (seatConfig) {
            connect(seatConfig,
                    &SeatUserDConfig::configInitializeSucceed,
                    this,
                    initializeWhenReady,
                    Qt::SingleShotConnection);
            connect(seatConfig,
                    &SeatUserDConfig::configInitializeFailed,
                    this,
                    initializeWhenReady,
                    Qt::SingleShotConnection);
        }
        initializeWhenReady();
    }

    ~TreelandPrivate()
    {
        for (auto plugin : plugins) {
            plugin->shutdown();
            delete plugin;
        }

        plugins.clear();

        for (auto it = pluginTs.begin(); it != pluginTs.end();) {
            QCoreApplication::removeTranslator(it->second);
            it->second->deleteLater();
            pluginTs.erase(it++);
        }
        // UserModel must be deleted before WaylandServer, to remove client surfaces before rendering ends.
        delete qmlEngine->singletonInstance<UserModel *>("Treeland", "UserModel");
        // WaylandServer must be deleted before QmlEngine, for surfaces to be cleanly removed.
        qmlEngine->clearSingletons();
    }

#ifndef DISABLE_DDM
    void onCurrentUserChanged()
    {
        auto userModel =
            qmlEngine->singletonInstance<UserModel *>("Treeland", "UserModel");
        auto user = userModel->currentUser();
        onCurrentChanged(user ? user->UID() : getuid());
    }

    void onCurrentChanged(uid_t uid)
    {
        auto userModel =
            server->qmlEngine()->singletonInstance<UserModel *>("Treeland", "UserModel");
        auto user = userModel->getUser(uid);
        if (!user) {
            qCWarning(lcTlDBus) << "user " << uid << " has been added but couldn't find it.";
            return;
        }

        auto locale = user->locale();
        qCInfo(lcTlDBus) << "current locale:" << locale.language();

        do {
            auto *newTrans = new QTranslator{ this };
            if (newTrans
                    ->load(locale, "treeland", ".", TREELAND_COMPONENTS_TRANSLATION_DIR, ".qm")) {
                if (lastTrans) {
                    QCoreApplication::removeTranslator(lastTrans);
                    lastTrans->deleteLater();
                }
                lastTrans = newTrans;
                QCoreApplication::installTranslator(lastTrans);
                qmlEngine->retranslate();
                break;
            }
            newTrans->deleteLater();
            qCWarning(lcTlDBus) << "failed to load new translator";
        } while (false);
    }

    void updatePluginTs(PluginInterface *plugin, const QString &scope)
    {
        auto userModel =
            server->qmlEngine()->singletonInstance<UserModel *>("Treeland", "UserModel");
        auto user = userModel->currentUser();
        if (!user) {
            return;
        }

        auto locale = userModel->currentUser()->locale();
        qCInfo(lcTlDBus) << "current locale:" << locale.language();
        QTranslator *newTrans = new QTranslator;

        if (newTrans->load(locale, scope, ".", TREELAND_COMPONENTS_TRANSLATION_DIR, ".qm")) {
            for (auto it = pluginTs.begin(); it != pluginTs.end(); ++it) {
                if (it->first == plugin) {
                    QCoreApplication::removeTranslator(it->second);
                    pluginTs.erase(it);
                    break;
                }
            }

            pluginTs[plugin] = newTrans;
            QCoreApplication::installTranslator(pluginTs[plugin]);
            qmlEngine->retranslate();
        } else {
            qCWarning(lcTlDBus) << "failed to load plugin translator: " << scope;
        }
    }
#endif

    void loadPlugin(const QString &path)
    {
        Q_Q(Treeland);

        QDir pluginsDir(path);

        if (!pluginsDir.exists()) {
            return;
        }

        const QStringList pluginFiles = pluginsDir.entryList(QDir::Files | QDir::NoDotAndDotDot);
        for (const QString &pluginFile : pluginFiles) {
            QString filePath = pluginsDir.absoluteFilePath(pluginFile);
            qCDebug(lcTlPlugin) << "Attempting to load plugin:" << filePath;

            QPluginLoader loader(filePath);
            QObject *pluginInstance = loader.instance();

            if (!pluginInstance) {
                qCWarning(lcTlPlugin) << "Failed to load plugin:" << loader.errorString();
                continue;
            }

            PluginInterface *plugin = qobject_cast<PluginInterface *>(pluginInstance);
            if (!plugin) {
                qCWarning(lcTlPlugin) << "Plugin does not implement PluginInterface.";
            }

            qCDebug(lcTlPlugin) << "Loaded plugin: " << plugin->name()
                             << ", enabled: " << plugin->enabled()
                             << ", metadata: " << loader.metaData();
            // TODO: use scheduler to run
            plugin->initialize(q);
            plugins.push_back(plugin);

            const QString scope{
                loader.metaData().value("MetaData").toObject().value("translate").toString()
            };
            qCDebug(lcTlPlugin) << "Plugin translate scope:" << scope;

#ifndef DISABLE_DDM
            connect(server->qmlEngine()->singletonInstance<UserModel *>("Treeland", "UserModel"),
                    &UserModel::currentUserNameChanged,
                    pluginInstance,
                    [this, plugin, scope] {
                        updatePluginTs(plugin, scope);
                    });

            updatePluginTs(plugin, scope);
#endif

            if (auto *multitaskview = qobject_cast<IMultitaskView *>(pluginInstance)) {
                qCDebug(lcTlPlugin) << "Get MultitaskView Instance.";
                connect(pluginInstance, &QObject::destroyed, this, [this] {
                    server->setMultitaskViewImpl(nullptr);
                });
                server->setMultitaskViewImpl(multitaskview);
            }

#if !defined(DISABLE_DDM) || defined(EXT_SESSION_LOCK_V1)
            if (auto *lockscreen = qobject_cast<ILockScreen *>(pluginInstance)) {
                qCDebug(lcTlPlugin) << "Get LockScreen Instance.";
                connect(pluginInstance, &QObject::destroyed, this, [this] {
                    server->setLockScreenImpl(nullptr);
                });
                server->setLockScreenImpl(lockscreen);
            }
#endif
        }
    }

private:
    Treeland *q_ptr;
#ifndef DISABLE_DDM
    Dtk::Accounts::DAccountsManager manager;
    QLocalSocket *socket{ nullptr };
#endif
    QTranslator *lastTrans{ nullptr };
    QmlEngine *qmlEngine{ nullptr };
    std::vector<PluginInterface *> plugins;
    QLocalSocket *helperSocket{ nullptr };
    WaylandServer *server{ nullptr };
    QMap<QString, std::shared_ptr<QDBusUnixFileDescriptor>> userDisplayFds;
    ScriptRunner *m_scriptRunner{ nullptr };
    std::vector<QAction *> shortcuts;
    std::map<PluginInterface *, QTranslator *> pluginTs;
};


class Compositor1Adaptor: public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.Compositor1")
    Q_CLASSINFO("D-Bus Introspection",
                "  <interface name=\"org.deepin.Compositor1\">\n"
                "    <method name=\"ActivateWayland\">\n"
                "      <arg direction=\"in\" type=\"h\" name=\"fd\"/>\n"
                "      <arg direction=\"out\" type=\"b\" name=\"result\"/>\n"
                "    </method>\n"
#ifndef DISABLE_XWAYLAND
                "    <method name=\"XWaylandName\">\n"
                "      <arg direction=\"out\" type=\"s\" name=\"result\"/>\n"
                "      <arg direction=\"out\" type=\"ay\" name=\"auth\"/>\n"
                "    </method>\n"
#endif
                "    <signal name=\"SessionChanged\"/>\n"
                "    <signal name=\"XWaylandAuthChanged\"/>\n"
                "  </interface>\n"
                "")
public:
    Compositor1Adaptor(Treeland *parent)
    : QDBusAbstractAdaptor(parent)
    {
        setAutoRelaySignals(true);
    }

    inline Treeland *parent() const
    { return static_cast<Treeland *>(QObject::parent()); }

public Q_SLOTS: // METHODS

    bool ActivateWayland(const QDBusUnixFileDescriptor &fd)
    {
        return parent()->ActivateWayland(fd);
    }

#ifndef DISABLE_XWAYLAND
    void XWaylandName()
    {
        parent()->XWaylandName();
    }
#endif

Q_SIGNALS: // SIGNALS
    void SessionChanged();
    void XWaylandAuthChanged();
};

Treeland::Treeland()
    : QObject()
    , d_ptr(std::make_unique<TreelandPrivate>(this))
{
    Q_D(Treeland);

    qmlRegisterModule("Treeland.Protocols", 1, 0);

    d->init([this] { initialize(); });
}

#ifndef DISABLE_DDM
void Treeland::onLoginStateQueried(bool hasLoggedInUser)
{
    if (hasLoggedInUser) {
        continueStartup();
    } else {
        runPrestartScripts();
    }
}
#endif

void Treeland::initialize()
{
    Q_D(Treeland);

    auto globalSession = d->server->sessionManager()->globalSession();
    Q_ASSERT(globalSession);

    if (CmdLine::ref().run().has_value()) {
        auto exec = [runCmd = CmdLine::ref().run().value(), d, globalSession] {
            qCInfo(lcTlDBus) << "run cmd:" << runCmd;
            if (auto cmdline = CmdLine::ref().unescapeExecArgs(runCmd); cmdline) {
                auto cmdArgs = cmdline.value();

                auto envs = QProcessEnvironment::systemEnvironment();
                envs.insert("WAYLAND_DISPLAY",
                            globalSession->socket()->fullServerName());
#ifndef DISABLE_XWAYLAND
                if (auto currentUserSession = d->server->sessionManager()->sessionForUid(getuid())) {
                    if (auto *xwayland = currentUserSession->xwayland())
                        envs.insert("DISPLAY", xwayland->displayName());
                } else if (auto *current = globalSession->xwayland()) {
                    envs.insert("DISPLAY", current->displayName());
                }
#endif
                envs.insert("XDG_SESSION_DESKTOP", "Treeland");

                QProcess process;
                process.setProgram(cmdArgs.constFirst());
                process.setArguments(cmdArgs.mid(1));
                process.setProcessEnvironment(envs);
                process.setStandardInputFile(QProcess::nullDevice());
                process.setStandardOutputFile(QProcess::nullDevice());
                process.setStandardErrorFile(QProcess::nullDevice());
                process.startDetached();
            }
        };
        auto con =
            connect(d->server->sessionManager(), &SessionManager::socketFileChanged, this, exec, Qt::SingleShotConnection);

        WSocket *defaultSocket = globalSession->socket();
        if (defaultSocket && defaultSocket->isValid()) {
            QObject::disconnect(con);
            exec();
        }
    }

    new Compositor1Adaptor(this);

    // init dbus after QML engine loaded.
    QDBusConnection::systemBus().registerService("org.deepin.Compositor1");
    QDBusConnection::systemBus().registerObject("/org/deepin/Compositor1", this);

    QDBusConnection::sessionBus().registerService("org.deepin.Compositor1");
    QDBusConnection::sessionBus().registerObject("/org/deepin/Compositor1", this);
#ifndef DISABLE_DDM
    auto *query = new LoginStateQuery(this);
    connect(query, &LoginStateQuery::finished, this, &Treeland::onLoginStateQueried);
    connect(query, &LoginStateQuery::finished, query, &QObject::deleteLater);
    query->start();
#else
    runPrestartScripts();
#endif
}

void Treeland::runPrestartScripts()
{
    Q_D(Treeland);
    auto globalSession = d->server->sessionManager()->globalSession();

    auto *runner = new ScriptRunner(globalSession->socket()->fullServerName(), d);
    d->m_scriptRunner = runner;
    connect(runner, &ScriptRunner::blockingFinished,
            this, &Treeland::continueStartup);
    connect(runner, &ScriptRunner::blockingFinished,
            runner, &ScriptRunner::startNonBlocking);

    const auto dataDirs = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    for (const auto &dataDir : dataDirs) {
        runner->append(QDir(dataDir).filePath(QStringLiteral("prestart.d/blocking")), ScriptRunner::Blocking);
        runner->append(QDir(dataDir).filePath(QStringLiteral("prestart.d/non-blocking")), ScriptRunner::NonBlocking);
    }

    runner->start();
}

void Treeland::continueStartup()
{
    Q_D(Treeland);

    QDir dir(QStringLiteral(TREELAND_PLUGINS_OUTPUT_PATH));
    if (dir.exists() && dir.isReadable()) {
        d->loadPlugin(QStringLiteral(TREELAND_PLUGINS_OUTPUT_PATH));
    } else {
        qCInfo(lcTlPlugin) << "The Treeland plugin build directory is inaccessible, "
                                   "falling back to the installation directory";
        d->loadPlugin(QStringLiteral(TREELAND_PLUGINS_INSTALL_PATH));
    }
#ifndef DISABLE_DDM
    auto *greeter = d->qmlEngine->singletonInstance<GreeterProxy *>("Treeland", "GreeterProxy");
    greeter->enableHandshake();
#else
    d->server->initializeWallpaper();
#endif
}

Treeland::~Treeland()
{

}

bool Treeland::debugMode() const
{
#ifdef QT_DEBUG
    return true;
#endif

    return qEnvironmentVariableIsSet("TREELAND_ENABLE_DEBUG");
}

QmlEngine *Treeland::qmlEngine() const
{
    Q_D(const Treeland);

    return d->qmlEngine;
}

Workspace *Treeland::workspace() const
{
    Q_D(const Treeland);

    return d->server->workspace();
}

RootSurfaceContainer *Treeland::rootSurfaceContainer() const
{
    Q_D(const Treeland);

    return d->server->rootSurfaceContainer();
}

void Treeland::releaseUserWaylandSocket(QString user, WSocket *expected)
{
    Q_D(Treeland);
    auto *userModel = d->server->qmlEngine()->singletonInstance<UserModel *>("Treeland", "UserModel");
    const auto account = userModel->getUser(user);
    if (!account || account->waylandSocket().get() != expected)
        return;
    // Retain the socket until cleanup finishes, including when invoked by its own watcher.
    const auto socket = account->waylandSocket();
    qCDebug(lcTlDBus) << "Releasing Wayland socket for user" << user
                      << "with" << socket->clients().size() << "clients";
    account->setWaylandSocket(nullptr);
    d->userDisplayFds.remove(user);
}

bool Treeland::ActivateWayland(QDBusUnixFileDescriptor _fd)
{
    Q_D(Treeland);
    if (!_fd.isValid()) {
        qCWarning(lcTlDBus) << "Cannot activate Wayland socket: invalid file descriptor";
        return false;
    }

    const auto owner = message().service();
    const auto uid = connection().interface()->serviceUid(owner);
    if (!uid.isValid()) {
        qCWarning(lcTlDBus) << "Cannot resolve Wayland socket owner" << owner << uid.error();
        return false;
    }
    const auto session = d->server->sessionManager()->sessionForUid(uid.value());
    WSocket *sessionSocket = session ? session->socket() : nullptr;
    if (!sessionSocket) {
        qCWarning(lcTlDBus) << "Cannot activate Wayland socket: no session for uid" << uid.value();
        return false;
    }
    auto *userModel = d->server->qmlEngine()->singletonInstance<UserModel *>("Treeland", "UserModel");
    const QString user = session->username();
    const auto account = userModel->getUser(user);
    if (!account) {
        qCWarning(lcTlDBus) << "Cannot activate Wayland socket: user account not found" << user;
        return false;
    }

    if (const auto existing = account->waylandSocket()) {
        struct stat incomingStat {}, existingStat {};
        if (fstat(_fd.fileDescriptor(), &incomingStat) != 0
            || fstat(existing->socketFd(), &existingStat) != 0) {
            qCWarning(lcTlDBus) << "Cannot compare Wayland sockets for user" << user << "errno" << errno;
            return false;
        }
        if (incomingStat.st_dev == existingStat.st_dev && incomingStat.st_ino == existingStat.st_ino) {
            if (existing->parentSocket() != sessionSocket) {
                qCWarning(lcTlDBus) << "Cannot reuse Wayland socket for user" << user
                                    << "in a different session" << session->id();
                return false;
            }
            auto *watcher = existing->findChild<QDBusServiceWatcher *>(QStringLiteral("WaylandSocketOwner"));
            if (watcher)
                watcher->setWatchedServices({owner});
            qCDebug(lcTlDBus) << "Reusing Wayland socket for user" << user
                              << "session" << session->id() << "owner" << owner;
            return true;
        }
    }

    auto fd = std::make_shared<QDBusUnixFileDescriptor>();
    fd->swap(_fd);
    // WSocket owns its descriptor; the saved D-Bus descriptor has a separate lifetime.
    const int socketFd = fcntl(fd->fileDescriptor(), F_DUPFD_CLOEXEC, 0);
    if (socketFd < 0) {
        qCWarning(lcTlDBus) << "Cannot duplicate Wayland socket for user" << user << "errno" << errno;
        return false;
    }
    auto socket = std::make_shared<WSocket>(sessionSocket);
    if (!socket->create(socketFd, false)) {
        qCWarning(lcTlDBus) << "Cannot create Wayland socket for user" << user << "session" << session->id();
        ::close(socketFd);
        return false;
    }

    auto *globalSocket = d->server->sessionManager()->globalSession()->socket();
    socket->setEnabled(sessionSocket->isEnabled(), globalSocket);
    connect(sessionSocket, &WSocket::enabledChanged, socket.get(),
            [child = socket.get(), sessionSocket, globalSocket] {
                child->setEnabled(sessionSocket->isEnabled(), globalSocket);
            });
    d->server->addSocket(socket.get());
    account->setWaylandSocket(socket);
    d->userDisplayFds[user] = fd;

    auto *watcher = new QDBusServiceWatcher(owner, connection(),
        QDBusServiceWatcher::WatchForUnregistration, socket.get());
    watcher->setObjectName(QStringLiteral("WaylandSocketOwner"));
    connect(watcher, &QDBusServiceWatcher::serviceUnregistered, socket.get(),
            [this, user, expected = socket.get(), watcher](const QString &service) {
                if (watcher->watchedServices().contains(service))
                    releaseUserWaylandSocket(user, expected);
            });
    connect(session.get(), &Session::aboutToBeDestroyed, socket.get(),
            [this, user, expected = socket.get()] { releaseUserWaylandSocket(user, expected); });

    qCDebug(lcTlDBus) << "Registered Wayland socket for user" << user
                      << "session" << session->id() << "owner" << owner;
    return true;
}

#ifndef DISABLE_XWAYLAND
void Treeland::XWaylandName()
{
    Q_D(Treeland);

    setDelayedReply(true);

    auto m = message();
    auto conn = connection();

    auto uid = connection().interface()->serviceUid(message().service());
    auto session = d->server->sessionManager()->sessionForUid(uid);
    auto *xwayland = session ? session->xwayland() : nullptr;
    if (!xwayland) {
        auto reply = m.createErrorReply(QDBusError::InternalError,
                                        "Failed to prepare XWayland session");
        conn.send(reply);
        return;
    }
    const QString &display = xwayland->displayName();

    QFile authFile(QStringLiteral("/tmp/.xauth_%1").arg(display));
    if (!authFile.open(QIODevice::ReadOnly)) {
        conn.send(m.createErrorReply(QDBusError::InternalError, "Failed to open xauth file"));
        return;
    }

    QVariantList result;
    result << QVariant(display) << QVariant(authFile.readAll());
    conn.send(m.createReply(result));
    return;
}
#endif

void Treeland::quit()
{
    // make sure all deleted before app exit
    d_ptr.reset();
    qApp->quit();
    // TODO: release drm master and return tty to text mode
}

} // namespace Treeland

#include "treeland.moc"
