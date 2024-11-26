// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "treeland.h"

#include "cmdline.h"
#include "compositor1adaptor.h"
#include "helper.h"
#include "multitaskviewinterface.h"
#include "plugininterface.h"
#include "qmlengine.h"
#include "treelandconfig.h"
#include "usermodel.h"

#ifndef DISABLE_DDM
#  include "lockscreeninterface.h"

#  include <Constants.h>
#  include <Messages.h>
#  include <SignalHandler.h>
#  include <SocketWriter.h>
using namespace DDM;

#  include <DAccountsManager>
#  include <DAccountsUser>
#endif

#include <wsocket.h>
#include <wxwayland.h>

#include <QCoreApplication>
#include <QDebug>
#include <QLocalSocket>
#include <QLoggingCategory>
#include <QMetaMethod>
#include <QTranslator>

#include <memory>
#include <pwd.h>
#include <sys/socket.h>
#include <unistd.h>

Q_LOGGING_CATEGORY(dbus, "treeland.dbus", QtDebugMsg);

DCORE_USE_NAMESPACE;

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

    void init()
    {
        qmlEngine = new QmlEngine(this);
        QObject::connect(qmlEngine, &QQmlEngine::quit, qApp, &QCoreApplication::quit);

        helper = qmlEngine->singletonInstance<Helper *>("Treeland", "Helper");
        helper->init();

#ifndef DISABLE_DDM
        connect(socket, &QLocalSocket::connected, this, &TreelandPrivate::connected);
        connect(socket, &QLocalSocket::disconnected, this, &TreelandPrivate::disconnected);
        connect(socket, &QLocalSocket::readyRead, this, &TreelandPrivate::readyRead);
        connect(socket, &QLocalSocket::errorOccurred, this, &TreelandPrivate::error);

        auto userModel =
            helper->qmlEngine()->singletonInstance<UserModel *>("Treeland.Greeter", "UserModel");

        auto updateUser = [this, userModel] {
            auto user = userModel->currentUser();
            onCurrentChanged(user ? user->UID() : getuid());
        };

        connect(userModel, &UserModel::currentUserNameChanged, this, updateUser);
        updateUser();
#endif
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
    }

#ifndef DISABLE_DDM
    void onCurrentChanged(uid_t uid)
    {
        auto userModel =
            helper->qmlEngine()->singletonInstance<UserModel *>("Treeland.Greeter", "UserModel");
        auto user = userModel->getUser(uid);
        if (!user) {
            qCWarning(dbus) << "user " << uid << " has been added but couldn't find it.";
            return;
        }

        auto locale = user->locale();
        qCInfo(dbus) << "current locale:" << locale.language();

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
            qCWarning(dbus) << "failed to load new translator";
        } while (false);
    }

    void updatePluginTs(PluginInterface *plugin, const QString &scope)
    {
        auto userModel =
            helper->qmlEngine()->singletonInstance<UserModel *>("Treeland.Greeter", "UserModel");
        auto user = userModel->currentUser();
        if (!user) {
            return;
        }

        auto locale = userModel->currentUser()->locale();
        qCInfo(dbus) << "current locale:" << locale.language();
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
            qCWarning(dbus) << "failed to load plugin translator: " << scope;
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
            qCDebug(dbus) << "Attempting to load plugin:" << filePath;

            QPluginLoader loader(filePath);
            QObject *pluginInstance = loader.instance();

            if (!pluginInstance) {
                qWarning(dbus) << "Failed to load plugin:" << loader.errorString();
                continue;
            }

            PluginInterface *plugin = qobject_cast<PluginInterface *>(pluginInstance);
            if (!plugin) {
                qWarning(dbus) << "Plugin does not implement PluginInterface.";
            }

            qCDebug(dbus) << "Loaded plugin: " << plugin->name()
                          << ", enabled: " << plugin->enabled()
                          << ", metadata: " << loader.metaData();
            // TODO: use scheduler to run
            plugin->initialize(q);
            plugins.push_back(plugin);

            const QString scope{
                loader.metaData().value("MetaData").toObject().value("translate").toString()
            };
            qCDebug(dbus) << "Plugin translate scope:" << scope;

#ifndef DISABLE_DDM
            connect(helper->qmlEngine()->singletonInstance<UserModel *>("Treeland.Greeter",
                                                                        "UserModel"),
                    &UserModel::currentUserNameChanged,
                    pluginInstance,
                    [this, plugin, scope] {
                        updatePluginTs(plugin, scope);
                    });

            updatePluginTs(plugin, scope);
#endif

            if (auto *multitaskview = qobject_cast<IMultitaskView *>(pluginInstance)) {
                qCDebug(dbus) << "Get MultitaskView Instance.";
                connect(pluginInstance, &QObject::destroyed, this, [this] {
                    helper->setMultitaskViewImpl(nullptr);
                });
                helper->setMultitaskViewImpl(multitaskview);
            }

#ifndef DISABLE_DDM
            if (auto *lockscreen = qobject_cast<ILockScreen *>(pluginInstance)) {
                qCDebug(dbus) << "Get LockScreen Instance.";
                connect(pluginInstance, &QObject::destroyed, this, [this] {
                    helper->setLockScreenImpl(nullptr);
                });
                helper->setLockScreenImpl(lockscreen);
            }
#endif
        }
    }

