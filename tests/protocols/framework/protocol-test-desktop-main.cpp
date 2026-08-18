// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "core/treeland.h"
#include "core/treelandinit.h"
#include "core/rootsurfacecontainer.h"
#include "protocol-test-client.h"
#include "interfaces/lockscreeninterface.h"
#include "interfaces/multitaskviewinterface.h"
#include "interfaces/plugininterface.h"
#include "interfaces/proxyinterface.h"
#include "seat/helper.h"
#include "session/session.h"
#include "treelandconfig.hpp"
#include "treelanduserconfig.hpp"

#include <QGuiApplication>
#include <QAbstractItemModel>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusObjectPath>
#include <QDir>
#include <QMetaObject>
#include <QPluginLoader>
#include <QSemaphore>
#include <QTimer>
#include <wsocket.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <pthread.h>
#include <memory>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

void protocol_test_desktop_setup(Helper *helper);
extern "C" bool protocol_test_desktop_ready(Helper *helper) __attribute__((weak));
extern "C" bool protocol_test_desktop_preflight() __attribute__((weak));
extern "C" bool protocol_test_desktop_skip() __attribute__((weak));

namespace {
constexpr auto accountsService = "org.freedesktop.Accounts";
constexpr auto accountsPath = "/org/freedesktop/Accounts";
constexpr auto accountsUserPath = "/org/freedesktop/Accounts/User1000";
constexpr auto dconfigService = "org.desktopspec.ConfigManager";

class ProtocolTestAccountsManager : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Accounts")
    Q_PROPERTY(QStringList UserList READ userList)

public:
    QStringList userList() const { return { QString::fromLatin1(accountsUserPath) }; }

public slots:
    QList<QDBusObjectPath> ListCachedUsers() const
    {
        return { QDBusObjectPath(QString::fromLatin1(accountsUserPath)) };
    }

    QDBusObjectPath FindUserById(qint64) const
    {
        return QDBusObjectPath(QString::fromLatin1(accountsUserPath));
    }
    QDBusObjectPath FindUserByName(const QString &) const
    {
        return QDBusObjectPath(QString::fromLatin1(accountsUserPath));
    }
};

class ProtocolTestAccountsUser : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Accounts.User")
    Q_PROPERTY(int AccountType MEMBER accountType)
    Q_PROPERTY(bool NoPasswdLogin MEMBER noPasswdLogin)
    Q_PROPERTY(qulonglong Uid MEMBER uid)
    Q_PROPERTY(qulonglong Gid MEMBER gid)
    Q_PROPERTY(QString UserName MEMBER userName)
    Q_PROPERTY(QString FullName MEMBER fullName)
    Q_PROPERTY(QString HomeDir MEMBER homeDir)
    Q_PROPERTY(QString IconFile MEMBER iconFile)
    Q_PROPERTY(QString PasswordHint MEMBER passwordHint)
    Q_PROPERTY(QString Locale MEMBER locale)

public:
    int accountType = 0;
    bool noPasswdLogin = false;
    qulonglong uid = 1000;
    qulonglong gid = 1000;
    QString userName = QStringLiteral("protocol-test");
    QString fullName = QStringLiteral("Protocol Test");
    QString homeDir = QStringLiteral("/tmp");
    QString iconFile;
    QString passwordHint;
    QString locale = QStringLiteral("C");
};

class ProtocolTestAccountsService
{
public:
    bool start()
    {
        int addressPipe[2];
        if (pipe(addressPipe) != 0)
            return false;

        m_busPid = fork();
        if (m_busPid == 0) {
            close(addressPipe[0]);
            dup2(addressPipe[1], STDOUT_FILENO);
            close(addressPipe[1]);
            execlp("dbus-daemon",
                   "dbus-daemon",
                   "--session",
                   "--nofork",
                   "--print-address=1",
                   "--nopidfile",
                   nullptr);
            _exit(127);
        }
        close(addressPipe[1]);
        if (m_busPid < 0) {
            close(addressPipe[0]);
            return false;
        }

        QByteArray address;
        char character;
        while (address.size() < 4096 && read(addressPipe[0], &character, 1) == 1) {
            if (character == '\n')
                break;
            address.append(character);
        }
        close(addressPipe[0]);
        if (address.isEmpty()) {
            stop();
            return false;
        }
        qputenv("DBUS_SYSTEM_BUS_ADDRESS", address);
        qputenv("DBUS_SESSION_BUS_ADDRESS", address);
        return startDConfigService();
    }

