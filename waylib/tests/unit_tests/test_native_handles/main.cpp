// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <WServer>
#include <WBackend>
#include <WInputDevice>
#include <WOutput>
#include <woutputlayout.h>

#include <QTest>
#include <QPointer>
#include <QSignalSpy>

extern "C" {
#include <interfaces/wlr_input_device.h>
#include <wlr/backend/headless.h>
}

#include <type_traits>
#include <utility>

struct wl_display;
struct wlr_backend;
struct wlr_input_device;
struct wlr_output;
struct wlr_output_layout;
struct wlr_session;

WAYLIB_SERVER_USE_NAMESPACE

static_assert(std::is_same_v<decltype(std::declval<WServer &>().handle()), wl_display *>);
static_assert(std::is_same_v<decltype(std::declval<WBackend &>().handle()), wlr_backend *>);
static_assert(std::is_same_v<decltype(std::declval<WBackend &>().session()), wlr_session *>);
static_assert(std::is_same_v<decltype(std::declval<WInputDevice &>().handle()), wlr_input_device *>);
static_assert(std::is_constructible_v<WInputDevice, wlr_input_device *>);
static_assert(std::is_same_v<decltype(std::declval<WOutput &>().handle()), wlr_output *>);
static_assert(std::is_same_v<decltype(std::declval<WOutputLayout &>().handle()), wlr_output_layout *>);

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

    void backendRemovesDestroyedInputDevice()
    {
        qputenv("WLR_BACKENDS", "headless");
        WServer server;
        auto *backend = server.attach<WBackend>();
        server.start();

        wlr_input_device nativeDevice;
        wlr_input_device_init(&nativeDevice, WLR_INPUT_DEVICE_POINTER, "backend-test-pointer");
        QSignalSpy added(backend, &WBackend::inputAdded);
        QSignalSpy removed(backend, &WBackend::inputRemoved);
        wl_signal_emit_mutable(&backend->handle()->events.new_input, &nativeDevice);
        QCOMPARE(added.count(), 1);
        QCOMPARE(backend->inputDeviceList().size(), 1);

        wlr_input_device_finish(&nativeDevice);

        QCOMPARE(removed.count(), 1);
        QVERIFY(backend->inputDeviceList().isEmpty());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        server.stop();
        qunsetenv("WLR_BACKENDS");
    }

    void outputAndLayoutTrackNativeLifetime()
    {
        WServer server;
        auto *nativeBackend = wlr_headless_backend_create(wl_display_get_event_loop(server.handle()));
        QVERIFY(nativeBackend);
        auto *nativeOutput = wlr_headless_add_output(nativeBackend, 800, 600);
        QVERIFY(nativeOutput);

        QPointer<WOutput> output = new WOutput(nativeOutput, nullptr);
        QCOMPARE(output->handle(), nativeOutput);
        QCOMPARE(WOutput::fromHandle(nativeOutput), output.data());

        auto *layout = new WOutputLayout(&server);
        QVERIFY(layout->handle());
        QSignalSpy layoutChanged(layout, &WOutputLayout::changed);
        layout->add(output, QPoint(30, 40));
        QCOMPARE(layoutChanged.count(), 1);
        QCOMPARE(output->position(), QPoint(30, 40));
        QCOMPARE(layout->outputs(), QList<WOutput *> { output });

        QSignalSpy invalidated(output, &WOutput::invalidated);
        bool aboutToBeInvalidated = false;
        connect(output, &WOutput::aboutToBeInvalidated, this, [&] {
            aboutToBeInvalidated = true;
            QCOMPARE(output->handle(), nativeOutput);
            QVERIFY(!WOutput::fromHandle(nativeOutput));
        });
        wlr_output_destroy(nativeOutput);

        QVERIFY(aboutToBeInvalidated);
        QCOMPARE(invalidated.count(), 1);
        QVERIFY(!output->handle());
        QVERIFY(!WOutput::fromHandle(nativeOutput));
        QVERIFY(layout->outputs().isEmpty());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(output.isNull());

        wlr_backend_destroy(nativeBackend);
    }
};

QTEST_MAIN(NativeHandlesTest)
#include "main.moc"