private Q_SLOTS:
#ifndef DISABLE_DDM
    void connected();
    void disconnected();
    void readyRead();
    void error();
#endif

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
    Helper *helper{ nullptr };
    QMap<QString, std::shared_ptr<WAYLIB_SERVER_NAMESPACE::WSocket>> userWaylandSocket;
    QMap<QString, std::shared_ptr<QDBusUnixFileDescriptor>> userDisplayFds;
    std::vector<QAction *> shortcuts;
    std::map<PluginInterface *, QTranslator *> pluginTs;
};

Treeland::Treeland()
    : QObject()
    , d_ptr(std::make_unique<TreelandPrivate>(this))
{
    Q_D(Treeland);

    qmlRegisterModule("Treeland.Protocols", 1, 0);
    qmlRegisterSingletonInstance("Treeland",
                                 1,
                                 0,
                                 "TreelandConfig",
                                 &TreelandConfig::ref()); // Inject treeland config singleton.

    d->init();

    if (CmdLine::ref().socket().has_value()) {
#ifndef DISABLE_DDM
        new DDM::SignalHandler(this);
        Q_ASSERT(d->helper);
        auto connectToServer = [this, d] {
            d->socket->connectToServer(CmdLine::ref().socket().value());
        };

        connect(d->helper, &Helper::socketFileChanged, this, connectToServer);

        WSocket *defaultSocket = d->helper->defaultWaylandSocket();
        if (defaultSocket && defaultSocket->isValid()) {
            connectToServer();
        }
#endif
    } else {
        struct passwd *pw = getpwuid(getuid());
        auto *userModel =
            d->helper->qmlEngine()->singletonInstance<UserModel *>("Treeland.Greeter", "UserModel");
        userModel->setCurrentUserName(pw->pw_name);
    }

    if (CmdLine::ref().run().has_value()) {
        auto exec = [runCmd = CmdLine::ref().run().value(), this, d] {
            qCInfo(dbus) << "run cmd:" << runCmd;
            if (auto cmdline = CmdLine::ref().unescapeExecArgs(runCmd); cmdline) {
                auto cmdArgs = cmdline.value();

                auto envs = QProcessEnvironment::systemEnvironment();
                envs.insert("WAYLAND_DISPLAY", d->helper->defaultWaylandSocket()->fullServerName());
                envs.insert("DISPLAY", d->helper->defaultXWaylandSocket()->displayName());
                envs.insert("DDE_CURRENT_COMPOSITOR", "Treeland");

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
            connect(d->helper, &Helper::socketFileChanged, this, exec, Qt::SingleShotConnection);

        WSocket *defaultSocket = d->helper->defaultWaylandSocket();
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

#ifdef QT_DEBUG
    d->loadPlugin(QStringLiteral(TREELAND_PLUGINS_OUTPUT_PATH));
#else
    d->loadPlugin(QStringLiteral(TREELAND_PLUGINS_INSTALL_PATH));
#endif
}

Treeland::~Treeland()
{
    Q_D(Treeland);
}

bool Treeland::testMode() const
{
    return !CmdLine::ref().socket().has_value();
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

    return d->helper->workspace();
}

RootSurfaceContainer *Treeland::rootSurfaceContainer() const
{
    Q_D(const Treeland);

    return d->helper->rootSurfaceContainer();
}

void Treeland::blockActivateSurface(bool block)
{
    TreelandConfig::ref().setBlockActivateSurface(block);
}

bool Treeland::isBlockActivateSurface() const
{
    return TreelandConfig::ref().blockActivateSurface();
}

#ifndef DISABLE_DDM
void TreelandPrivate::connected()
{
    // log connection
    qCDebug(dbus) << "Connected to the daemon.";

    // send connected message
    DDM::SocketWriter(socket) << quint32(DDM::GreeterMessages::Connect);
}

void TreelandPrivate::disconnected()
{
    Q_Q(Treeland);

    // log disconnection
    qCDebug(dbus) << "Disconnected from the daemon.";

    Q_EMIT q->socketDisconnected();

    qCDebug(dbus) << "Display Manager is closed socket connect, quiting treeland.";
    qApp->exit();
}

void TreelandPrivate::error()
{
    qCritical() << "Socket error: " << socket->errorString();
}

void TreelandPrivate::readyRead()
{
    Q_Q(Treeland);

    // input stream
    QDataStream input(socket);

    while (input.device()->bytesAvailable()) {
        // read message
        quint32 message;
        input >> message;

        switch (DDM::DaemonMessages(message)) {
        case DDM::DaemonMessages::Capabilities: {
            // log message
            qCDebug(dbus) << "Message received from daemon: Capabilities";
        } break;
        case DDM::DaemonMessages::LoginSucceeded:
        case DDM::DaemonMessages::UserActivateMessage: {
            QString user;
            input >> user;

            auto *userModel =
                helper->qmlEngine()->singletonInstance<UserModel *>("Treeland.Greeter",
                                                                    "UserModel");
            // NOTE: maybe DDM will active dde user.
            if (!userModel->getUser(user)) {
                break;
            }

            for (auto key : userWaylandSocket.keys()) {
                userWaylandSocket[key]->setEnabled(key == user);
            }

            userModel->setCurrentUserName(user);
        } break;
        case DDM::DaemonMessages::SwitchToGreeter: {
            helper->showLockScreen();
        } break;
        default:
            break;
        }
    }
}
#endif

bool Treeland::ActivateWayland(QDBusUnixFileDescriptor _fd)
{
    Q_D(Treeland);

    if (!_fd.isValid()) {
        return false;
    }

    auto fd = std::make_shared<QDBusUnixFileDescriptor>();
    fd->swap(_fd);

    auto uid = connection().interface()->serviceUid(message().service());
    struct passwd *pw;
    pw = getpwuid(uid);
    QString user{ pw->pw_name };

    auto socket = std::make_shared<WSocket>(true);
    socket->create(fd->fileDescriptor(), false);

    auto userModel =
        d->helper->qmlEngine()->singletonInstance<UserModel *>("Treeland.Greeter", "UserModel");
    socket->setEnabled(userModel->currentUserName() == user);

    d->helper->addSocket(socket.get());

    d->userWaylandSocket[user] = socket;
    d->userDisplayFds[user] = fd;

    connect(connection().interface(),
            &QDBusConnectionInterface::serviceUnregistered,
            socket.get(),
            [this, user, d] {
                d->userWaylandSocket.remove(user);
                d->userDisplayFds.remove(user);
            });

    return true;
}

QString Treeland::XWaylandName()
{
    Q_D(Treeland);

    setDelayedReply(true);

    auto uid = connection().interface()->serviceUid(message().service());
    struct passwd *pw;
    pw = getpwuid(uid);
    QString user{ pw->pw_name };

    auto *xwayland = d->helper->createXWayland();
    const QString &display = xwayland->displayName();

    auto m = message();
    auto conn = connection();

    QProcess *process = new QProcess(this);
    connect(process, &QProcess::finished, [process, m, conn, user, display] {
        if (process->exitCode() != 0) {
            qCWarning(dbus) << "xhost command failed with exit code" << process->exitCode()
                            << process->readAllStandardOutput() << process->readAllStandardError();
            auto reply =
                m.createErrorReply(QDBusError::InternalError, "Failed to set xhost permissions");
            conn.send(reply);
        } else {
            qCDebug(dbus) << process->exitCode() << " " << process->readAllStandardOutput()
                          << process->readAllStandardError();
            qCDebug(dbus) << QString("user %1 got xwayland display %2.").arg(user).arg(display);
            auto reply = m.createReply(display);
            conn.send(reply);
        }
        process->deleteLater();
    });

    connect(process, &QProcess::errorOccurred, [this, m, conn] {
        auto reply =
            m.createErrorReply(QDBusError::InternalError, "Failed to set xhost permissions");
        conn.send(reply);
    });

    auto env = QProcessEnvironment::systemEnvironment();
    env.insert("DISPLAY", display);
    process->setProcessEnvironment(env);
    process->setProgram("xhost");
    process->setArguments({ QString("+si:localuser:%1").arg(user) });
    process->start();

    return {};
}

} // namespace Treeland

#include "treeland.moc"
