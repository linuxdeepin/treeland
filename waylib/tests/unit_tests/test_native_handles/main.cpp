// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only


#include <WServer>
#include <WBackend>
#include <WInputDevice>
#include <WOutput>
#include <WOutputManagerV1>
#include <WCursor>
#include <WCursorShapeManagerV1>
#include <WForeignToplevel>
#include <WSeat>
#include <WSurface>
#include <wscoplistener.h>
#include <WXdgDecorationManager>
#include <WXdgShell>
#include <WLayerShell>
#include <WLayerSurface>
#include <WSessionLock>
#include <WSessionLockManager>
#include <WSessionLockSurface>
#include <WXWayland>
#include <WXWaylandSurface>
#include <wxdgdialogmanagerv1.h>
#include <wextforeigntoplevellistv1.h>
#include <wxdgpopupsurface.h>
#include <wxdgtoplevelsurface.h>
#include <winputpopupsurface.h>
#include <woutputlayout.h>
#include <private/winputmethodv2_p.h>
#include <private/wtextinputv3_p.h>
#include <private/wvirtualkeyboardv1_p.h>

#include <QTest>
#include <QPointer>
#include <QSignalSpy>

#include <wlr_all.h>

// wlroots 0.19 exports these functions but only declares them in an
// uninstalled internal header.
extern "C" {
void wlr_input_device_init(struct wlr_input_device *device,
                           enum wlr_input_device_type type, const char *name);
void wlr_input_device_finish(struct wlr_input_device *device);
}

#include <type_traits>
#include <utility>

struct wl_display;
struct wlr_backend;
struct wlr_cursor;
struct wlr_cursor_shape_manager_v1;
struct wlr_input_device;
struct wlr_output;
struct wlr_output_manager_v1;
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
struct wlr_xwayland;
struct wlr_xwayland_surface;
struct wlr_session_lock_manager_v1;
struct wlr_session_lock_surface_v1;
struct wlr_session_lock_v1;
struct wlr_input_method_manager_v2;
struct wlr_input_method_v2;
struct wlr_input_popup_surface_v2;
struct wlr_text_input_manager_v3;
struct wlr_text_input_v3;
struct wlr_virtual_keyboard_manager_v1;
struct wlr_ext_foreign_toplevel_list_v1;
struct wlr_foreign_toplevel_manager_v1;

WAYLIB_SERVER_USE_NAMESPACE

static_assert(std::is_same_v<decltype(std::declval<WServer &>().handle()), wl_display *>);
static_assert(std::is_same_v<decltype(std::declval<WBackend &>().handle()), wlr_backend *>);
static_assert(std::is_same_v<decltype(std::declval<WBackend &>().session()), wlr_session *>);
static_assert(std::is_same_v<decltype(std::declval<WInputDevice &>().handle()), wlr_input_device *>);
static_assert(std::is_constructible_v<WInputDevice, wlr_input_device *>);
static_assert(std::is_same_v<decltype(std::declval<WOutput &>().handle()), wlr_output *>);
static_assert(std::is_same_v<decltype(std::declval<WOutputManagerV1 &>().handle()),
                             wlr_output_manager_v1 *>);
static_assert(std::is_same_v<decltype(std::declval<WOutputLayout &>().handle()), wlr_output_layout *>);
static_assert(std::is_same_v<decltype(std::declval<WCursor &>().handle()), wlr_cursor *>);
static_assert(std::is_same_v<decltype(std::declval<WCursorShapeManagerV1 &>().handle()),
                             wlr_cursor_shape_manager_v1 *>);
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
static_assert(std::is_same_v<decltype(std::declval<WXWayland &>().handle()), wlr_xwayland *>);
static_assert(std::is_same_v<decltype(std::declval<WXWaylandSurface &>().handle()),
                             wlr_xwayland_surface *>);
static_assert(std::is_same_v<decltype(std::declval<WInputMethodManagerV2 &>().handle()),
                             wlr_input_method_manager_v2 *>);
static_assert(std::is_same_v<decltype(std::declval<WInputMethodV2 &>().handle()),
                             wlr_input_method_v2 *>);
static_assert(std::is_same_v<decltype(std::declval<WInputPopupSurface &>().handle()),
                             wlr_input_popup_surface_v2 *>);
static_assert(std::is_same_v<decltype(std::declval<WTextInputManagerV3 &>().handle()),
                             wlr_text_input_manager_v3 *>);
static_assert(std::is_same_v<decltype(std::declval<WTextInputV3 &>().handle()),
                             wlr_text_input_v3 *>);
