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
#include <QDir>
#include <QMetaObject>
#include <QPluginLoader>
#include <QSemaphore>
#include <QTimer>
#include <wsocket.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#include <memory>
#include <vector>

void protocol_test_desktop_setup(Helper *helper);
extern "C" bool protocol_test_desktop_ready(Helper *helper) __attribute__((weak));
extern "C" bool protocol_test_desktop_preflight() __attribute__((weak));
extern "C" bool protocol_test_desktop_skip() __attribute__((weak));

namespace {
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
    // The fixture loads the CMake-provided build-plugin path after Treeland is
    // constructed, instead of resolving Release plugins from the install prefix.
    qputenv("TREELAND_TEST_SKIP_PLUGINS", "1");
    if (protocol_test_desktop_preflight && !protocol_test_desktop_preflight()) {
        std::fflush(nullptr);
        std::_Exit(77);
    }
    Treeland::preInit(Treeland::InitOptions{
        .headless = true,
        .createPlatformTheme = {},
    });
    QGuiApplication app(argc, argv);

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

    const auto fixtureReady = [helper] {
        if (helper->rootSurfaceContainer()->outputs().isEmpty())
            return false;
        return !protocol_test_desktop_ready || protocol_test_desktop_ready(helper);
    };
    const auto startClient = [&] {
        if (clientStarted || !fixtureReady())
            return;
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
    std::fflush(nullptr);
    std::_Exit(context.result);
}