    bool registerObjects()
    {
        auto connection = QDBusConnection::systemBus();
        constexpr auto exportOptions = QDBusConnection::ExportAllSlots
            | QDBusConnection::ExportAllProperties;
        return connection.isConnected() && waitForService(connection, dconfigService)
            && connection.registerService(QString::fromLatin1(accountsService))
            && connection.registerObject(QString::fromLatin1(accountsPath), &m_manager, exportOptions)
            && connection.registerObject(QString::fromLatin1(accountsUserPath), &m_user, exportOptions);
    }

    void stop()
    {
        if (m_dconfigPid > 0) {
            kill(m_dconfigPid, SIGTERM);
            waitpid(m_dconfigPid, nullptr, 0);
            m_dconfigPid = -1;
        }
        if (m_busPid > 0) {
            kill(m_busPid, SIGTERM);
            waitpid(m_busPid, nullptr, 0);
            m_busPid = -1;
        }
        if (!m_dconfigPrefix.isEmpty())
            QDir(m_dconfigPrefix).removeRecursively();
    }

private:
    bool startDConfigService()
    {
        char prefix[] = "/tmp/treeland-protocol-dconfig-XXXXXX";
        if (!mkdtemp(prefix))
            return false;
        m_dconfigPrefix = QString::fromLocal8Bit(prefix);
        const QByteArray dsgDir = qgetenv("TREELAND_PROTOCOL_TEST_DSG_DIR");
        if (dsgDir.isEmpty() || !QDir().mkpath(m_dconfigPrefix + "/usr/share"))
            return false;
        const QByteArray dsgLink = (m_dconfigPrefix + "/usr/share/dsg").toLocal8Bit();
        if (symlink(dsgDir.constData(), dsgLink.constData()) != 0)
            return false;

        m_dconfigPid = fork();
        if (m_dconfigPid == 0) {
            execlp("dde-dconfig-daemon",
                   "dde-dconfig-daemon",
                   "-p",
                   prefix,
                   nullptr);
            _exit(127);
        }
        return m_dconfigPid > 0;
    }

    static bool waitForService(const QDBusConnection &connection, const QString &service)
    {
        auto *interface = connection.interface();
        if (!interface)
            return false;
        for (int attempt = 0; attempt != 100; ++attempt) {
            const auto registered = interface->isServiceRegistered(service);
            if (registered.isValid() && registered.value())
                return true;
            usleep(10'000);
        }
        return false;
    }

    pid_t m_busPid = -1;
    pid_t m_dconfigPid = -1;
    QString m_dconfigPrefix;
    ProtocolTestAccountsManager m_manager;
    ProtocolTestAccountsUser m_user;
};

class ProtocolTestRunner : public QObject
{
public:
    int invoke(protocol_test_server_callback callback, void *data)
    {
        QSemaphore done;
        if (!QMetaObject::invokeMethod(this, [callback, data, &done] {
                callback(data);
                done.release();
            }, Qt::QueuedConnection))
            return 0;
        done.acquire();
        return 1;
    }
};

ProtocolTestRunner *g_runner = nullptr;

class TestTreelandProxy final : public TreelandProxyInterface
{
public:
    explicit TestTreelandProxy(Treeland::Treeland *treeland)
        : m_treeland(treeland)
    {
    }

    QmlEngine *qmlEngine() const override { return m_treeland->qmlEngine(); }
    Workspace *workspace() const override { return m_treeland->workspace(); }
    RootSurfaceContainer *rootSurfaceContainer() const override
    {
        return m_treeland->rootSurfaceContainer();
    }

private:
    Treeland::Treeland *m_treeland = nullptr;
};

void loadTestPlugins(TreelandProxyInterface *proxy, Helper *helper)
{
    static std::vector<std::unique_ptr<QPluginLoader>> loaders;
    const QDir pluginsDir(qEnvironmentVariable("TREELAND_TEST_PLUGINS_PATH"));
    for (const auto &pluginFile : pluginsDir.entryList(QDir::Files | QDir::NoDotAndDotDot)) {
        auto loader = std::make_unique<QPluginLoader>(pluginsDir.absoluteFilePath(pluginFile));
        auto *instance = loader->instance();
        auto *plugin = qobject_cast<PluginInterface *>(instance);
        if (!plugin)
            continue;

        plugin->initialize(proxy);
        if (auto *multitask = qobject_cast<IMultitaskView *>(instance))
            helper->setMultitaskViewImpl(multitask);
        if (auto *lockscreen = qobject_cast<ILockScreen *>(instance))
            helper->setLockScreenImpl(lockscreen);
        loaders.push_back(std::move(loader));
    }
}

struct ClientThreadContext {
    const char *socketName = nullptr;
    std::atomic_bool done = false;
    int result = 1;
};

void *runClient(void *data)
{
    auto *context = static_cast<ClientThreadContext *>(data);
    context->result = protocol_test_run(context->socketName);
    context->done = true;
    return nullptr;
}
}

