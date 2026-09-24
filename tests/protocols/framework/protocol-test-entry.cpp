// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "core/dconfigmanager.h"
#include "core/treeland.h"
#include "core/treelandinit.h"
#include "core/rootsurfacecontainer.h"
#include "test-accounts-service.h"
#include "server-bridge-api.h"
#include "test-dconfig-service.h"
#include "seat/helper.h"
#include "session/session.h"
#include "treelandconfig.hpp"
#include "treelanduserconfig.hpp"

#include <QGuiApplication>
#include <QAbstractItemModel>
#include <QEventLoop>
#include <QMetaObject>
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
#include <pthread.h>

void protocol_test_setup(Helper *helper);
extern "C" int protocol_test_run(const char *socketName);
extern "C" bool protocol_test_ready(Helper *helper) __attribute__((weak));
extern "C" bool protocol_test_preflight() __attribute__((weak));
extern "C" bool protocol_test_skip() __attribute__((weak));

namespace {
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
    ProtocolTest(Helper *helper, const QByteArray &socketName)
        : m_helper(helper)
        , m_socketName(socketName)
        , m_context { .socketName = m_socketName.constData(), .test = this }
    {
    }

public slots:
    void notifyClientFinished() { Q_EMIT clientFinished(); }

private slots:
    void initTestCase()
    {
        auto ensureOutputInLayout = [this](WOutput *output) {
            if (!output)
                return;

            if (!output->renderer()) {
                auto *window = m_helper->window();
                if (window->renderer() && window->allocator()) {
                    wlr_output_init_render(output->handle(),
                                           window->allocator(),
                                           window->renderer());
                }
            }

            if (!output->isEnabled())
                return;

            auto *layout = m_helper->rootSurfaceContainer()->outputLayout();
            if (output->renderer() && layout
                && !layout->outputs().contains(output)) {
                layout->autoAdd(output);
            }
        };
        connect(m_helper->window(),
                &WOutputRenderWindow::outputViewportInitialized,
                this,
                [ensureOutputInLayout](WOutputViewport *viewport) {
                    ensureOutputInLayout(viewport->output());
                });
        connect(m_helper->backend(),
                &WBackend::outputAdded,
                this,
                [this, ensureOutputInLayout](WOutput *output) {
                    connect(output,
                            &WOutput::enabledChanged,
                            this,
                            [ensureOutputInLayout, output] { ensureOutputInLayout(output); });
                    ensureOutputInLayout(output);
                });
        protocol_test_setup(m_helper);
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

signals:
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

    bool fixtureReady() const
    {
        return !m_helper->rootSurfaceContainer()->outputs().isEmpty()
            && (!protocol_test_ready || protocol_test_ready(m_helper));
    }

    bool waitForFixture()
    {
        if (fixtureReady())
            return true;

        QEventLoop loop;
        const auto quitWhenReady = [this, &loop] {
            if (fixtureReady())
                loop.quit();
        };
        connect(m_helper->rootSurfaceContainer()->outputModel(),
                &QAbstractItemModel::rowsInserted,
                &loop,
                [quitWhenReady](const QModelIndex &, int, int) { quitWhenReady(); });
        connect(m_helper->globalConfig(),
                &TreelandConfig::configInitializeSucceed,
                &loop,
                [quitWhenReady](auto *) { quitWhenReady(); });
        connect(m_helper->config(),
                &TreelandUserConfig::configInitializeSucceed,
                &loop,
                [quitWhenReady](auto *) { quitWhenReady(); });
        loop.exec();
        return fixtureReady();
    }

    Helper *m_helper = nullptr;
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

    // Treeland::Treeland completes initialization asynchronously: Helper is
    // only created once the initial user DConfig objects have finished
    // initializing. Spin the event loop until then.
    Helper *helper = Helper::instance();
    if (!helper) {
        QEventLoop loop;
        QTimer poller;
        QObject::connect(&poller, &QTimer::timeout, &loop, [&loop, &helper] {
            helper = Helper::instance();
            if (helper)
                loop.quit();
        });
        poller.start(50);
        QTimer::singleShot(15000, &loop, &QEventLoop::quit);
        loop.exec();
    }
    if (!helper) {
        std::fflush(nullptr);
        std::_Exit(1);
    }

    const auto session = helper->sessionManager()->globalSession();
    if (!session || !session->socket() || !session->socket()->isValid())
        return 1;

    const QByteArray socketName = session->socket()->fullServerName().toUtf8();
    ProtocolTestRunner runner;
    g_runner = &runner;
    ProtocolTest test(helper, socketName);
    int result = QTest::qExec(&test, argc, argv);
    if (g_clientSkipped && result == 0)
        result = 77;
    g_runner = nullptr;

    // Treeland owns process-lifetime QML singletons whose shutdown ordering is
    // only exercised by a compositor process exit.  Letting the stack object
    // destruct here tears down Helper after its seat event filter and currently
    // reaches an invalid SeatManager during that production-only shutdown.
    // The protocol client has already completed and been joined, so terminate
    // without running the unrelated compositor shutdown sequence.
    dconfigService.stop();
    std::fflush(nullptr);
    std::_Exit(result);
}

#include "protocol-test-entry.moc"
