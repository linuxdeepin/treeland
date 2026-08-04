// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <WServer>
#include <WBackend>
#include <WInputDevice>
#include <WOutput>
#include <WCursor>
#include <WSeat>
#include <WSurface>
#include <WXdgDecorationManager>
#include <WXdgShell>
#include <WLayerShell>
#include <WLayerSurface>
#include <WSessionLock>
#include <WSessionLockManager>
#include <WSessionLockSurface>
#include <wxdgdialogmanagerv1.h>
#include <wxdgpopupsurface.h>
#include <wxdgtoplevelsurface.h>
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
struct wlr_cursor;
struct wlr_input_device;
struct wlr_output;
struct wlr_output_layout;
struct wlr_seat;
struct wlr_session;
struct wlr_layer_shell_v1;
struct wlr_layer_surface_v1;
struct wlr_xdg_decoration_manager_v1;
struct wlr_xdg_popup;
struct wlr_xdg_shell;
struct wlr_xdg_toplevel;
struct wlr_xdg_wm_dialog_v1;
struct wlr_session_lock_manager_v1;
struct wlr_session_lock_surface_v1;
struct wlr_session_lock_v1;

WAYLIB_SERVER_USE_NAMESPACE

static_assert(std::is_same_v<decltype(std::declval<WServer &>().handle()), wl_display *>);
static_assert(std::is_same_v<decltype(std::declval<WBackend &>().handle()), wlr_backend *>);
static_assert(std::is_same_v<decltype(std::declval<WBackend &>().session()), wlr_session *>);
static_assert(std::is_same_v<decltype(std::declval<WInputDevice &>().handle()), wlr_input_device *>);
static_assert(std::is_constructible_v<WInputDevice, wlr_input_device *>);
static_assert(std::is_same_v<decltype(std::declval<WOutput &>().handle()), wlr_output *>);
static_assert(std::is_same_v<decltype(std::declval<WOutputLayout &>().handle()), wlr_output_layout *>);
static_assert(std::is_same_v<decltype(std::declval<WCursor &>().handle()), wlr_cursor *>);
static_assert(std::is_same_v<decltype(std::declval<WSeat &>().handle()), wlr_seat *>);
static_assert(std::is_same_v<decltype(std::declval<WSurface &>().handle()), wlr_surface *>);
static_assert(std::is_same_v<decltype(std::declval<WXdgShell &>().handle()), wlr_xdg_shell *>);
static_assert(std::is_same_v<decltype(std::declval<WXdgToplevelSurface &>().handle()), wlr_xdg_toplevel *>);
static_assert(std::is_same_v<decltype(std::declval<WXdgPopupSurface &>().handle()), wlr_xdg_popup *>);
static_assert(std::is_same_v<decltype(std::declval<WLayerShell &>().handle()), wlr_layer_shell_v1 *>);
static_assert(std::is_same_v<decltype(std::declval<WLayerSurface &>().handle()), wlr_layer_surface_v1 *>);
static_assert(std::is_same_v<decltype(std::declval<WXdgDecorationManager &>().handle()),
                             wlr_xdg_decoration_manager_v1 *>);
static_assert(std::is_same_v<decltype(std::declval<WXdgDialogManagerV1 &>().handle()),
                             wlr_xdg_wm_dialog_v1 *>);
static_assert(std::is_same_v<decltype(std::declval<WSessionLockManager &>().handle()),
                             wlr_session_lock_manager_v1 *>);
static_assert(std::is_same_v<decltype(std::declval<WSessionLock &>().handle()),
                             wlr_session_lock_v1 *>);
static_assert(std::is_same_v<decltype(std::declval<WSessionLockSurface &>().handle()),
                             wlr_session_lock_surface_v1 *>);

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

    void shellManagersExposeNativeHandles()
    {
        WServer server;
        auto *xdgShell = server.attach<WXdgShell>(6);
        auto *layerShell = server.attach<WLayerShell>(xdgShell);
        auto *decorationManager = server.attach<WXdgDecorationManager>();
        auto *dialogManager = server.attach<WXdgDialogManagerV1>();
        auto *sessionLockManager = server.attach<WSessionLockManager>();

        server.start();
        QVERIFY(xdgShell->handle());
        QVERIFY(layerShell->handle());
        QVERIFY(decorationManager->handle());
        QVERIFY(dialogManager->handle());
        QVERIFY(sessionLockManager->handle());
        server.stop();
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

    void cursorOwnsNativeHandle()
    {
        wlr_cursor *nativeCursor = nullptr;
        {
            QObject owner;
            auto *cursor = new WCursor(&owner);
            nativeCursor = cursor->handle();
            QVERIFY(nativeCursor);
            QCOMPARE(WCursor::fromHandle(nativeCursor), cursor);
        }
        QVERIFY(!WCursor::fromHandle(nativeCursor));
    }

    void seatTracksNativeLifetime()
    {
        qputenv("WAYLIB_DISABLE_GESTURE", "1");
        WServer server;
        QPointer<WSeat> seat = server.attach<WSeat>();
        server.start();

        auto *nativeSeat = seat->handle();
        QVERIFY(nativeSeat);
        QCOMPARE(WSeat::fromHandle(nativeSeat), seat.data());

        server.stop();
        QVERIFY(seat.isNull());
        QVERIFY(!WSeat::fromHandle(nativeSeat));
        qunsetenv("WAYLIB_DISABLE_GESTURE");
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
