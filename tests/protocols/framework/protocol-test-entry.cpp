// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "dconfig/dconfigmanager.h"
#include "session/sessionmanager.h"
#include "core/treeland.h"
#include "core/treelandinit.h"
#include "core/rootsurfacecontainer.h"
#include "test-accounts-service.h"
#include "server-bridge-api.h"
#include "test-dconfig-service.h"
#include "core/waylandserver.h"
#include "session/session.h"
#include "session/sessionoutputmanager.h"
#include "treelandconfig.hpp"
#include "treelanduserconfig.hpp"

#include <QGuiApplication>
#include <QAbstractItemModel>
#include <QEventLoop>
#include <QMetaObject>
#include <QSet>
#include <QTimer>
#include <QSemaphore>
#include <QtTest>
#include <wbackend.h>
#include <woutput.h>
#include <woutputlayout.h>
#include <woutputrenderwindow.h>
#include <woutputviewport.h>
#include <wsocket.h>

#include <wlr_all.h>

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <pthread.h>

void protocol_test_setup(WaylandServer *server);
extern "C" int protocol_test_run(const char *socketName);
extern "C" bool protocol_test_ready(WaylandServer *server) __attribute__((weak));
extern "C" bool protocol_test_preflight() __attribute__((weak));
extern "C" bool protocol_test_skip() __attribute__((weak));

namespace {
// Upper bound for a fixture to report ready; the protocol tests themselves may
// not add fixed delays, this only turns a missing signal into a failure.
constexpr int FixtureWatchdogMs = 15000;

class ProtocolTestRunner : public QObject
{
public:
    int invoke(server_thread_callback callback, void *data)
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
bool g_clientSkipped = false;

class ProtocolTest;
struct ClientThreadContext {
    const char *socketName = nullptr;
    ProtocolTest *test = nullptr;
    int result = 1;
};

class ProtocolTest final : public QObject
{
    Q_OBJECT

public:
    ProtocolTest(WaylandServer *server, const QByteArray &socketName)
        : m_server(server)
        , m_socketName(socketName)
        , m_context { .socketName = m_socketName.constData(), .test = this }
    {
    }

public Q_SLOTS:
    void notifyClientFinished() { Q_EMIT clientFinished(); }

private Q_SLOTS:
    void initTestCase()
    {
        auto ensureOutputInLayout = [this](WOutput *output) {
            if (!output)
                return;

            if (!output->renderer()) {
                auto *window = m_server->window();
                if (window->renderer() && window->allocator()) {
                    wlr_output_init_render(output->handle(),
                                           window->allocator(),
                                           window->renderer());
                }
            }

            if (!output->isEnabled())
                return;

            auto *layout = m_server->rootSurfaceContainer()->outputLayout();
            if (output->renderer() && layout
                && !layout->outputs().contains(output)) {
                layout->autoAdd(output);
            }
        };
        connect(m_server->window(),
                &WOutputRenderWindow::outputViewportInitialized,
                this,
                [ensureOutputInLayout](WOutputViewport *viewport) {
                    ensureOutputInLayout(viewport->output());
                });
        connect(m_server->backend(),
                &WBackend::outputAdded,
                this,
                [this, ensureOutputInLayout](WOutput *output) {
                    connect(output,
                            &WOutput::enabledChanged,
                            this,
                            [ensureOutputInLayout, output] { ensureOutputInLayout(output); });
                    ensureOutputInLayout(output);
                });
        protocol_test_setup(m_server);
        if (protocol_test_skip && protocol_test_skip())
            QSKIP("protocol fixture requested skip");
    }

    void protocol()
    {
        QVERIFY2(waitForFixture(), "protocol fixture did not become ready");
        QEventLoop loop;
        connect(this, &ProtocolTest::clientFinished, &loop, &QEventLoop::quit);
        QVERIFY2(pthread_create(&m_thread, nullptr, runClient, &m_context) == 0,
                 "failed to start protocol client");
        loop.exec();

        pthread_join(m_thread, nullptr);
        if (m_context.result == 77) {
            g_clientSkipped = true;
            QSKIP("protocol client requested skip");
        }
        QCOMPARE(m_context.result, 0);
    }

Q_SIGNALS:
    void clientFinished();

private:
    static void *runClient(void *data)
    {
        auto *context = static_cast<ClientThreadContext *>(data);
        context->result = protocol_test_run(context->socketName);
        QMetaObject::invokeMethod(context->test,
                                  [test = context->test] { test->notifyClientFinished(); },
                                  Qt::QueuedConnection);
        return nullptr;
    }

    // A session that is already watched must not be connected again, and a
    // session created while the fixture is waiting must not be missed:
    // SessionManager::sessionAdded() announces the latter.
    void watchSessionOutputs(QEventLoop *loop,
                             const std::function<void()> &quitWhenReady)
    {
        for (const auto &session : m_server->sessionManager()->sessions()) {
            auto *outputs = session->outputManager();
            if (!outputs || m_watchedSessionOutputs.contains(outputs))
                continue;
            m_watchedSessionOutputs.insert(outputs);
            connect(outputs,
                    &SessionOutputManager::desktopGeometryChanged,
                    loop,
                    quitWhenReady);
        }
    }

    bool fixtureReady() const
    {
        return !m_server->rootSurfaceContainer()->outputs().isEmpty()
            && (!protocol_test_ready || protocol_test_ready(m_server));
    }