extern "C" int protocol_test_invoke_server(protocol_test_server_callback callback, void *data)
{
    return g_runner && callback ? g_runner->invoke(callback, data) : 0;
}

int main(int argc, char *argv[])
{
    // QML resources use stable qrc URLs, so a stale user disk cache can be
    // reused after rebuilding a plugin and make protocol tests run old QML.
    qputenv("QML_DISABLE_DISK_CACHE", "1");
    if (protocol_test_desktop_preflight && !protocol_test_desktop_preflight()) {
        std::fflush(nullptr);
        std::_Exit(77);
    }
    ProtocolTestAccountsService accountsService;
    if (!accountsService.start()) {
        accountsService.stop();
        return 1;
    }
    Treeland::preInit(Treeland::InitOptions{
        .headless = true,
        .createPlatformTheme = {},
    });
    QGuiApplication app(argc, argv);
    Treeland::postInit();
    if (!accountsService.registerObjects()) {
        accountsService.stop();
        return 1;
    }

    Treeland::Treeland treeland;
    auto *helper = Helper::instance();
    if (!helper)
        return 1;
    TestTreelandProxy testProxy(&treeland);
    loadTestPlugins(&testProxy, helper);
    protocol_test_desktop_setup(helper);
    if (protocol_test_desktop_skip && protocol_test_desktop_skip()) {
        std::fflush(nullptr);
        std::_Exit(77);
    }

    const auto session = helper->sessionManager()->globalSession();
    if (!session || !session->socket() || !session->socket()->isValid())
        return 1;

    ProtocolTestRunner runner;
    g_runner = &runner;
    const QByteArray socketName = session->socket()->fullServerName().toUtf8();
    ClientThreadContext context { .socketName = socketName.constData() };

    pthread_t thread;
    bool clientStarted = false;
    int fixtureWaitReports = 0;

    const auto fixtureReady = [helper] {
        if (helper->rootSurfaceContainer()->outputs().isEmpty())
            return false;
        return !protocol_test_desktop_ready || protocol_test_desktop_ready(helper);
    };
    const auto startClient = [&] {
        if (clientStarted)
            return;
        if (!fixtureReady()) {
            if (fixtureWaitReports++ < 3) {
                const auto *root = helper->rootSurfaceContainer();
                std::fprintf(stderr,
                             "desktop runner waiting: outputs=%d model-rows=%d global=(ready=%d failed=%d) "
                             "user=(ready=%d failed=%d)\n",
                             static_cast<int>(root->outputs().size()),
                             root->outputModel()->rowCount(),
                             helper->globalConfig()->isInitializeSucceeded(),
                             helper->globalConfig()->isInitializeFailed(),
                             helper->config()->isInitializeSucceeded(),
                             helper->config()->isInitializeFailed());
            }
            return;
        }
        if (pthread_create(&thread, nullptr, runClient, &context) != 0) {
            context.result = 1;
            context.done = true;
            return;
        }
        clientStarted = true;
    };

    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [&] {
        if (context.done)
            app.quit();
    });
    timer.start(10);
    // Fixture readiness is a production state, so it is driven by the output
    // model's insertion signal rather than a timeout/polling guess.  Most
    // fixtures use the default (at least one output); specialized fixtures
    // may provide protocol_test_desktop_ready().
    QObject::connect(helper->rootSurfaceContainer()->outputModel(),
                     &QAbstractItemModel::rowsInserted,
                     &app,
                     [startClient](const QModelIndex &, int, int) { startClient(); });
    QObject::connect(helper->globalConfig(),
                     &TreelandConfig::configInitializeSucceed,
                     &app,
                     [startClient](auto *) { startClient(); });
    QObject::connect(helper->config(),
                     &TreelandUserConfig::configInitializeSucceed,
                     &app,
                     [startClient](auto *) { startClient(); });
    startClient();
    app.exec();
    if (clientStarted)
        pthread_join(thread, nullptr);
    g_runner = nullptr;

    // Treeland owns process-lifetime QML singletons whose shutdown ordering is
    // only exercised by a compositor process exit.  Letting the stack object
    // destruct here tears down Helper after its seat event filter and currently
    // reaches an invalid SeatsManager during that production-only shutdown.
    // The protocol client has already completed and been joined, so terminate
    // without running the unrelated compositor shutdown sequence.
    accountsService.stop();
    std::fflush(nullptr);
    std::_Exit(context.result);
}

#include "protocol-test-desktop-main.moc"