static_assert(std::is_same_v<decltype(std::declval<WVirtualKeyboardManagerV1 &>().handle()),
                             wlr_virtual_keyboard_manager_v1 *>);
static_assert(std::is_same_v<decltype(std::declval<WExtForeignToplevelListV1 &>().handle()),
                             wlr_ext_foreign_toplevel_list_v1 *>);
static_assert(std::is_same_v<decltype(std::declval<WForeignToplevel &>().handle()),
                             wlr_foreign_toplevel_manager_v1 *>);

static void destroyTestBuffer(wlr_buffer *buffer)
{
    wlr_buffer_finish(buffer);
}

static const wlr_buffer_impl testBufferImpl = {
    .destroy = destroyTestBuffer,
    .get_dmabuf = nullptr,
    .get_shm = nullptr,
    .begin_data_ptr_access = nullptr,
    .end_data_ptr_access = nullptr,
};

class NativeHandlesTest : public QObject
{
    Q_OBJECT

    static void initXWaylandSurface(wlr_xwayland_surface *surface)
    {
        wl_list_init(&surface->children);
        for (auto *signal : {
                 &surface->events.destroy,
                 &surface->events.associate,
                 &surface->events.dissociate,
                 &surface->events.set_parent,
                 &surface->events.request_activate,
                 &surface->events.request_configure,
                 &surface->events.request_fullscreen,
                 &surface->events.request_maximize,
                 &surface->events.request_minimize,
                 &surface->events.request_move,
                 &surface->events.request_resize,
                 &surface->events.set_override_redirect,
                 &surface->events.set_geometry,
                 &surface->events.set_hints,
                 &surface->events.set_window_type,
                 &surface->events.set_decorations,
                 &surface->events.set_title,
                 &surface->events.set_class,
             }) {
            wl_signal_init(signal);
        }
    }

private Q_SLOTS:
    void serverExposesNativeDisplay()
    {
        WServer server;
        QVERIFY(server.handle());
    }

    void bufferCountTracksNativeLifetime()
    {
        const auto initialCount = waylib_buffer_get_count();
        wlr_buffer buffer {};
        wlr_buffer_init(&buffer, &testBufferImpl, 1, 1);
        QCOMPARE(waylib_buffer_get_count(), initialCount + 1);

        wlr_buffer_drop(&buffer);

        QCOMPARE(waylib_buffer_get_count(), initialCount);
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
        auto *cursorShapeManager = server.attach<WCursorShapeManagerV1>();
        auto *foreignToplevelManager = server.attach<WForeignToplevel>();
        auto *extForeignToplevelList = server.attach<WExtForeignToplevelListV1>();

        server.start();
        QVERIFY(xdgShell->handle());
        QVERIFY(layerShell->handle());
        QVERIFY(decorationManager->handle());
        QVERIFY(dialogManager->handle());
        QVERIFY(sessionLockManager->handle());
        QVERIFY(cursorShapeManager->handle());
        QVERIFY(foreignToplevelManager->handle());
        QVERIFY(extForeignToplevelList->handle());
        server.stop();
    }

    void inputProtocolManagersExposeNativeHandles()
    {
        WServer server;
        auto *inputMethodManager = server.attach<WInputMethodManagerV2>();
        auto *textInputManager = server.attach<WTextInputManagerV3>();
        auto *virtualKeyboardManager = server.attach<WVirtualKeyboardManagerV1>();
        auto *outputManager = server.attach<WOutputManagerV1>();

        server.start();
        QVERIFY(inputMethodManager->handle());
        QVERIFY(textInputManager->handle());
        QVERIFY(virtualKeyboardManager->handle());
        QVERIFY(outputManager->handle());
        server.stop();
    }