    bool waitForFixture()
    {
        if (fixtureReady())
            return true;

        QEventLoop loop;
        // The subscriptions below belong to this loop; a later wait starts from
        // an empty set so it can subscribe everything again.
        m_watchedSessionOutputs.clear();
        const auto quitWhenReady = [this, &loop] {
            if (fixtureReady())
                loop.quit();
        };
        // Every production signal that can make fixtureReady() true belongs
        // here: the output model gains the fixture's output, the output layout
        // receives it, a session's output manager publishes new protocol
        // heads, or a configuration finishes loading. A readiness condition
        // without a signal here would stall until the watchdog below.
        connect(m_server->rootSurfaceContainer()->outputModel(),
                &QAbstractItemModel::rowsInserted,
                &loop,
                [quitWhenReady](const QModelIndex &, int, int) { quitWhenReady(); });
        connect(m_server->rootSurfaceContainer()->outputLayout(),
                &WOutputLayout::outputsChanged,
                &loop,
                quitWhenReady);
        watchSessionOutputs(&loop, quitWhenReady);
        connect(m_server->sessionManager(),
                &SessionManager::sessionAdded,
                &loop,
                [this, &loop, quitWhenReady] { watchSessionOutputs(&loop, quitWhenReady); });
        connect(m_server->globalConfig(),
                &TreelandConfig::configInitializeSucceed,
                &loop,
                [quitWhenReady](auto *) { quitWhenReady(); });
        connect(m_server->config(),
                &TreelandUserConfig::configInitializeSucceed,
                &loop,
                [quitWhenReady](auto *) { quitWhenReady(); });
        // The LockScreen plugin is loaded after the blocking prestart hooks
        // finish, which none of the signals above announce. Wait for the lock
        // screen availability signal so protocol_test_ready() gates that depend
        // on it (e.g. ext-session-lock) wake up instead of hanging.
        connect(m_server,
                &WaylandServer::lockScreenAvailableChanged,
                &loop,
                [quitWhenReady] { quitWhenReady(); });
        // addOutput() auto-adds to the layout (emits outputsChanged) before it
        // sets the primary output, so a primaryOutput() predicate also needs
        // this signal to be observable.
        connect(m_server->rootSurfaceContainer(),
                &RootSurfaceContainer::primaryOutputChanged,
                &loop,
                quitWhenReady);

        // Watchdog only: it keeps a fixture that never signals from consuming
        // the whole CTest timeout, and it fails the test through the caller's
        // QVERIFY2 instead of letting it hang.
        QTimer watchdog;
        watchdog.setSingleShot(true);
        connect(&watchdog, &QTimer::timeout, &loop, &QEventLoop::quit);
        watchdog.start(FixtureWatchdogMs);

        loop.exec();
        return fixtureReady();
    }

    WaylandServer *m_server = nullptr;
    QSet<SessionOutputManager *> m_watchedSessionOutputs;
    QByteArray m_socketName;
    ClientThreadContext m_context;
    pthread_t m_thread {};
};
}

extern "C" int invoke_on_server_thread(server_thread_callback callback, void *data)
{
    return g_runner && callback ? g_runner->invoke(callback, data) : 0;
}

int main(int argc, char *argv[])
{
    // QML resources use stable qrc URLs, so a stale user disk cache can be
    // reused after rebuilding a plugin and make protocol tests run old QML.
    qputenv("QML_DISABLE_DISK_CACHE", "1");
    // The compositor exits via std::_Exit(), so no destructor would ever stop a
    // wallpaper factory child; simply do not start one.
    qputenv("TREELAND_DISABLE_WALLPAPER", "1");
    if (protocol_test_preflight && !protocol_test_preflight()) {
        std::fflush(nullptr);
        std::_Exit(77);
    }
    TestDConfigService dconfigService;
    if (!dconfigService.start()) {
        dconfigService.stop();
        return 1;
    }
    auto application = Treeland::preInit(argc, argv);
    Treeland::postInit();
    if (!dconfigService.waitForService()) {
        dconfigService.stop();
        return 1;
    }
    TestAccountsService accountsService;
    if (!accountsService.registerObjects()) {
        dconfigService.stop();
        return 1;
    }

    DConfigManager dConfigManager(application.get());
    Treeland::Treeland treeland;

    // Treeland::Treeland completes initialization asynchronously: WaylandServer is
    // only created once the initial user DConfig objects have finished
    // initializing. Spin the event loop until then.
    WaylandServer *server = WaylandServer::instance();
    if (!server) {
        QEventLoop loop;
        QTimer poller;
        QObject::connect(&poller, &QTimer::timeout, &loop, [&loop, &server] {
            server = WaylandServer::instance();
            if (server)
                loop.quit();
        });
        poller.start(50);
        QTimer::singleShot(15000, &loop, &QEventLoop::quit);
        loop.exec();
    }
    if (!server) {
        std::fflush(nullptr);
        std::_Exit(1);
    }

    const auto session = server->sessionManager()->globalSession();
    if (!session || !session->socket() || !session->socket()->isValid())
        return 1;

    const QByteArray socketName = session->socket()->fullServerName().toUtf8();
    ProtocolTestRunner runner;
    g_runner = &runner;
    ProtocolTest test(server, socketName);
    int result = QTest::qExec(&test, argc, argv);
    if (g_clientSkipped && result == 0)
        result = 77;
    g_runner = nullptr;

    // Treeland owns process-lifetime QML singletons whose shutdown ordering is
    // only exercised by a compositor process exit.  Letting the stack object
    // destruct here tears down WaylandServer after its seat event filter and currently
    // reaches an invalid SeatManager during that production-only shutdown.
    // The protocol client has already completed and been joined, so terminate
    // without running the unrelated compositor shutdown sequence.
    dconfigService.stop();
    std::fflush(nullptr);
    std::_Exit(result);
}

#include "protocol-test-entry.moc"
