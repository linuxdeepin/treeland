// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "core/treelandinit.h"
#include "protocol-test-client.h"

#include <QGuiApplication>
#include <QMetaObject>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QTimer>
#include <wsocket.h>
#include <wserver.h>

#include <atomic>
#include <pthread.h>

WAYLIB_SERVER_USE_NAMESPACE

void protocol_test_setup(WServer *server);

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

    WServer server;
    server.start();
    Treeland::initTestServer(&server);
    protocol_test_setup(&server);

    QTemporaryDir runtimeDir;
    if (!runtimeDir.isValid())
        return 1;
    WSocket socket(false);
    if (!socket.autoCreate(runtimeDir.path()))
        return 1;
    server.addSocket(&socket);

    ProtocolTestRunner runner;
    g_runner = &runner;
    const QByteArray socketName = socket.fullServerName().toUtf8();
    ClientThreadContext context { .socketName = socketName.constData() };

    pthread_t thread;
    if (pthread_create(&thread, nullptr, runClient, &context) != 0)
        return 1;

    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [&] {
        if (context.done)
            app.quit();
    });
    timer.start(10);
    app.exec();
    pthread_join(thread, nullptr);
    g_runner = nullptr;
    return context.result;
}