    void inputProtocolObjectsTrackNativeLifetime()
    {
        wlr_input_method_v2 nativeInputMethod {};
        wl_signal_init(&nativeInputMethod.events.commit);
        wl_signal_init(&nativeInputMethod.events.grab_keyboard);
        wl_signal_init(&nativeInputMethod.events.new_popup_surface);
        wl_signal_init(&nativeInputMethod.events.destroy);
        QPointer<WInputMethodV2> inputMethod = new WInputMethodV2(&nativeInputMethod);
        // Owner rule: the test created it, so it releases it on destroy.
        // Owner rule: the test acts as the creator and monitors the
        // native destroy signal to release the wrapper.
        bool inputMethodDestroyed = false;
        QObject::connect(inputMethod, &QObject::destroyed, this, [&inputMethodDestroyed] { inputMethodDestroyed = true; });
        WScopedListener inputMethodDestroyListener;
        inputMethodDestroyListener.init(&nativeInputMethod.events.destroy, this,
            [inputMethod, &inputMethodDestroyListener](void *) { inputMethodDestroyListener.disconnect(); delete inputMethod; });

        wl_signal_emit_mutable(&nativeInputMethod.events.destroy, &nativeInputMethod);

        QVERIFY(inputMethodDestroyed);
        QVERIFY(inputMethod.isNull());

        wlr_input_popup_surface_v2 nativePopup {};
        wl_signal_init(&nativePopup.events.destroy);
        QPointer<WInputPopupSurface> popup = new WInputPopupSurface(&nativePopup, nullptr);
        // Owner rule: the test created it, so it releases it on destroy.
        // Owner rule: the test acts as the creator and monitors the
        // native destroy signal to release the wrapper.
        bool popupDestroyed = false;
        QObject::connect(popup, &QObject::destroyed, this, [&popupDestroyed] { popupDestroyed = true; });
        WScopedListener popupDestroyListener;
        popupDestroyListener.init(&nativePopup.events.destroy, this,
            [popup, &popupDestroyListener](void *) { popupDestroyListener.disconnect(); delete popup; });

        wl_signal_emit_mutable(&nativePopup.events.destroy, &nativePopup);

        QVERIFY(popupDestroyed);
        QVERIFY(popup.isNull());

        wlr_text_input_v3 nativeTextInput {};
        wl_signal_init(&nativeTextInput.events.enable);
        wl_signal_init(&nativeTextInput.events.disable);
        wl_signal_init(&nativeTextInput.events.commit);
        wl_signal_init(&nativeTextInput.events.destroy);
        QPointer<WTextInputV3> textInput = new WTextInputV3(&nativeTextInput, nullptr);
        QSignalSpy textInputDestroyed(textInput, &WTextInput::entityAboutToDestroy);
        // Owner rule: the test acts as the creator and monitors the
        // native destroy signal to release the wrapper, mirroring
        // WTextInputManagerV3 (WTextInputV3 itself does not observe
        // the native destroy signal).
        WScopedListener textInputDestroyListener;
        textInputDestroyListener.init(&nativeTextInput.events.destroy, this,
            [textInput, &textInputDestroyListener](void *) {
                textInputDestroyListener.disconnect();
                Q_EMIT textInput->entityAboutToDestroy();
                delete textInput;
            });

        wl_signal_emit_mutable(&nativeTextInput.events.destroy, &nativeTextInput);

        QCOMPARE(textInputDestroyed.count(), 1);
        QVERIFY(textInput.isNull());
    }

    void inputProtocolObserversDetachBeforeNativeDestruction()
    {
        wlr_input_method_v2 nativeInputMethod {};
        wl_signal_init(&nativeInputMethod.events.commit);
        wl_signal_init(&nativeInputMethod.events.grab_keyboard);
        wl_signal_init(&nativeInputMethod.events.new_popup_surface);
        wl_signal_init(&nativeInputMethod.events.destroy);
        {
            auto *im = new WInputMethodV2(&nativeInputMethod);
            delete im;
        }
        wl_signal_emit_mutable(&nativeInputMethod.events.destroy, &nativeInputMethod);

        wlr_input_popup_surface_v2 nativePopup {};
        wl_signal_init(&nativePopup.events.destroy);
        {
            auto *popup = new WInputPopupSurface(&nativePopup, nullptr);
            delete popup;
        }
        wl_signal_emit_mutable(&nativePopup.events.destroy, &nativePopup);

        wlr_text_input_v3 nativeTextInput {};
        wl_signal_init(&nativeTextInput.events.enable);
        wl_signal_init(&nativeTextInput.events.disable);
        wl_signal_init(&nativeTextInput.events.commit);
        wl_signal_init(&nativeTextInput.events.destroy);
        auto *textInput = new WTextInputV3(&nativeTextInput, nullptr);
        delete textInput;
        wl_signal_emit_mutable(&nativeTextInput.events.destroy, &nativeTextInput);
    }

