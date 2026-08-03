// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <WServer>
#include <WBackend>
#include <WInputDevice>

#include <QTest>
#include <QPointer>
#include <QSignalSpy>

extern "C" {
#include <interfaces/wlr_input_device.h>
}

#include <type_traits>
#include <utility>

struct wl_display;
struct wlr_backend;
struct wlr_input_device;
struct wlr_session;

WAYLIB_SERVER_USE_NAMESPACE

static_assert(std::is_same_v<decltype(std::declval<WServer &>().handle()), wl_display *>);
static_assert(std::is_same_v<decltype(std::declval<WBackend &>().handle()), wlr_backend *>);
static_assert(std::is_same_v<decltype(std::declval<WBackend &>().session()), wlr_session *>);
static_assert(std::is_same_v<decltype(std::declval<WInputDevice &>().handle()), wlr_input_device *>);
static_assert(std::is_constructible_v<WInputDevice, wlr_input_device *>);

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

    void inputDeviceTracksNativeLifetime()
    {
        wlr_input_device nativeDevice;
        wlr_input_device_init(&nativeDevice, WLR_INPUT_DEVICE_POINTER, "test-pointer");

        QPointer<WInputDevice> device = new WInputDevice(&nativeDevice);
        QCOMPARE(WInputDevice::fromHandle(&nativeDevice), device.data());
        QSignalSpy invalidated(device, &WInputDevice::invalidated);

        wlr_input_device_finish(&nativeDevice);

        QCOMPARE(invalidated.count(), 1);
        QVERIFY(!WInputDevice::fromHandle(&nativeDevice));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(device.isNull());
    }
};

QTEST_MAIN(NativeHandlesTest)
#include "main.moc"
