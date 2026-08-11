// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "core/treeland.h"
#include "core/treelandinit.h"
#include "core/rootsurfacecontainer.h"
#include "protocol-test-client.h"
#include "seat/helper.h"
#include "session/session.h"

#include <QGuiApplication>
#include <QElapsedTimer>
#include <QMetaObject>
#include <QSemaphore>
#include <QTimer>
#include <wsocket.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <pthread.h>

void protocol_test_desktop_setup(Helper *helper);

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
    Treeland::preInit(Treeland::InitOptions{ .headless = true });
    QGuiApplication app(argc, argv);

    Treeland::Treeland treeland;
    auto *helper = Helper::instance();
    if (!helper)
        return 1;
    protocol_test_desktop_setup(helper);

    const auto session = helper->sessionManager()->globalSession();
    if (!session || !session->socket() || !session->socket()->isValid())
        return 1;

    ProtocolTestRunner runner;
    g_runner = &runner;
    const QByteArray socketName = session->socket()->fullServerName().toUtf8();
    ClientThreadContext context { .socketName = socketName.constData() };

    pthread_t thread;
    bool clientStarted = false;

    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [&] {
        if (context.done)
            app.quit();
    });
    timer.start(10);
    // Helper starts the headless backend during construction.  Output setup
    // also waits for DConfig and QML work, so start the client only once the
    // production root container has accepted an output.
    QElapsedTimer outputDeadline;
    outputDeadline.start();
    QTimer outputReadyTimer;
    QObject::connect(&outputReadyTimer, &QTimer::timeout, [&] {
        if (clientStarted)
            return;
        if (helper->rootSurfaceContainer()->outputs().isEmpty()) {
            if (outputDeadline.elapsed() < 5000)
                return;
            fprintf(stderr, "desktop protocol fixture timed out waiting for a root output\n");
            context.result = 1;
            context.done = true;
            outputReadyTimer.stop();
            return;
        }
        if (pthread_create(&thread, nullptr, runClient, &context) != 0) {
            context.result = 1;
            context.done = true;
            return;
        }
        clientStarted = true;
        outputReadyTimer.stop();
    });
    outputReadyTimer.start(10);
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