    void xwaylandSurfaceTracksNativeLifetime()
    {
        wlr_xwayland_surface nativeSurface {};
        initXWaylandSurface(&nativeSurface);
        QPointer<WXWaylandSurface> surface = new WXWaylandSurface(&nativeSurface, nullptr);
        // Owner rule: the test acts as the creator and monitors the native
        // destroy signal to release the wrapper.
        bool surfaceDestroyed = false;
        QObject::connect(surface, &QObject::destroyed, this, [&surfaceDestroyed] { surfaceDestroyed = true; });
        WScopedListener surfaceDestroyListener;
        surfaceDestroyListener.init(&nativeSurface.events.destroy, this,
            [surface, &surfaceDestroyListener](void *) { surfaceDestroyListener.disconnect(); delete surface; });
        QCOMPARE(WXWaylandSurface::fromHandle(&nativeSurface), surface.data());

        wl_signal_emit_mutable(&nativeSurface.events.destroy, &nativeSurface);

        QVERIFY(surfaceDestroyed);
        QVERIFY(!WXWaylandSurface::fromHandle(&nativeSurface));
        QVERIFY(surface.isNull());
    }

    void xwaylandSurfaceObserverDetachesBeforeNativeDestruction()
    {
        wlr_xwayland_surface nativeSurface {};
        initXWaylandSurface(&nativeSurface);
        {
            auto *surface = new WXWaylandSurface(&nativeSurface, nullptr);
            delete surface;
        }
        QVERIFY(!WXWaylandSurface::fromHandle(&nativeSurface));
        wl_signal_emit_mutable(&nativeSurface.events.destroy, &nativeSurface);
    }

    void inputDeviceTracksNativeLifetime()
    {
        wlr_input_device nativeDevice;
        wlr_input_device_init(&nativeDevice, WLR_INPUT_DEVICE_POINTER, "test-pointer");

        QPointer<WInputDevice> device = new WInputDevice(&nativeDevice);
        // Owner rule: the test acts as the creator and monitors the native
        // destroy signal to release the wrapper.
        bool deviceDestroyed = false;
        QObject::connect(device, &QObject::destroyed, this, [&deviceDestroyed] { deviceDestroyed = true; });
        WScopedListener deviceDestroyListener;
        deviceDestroyListener.init(&nativeDevice.events.destroy, this,
            [device, &deviceDestroyListener](void *) { deviceDestroyListener.disconnect(); delete device; });
        QCOMPARE(WInputDevice::fromHandle(&nativeDevice), device.data());

        wlr_input_device_finish(&nativeDevice);

        QVERIFY(deviceDestroyed);
        QVERIFY(!WInputDevice::fromHandle(&nativeDevice));
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
        QPointer<WCursor> cursor;
        wlr_cursor *nativeCursor = nullptr;
        {
            cursor = new WCursor();
            nativeCursor = cursor->handle();
            QVERIFY(nativeCursor);
            QCOMPARE(WCursor::fromHandle(nativeCursor), cursor.data());
            // Owner rule: the test created it, so it releases it.
            delete cursor;
        }
        // Do not read nativeCursor->data here: the native cursor is already
        // destroyed (freed), so fromHandle() would be a use-after-free.
        QVERIFY(cursor.isNull());
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
        // WM-194: WServer::stop() no longer deletes interfaces — the wrapper
        // survives for restart. Only the native handle is destroyed and the
        // reverse fromHandle() mapping is cleared before wlr_seat_destroy().
        QVERIFY(!seat.isNull());              // wrapper object survives stop()
        QVERIFY(!seat->handle());             // native wlr_seat destroyed
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
        // Owner rule: the test acts as the creator and monitors the native
        // destroy signal to release the wrapper.
        bool outputDestroyed = false;
        QObject::connect(output, &QObject::destroyed, this, [&outputDestroyed] { outputDestroyed = true; });
        WScopedListener outputDestroyListener;
        outputDestroyListener.init(&nativeOutput->events.destroy, this,
            [output, &outputDestroyListener](void *) {
                outputDestroyListener.disconnect();
                delete output;
            });
        QCOMPARE(output->handle(), nativeOutput);
        QCOMPARE(WOutput::fromHandle(nativeOutput), output.data());

        // Headless backend does not emit the destroy signal inside
        // wlr_output_destroy() (it asserts empty listener lists in
        // wlr_output_finish instead), so simulate the native destruction.
        wl_signal_emit_mutable(&nativeOutput->events.destroy, nativeOutput);

        QVERIFY(outputDestroyed);
        QVERIFY(!WOutput::fromHandle(nativeOutput));
        QVERIFY(output.isNull());

        wlr_output_destroy(nativeOutput);
        wlr_backend_destroy(nativeBackend);
    }
};

QTEST_MAIN(NativeHandlesTest)
#include "main.moc"
