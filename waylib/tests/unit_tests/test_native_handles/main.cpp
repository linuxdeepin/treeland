// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <WServer>
#include <WBackend>

#include <QTest>

#include <type_traits>
#include <utility>

struct wl_display;
struct wlr_backend;
struct wlr_session;

WAYLIB_SERVER_USE_NAMESPACE

static_assert(std::is_same_v<decltype(std::declval<WServer &>().handle()), wl_display *>);
static_assert(std::is_same_v<decltype(std::declval<WBackend &>().handle()), wlr_backend *>);
static_assert(std::is_same_v<decltype(std::declval<WBackend &>().session()), wlr_session *>);

class NativeHandlesTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void serverExposesNativeDisplay()
    {
        WServer server;
        QVERIFY(server.handle());
    }

    void backendOwnsNativeHandle()
    {
        qputenv("WLR_BACKENDS", "headless");
        {
            WServer server;
            auto *backend = server.attach<WBackend>();
            server.start();
            QVERIFY(backend->handle());
            server.stop();
        }
        qunsetenv("WLR_BACKENDS");
    }
};

QTEST_MAIN(NativeHandlesTest)
#include "main.moc"
