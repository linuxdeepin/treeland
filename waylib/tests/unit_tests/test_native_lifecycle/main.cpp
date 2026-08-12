// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <WSurface>
#include <WLayerSurface>
#include <WSessionLock>
#include <WSessionLockSurface>
#include <wimagebuffer.h>
#include <wscoplistener.h>
#include <wxdgpopupsurface.h>
#include <wxdgtoplevelsurface.h>

#include <QPointer>
#include <QImage>
#include <QSignalSpy>
#include <QTest>

extern "C" {
#include <wlr/types/wlr_compositor.h>
#define namespace scope
#include <wlr/types/wlr_layer_shell_v1.h>
#undef namespace
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_session_lock_v1.h>
#include <wlr/types/wlr_buffer.h>
}

WAYLIB_SERVER_USE_NAMESPACE

namespace {
struct BufferDestroyProbe {
    wl_listener listener;
    bool destroyed = false;

    BufferDestroyProbe()
    {
        listener.notify = notify;
        wl_list_init(&listener.link);
    }

    static void notify(wl_listener *listener, void *)
    {
        BufferDestroyProbe *probe;
        probe = wl_container_of(listener, probe, listener);
        probe->destroyed = true;
        wl_list_remove(&probe->listener.link);
        wl_list_init(&probe->listener.link);
    }
};

void initSurface(wlr_surface *surface)
{
    *surface = {};
    wl_signal_init(&surface->events.client_commit);
    wl_signal_init(&surface->events.commit);
    wl_signal_init(&surface->events.map);
    wl_signal_init(&surface->events.unmap);
    wl_signal_init(&surface->events.new_subsurface);
    wl_signal_init(&surface->events.destroy);
    wl_list_init(&surface->current.frame_callback_list);
    wl_list_init(&surface->current.subsurfaces_below);
    wl_list_init(&surface->current.subsurfaces_above);
    wl_list_init(&surface->current_outputs);
}

void initXdgSurface(wlr_xdg_surface *surface, wlr_surface *nativeSurface,
                    wlr_xdg_client *client)
{
    *surface = {};
    surface->surface = nativeSurface;
    surface->client = client;
    wl_signal_init(&surface->events.configure);
    wl_signal_init(&surface->events.new_popup);
}

void initToplevel(wlr_xdg_toplevel *toplevel, wlr_xdg_surface *surface)
{
    *toplevel = {};
    toplevel->base = surface;
    wl_signal_init(&toplevel->events.destroy);
    wl_signal_init(&toplevel->events.request_maximize);
    wl_signal_init(&toplevel->events.request_fullscreen);
    wl_signal_init(&toplevel->events.request_minimize);
    wl_signal_init(&toplevel->events.request_move);
    wl_signal_init(&toplevel->events.request_resize);
    wl_signal_init(&toplevel->events.request_show_window_menu);
    wl_signal_init(&toplevel->events.set_parent);
    wl_signal_init(&toplevel->events.set_title);
    wl_signal_init(&toplevel->events.set_app_id);
}
}

class NativeLifecycleTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void imageBufferHonorsNativeLockLifetime()
    {
        QImage image(16, 12, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::red);
        auto *buffer = WImageBufferImpl::create(image);
        QVERIFY(buffer);
        QCOMPARE(buffer->width, image.width());
        QCOMPARE(buffer->height, image.height());

        wlr_shm_attributes attributes {};
        // WImageBuffer is a CPU-memory buffer: it has no shared-memory fd, so
        // it must not advertise fabricated SHM attributes (fd 0 would alias
        // stdin). Its capability is the data pointer, verified below.
        QVERIFY(!wlr_buffer_get_shm(buffer, &attributes));

        void *data = nullptr;
        uint32_t format = 0;
        size_t stride = 0;
        QVERIFY(wlr_buffer_begin_data_ptr_access(buffer, WLR_BUFFER_DATA_PTR_ACCESS_READ,
                                                 &data, &format, &stride));
        QCOMPARE(stride, image.bytesPerLine());
        wlr_buffer_end_data_ptr_access(buffer);

        BufferDestroyProbe probe;
        wl_signal_add(&buffer->events.destroy, &probe.listener);
        QCOMPARE(wlr_buffer_lock(buffer), buffer);
        wlr_buffer_drop(buffer);
        QVERIFY(!probe.destroyed);
        wlr_buffer_unlock(buffer);
        QVERIFY(probe.destroyed);
    }

    void nativeSurfaceDestroyedFirst()
    {
        wlr_surface nativeSurface;
        initSurface(&nativeSurface);
        QPointer<WSurface> surface = new WSurface(&nativeSurface);
        // Owner rule: the test acts as the creator and monitors the native
        // destroy signal to release the wrapper.
        bool surfaceDestroyed = false;
        QObject::connect(surface, &QObject::destroyed, this,
            [&surfaceDestroyed] { surfaceDestroyed = true; });
        bool handleAvailableDuringDestroy = false;
        QObject::connect(surface, &WSurface::beforeDestroy, this,
            [surface, &handleAvailableDuringDestroy, &nativeSurface] {
                handleAvailableDuringDestroy = surface
                    && surface->handle() == &nativeSurface;
            });
        WScopedListener surfaceDestroyListener;
        surfaceDestroyListener.init(&nativeSurface.events.destroy, this,
            [surface, &surfaceDestroyListener](void *) { surfaceDestroyListener.disconnect(); delete surface; });
        QCOMPARE(WSurface::fromHandle(&nativeSurface), surface.data());

        wl_signal_emit_mutable(&nativeSurface.events.destroy, &nativeSurface);
        QVERIFY(handleAvailableDuringDestroy);

        QVERIFY(surfaceDestroyed);
        QVERIFY(!WSurface::fromHandle(&nativeSurface));
        QVERIFY(surface.isNull());
    }

    void surfaceObserverDestroyedFirst()
    {
        wlr_surface nativeSurface;
        initSurface(&nativeSurface);
        auto *surface = new WSurface(&nativeSurface);
        QCOMPARE(WSurface::fromHandle(&nativeSurface), surface);

        delete surface;
        QVERIFY(!WSurface::fromHandle(&nativeSurface));

        // Every native listener must already be unlinked.
        wl_signal_emit_mutable(&nativeSurface.events.destroy, &nativeSurface);
    }

    void nativeXdgToplevelDestroyedFirst()
    {
        wlr_surface nativeSurface;
        initSurface(&nativeSurface);
        wlr_xdg_client client = {};
        wlr_xdg_surface xdgSurface;
        initXdgSurface(&xdgSurface, &nativeSurface, &client);
        wlr_xdg_toplevel nativeToplevel;
        initToplevel(&nativeToplevel, &xdgSurface);

        QPointer<WXdgToplevelSurface> surface = new WXdgToplevelSurface(&nativeToplevel);
        // Owner rule: the test acts as the creator and monitors the native
        // destroy signal to release the wrapper.
        bool surfaceDestroyed = false;
        QObject::connect(surface, &QObject::destroyed, this, [&surfaceDestroyed] { surfaceDestroyed = true; });
        WScopedListener surfaceDestroyListener;
        surfaceDestroyListener.init(&nativeToplevel.events.destroy, this,
            [surface, &surfaceDestroyListener](void *) { surfaceDestroyListener.disconnect(); delete surface; });
        QCOMPARE(WXdgToplevelSurface::fromHandle(&nativeToplevel), surface.data());

        wl_signal_emit_mutable(&nativeToplevel.events.destroy, &nativeToplevel);

        QVERIFY(surfaceDestroyed);
        QVERIFY(!WXdgToplevelSurface::fromHandle(&nativeToplevel));
        QVERIFY(surface.isNull());
    }

    void xdgToplevelObserverDestroyedFirst()
    {
        wlr_surface nativeSurface;
        initSurface(&nativeSurface);
        wlr_xdg_client client = {};
        wlr_xdg_surface xdgSurface;
        initXdgSurface(&xdgSurface, &nativeSurface, &client);
        wlr_xdg_toplevel nativeToplevel;
        initToplevel(&nativeToplevel, &xdgSurface);

        auto *surface = new WXdgToplevelSurface(&nativeToplevel);
        delete surface;

        QVERIFY(!WXdgToplevelSurface::fromHandle(&nativeToplevel));
        wl_signal_emit_mutable(&nativeToplevel.events.destroy, &nativeToplevel);
    }

    void nativeXdgPopupDestroyedFirst()
    {
        wlr_surface nativeSurface;
        initSurface(&nativeSurface);
        wlr_xdg_client client = {};
        wlr_xdg_surface xdgSurface;
        initXdgSurface(&xdgSurface, &nativeSurface, &client);
        wlr_xdg_popup nativePopup = {};
        nativePopup.base = &xdgSurface;
        wl_signal_init(&nativePopup.events.destroy);
        wl_signal_init(&nativePopup.events.reposition);

        QPointer<WXdgPopupSurface> surface = new WXdgPopupSurface(&nativePopup);
        // Owner rule: the test acts as the creator and monitors the native
        // destroy signal to release the wrapper.
        bool surfaceDestroyed = false;
        QObject::connect(surface, &QObject::destroyed, this, [&surfaceDestroyed] { surfaceDestroyed = true; });
        WScopedListener surfaceDestroyListener;
        surfaceDestroyListener.init(&nativePopup.events.destroy, this,
            [surface, &surfaceDestroyListener](void *) { surfaceDestroyListener.disconnect(); delete surface; });
        QCOMPARE(WXdgPopupSurface::fromHandle(&nativePopup), surface.data());
        wl_signal_emit_mutable(&nativePopup.events.destroy, &nativePopup);

        QVERIFY(surfaceDestroyed);
        QVERIFY(!WXdgPopupSurface::fromHandle(&nativePopup));
        QVERIFY(surface.isNull());
    }

    void nativeLayerSurfaceDestroyedFirst()
    {
        wlr_surface nativeSurface;
        initSurface(&nativeSurface);
        wlr_layer_surface_v1 nativeLayerSurface = {};
        nativeLayerSurface.surface = &nativeSurface;
        wl_signal_init(&nativeLayerSurface.events.destroy);
        wl_signal_init(&nativeLayerSurface.events.new_popup);

        QPointer<WLayerSurface> surface = new WLayerSurface(&nativeLayerSurface);
        // Owner rule: the test acts as the creator and monitors the native
        // destroy signal to release the wrapper.
        bool surfaceDestroyed = false;
        QObject::connect(surface, &QObject::destroyed, this, [&surfaceDestroyed] { surfaceDestroyed = true; });
        WScopedListener surfaceDestroyListener;
        surfaceDestroyListener.init(&nativeLayerSurface.events.destroy, this,
            [surface, &surfaceDestroyListener](void *) { surfaceDestroyListener.disconnect(); delete surface; });
        QCOMPARE(WLayerSurface::fromHandle(&nativeLayerSurface), surface.data());
        wl_signal_emit_mutable(&nativeLayerSurface.events.destroy, &nativeLayerSurface);

        QVERIFY(surfaceDestroyed);
        QVERIFY(!WLayerSurface::fromHandle(&nativeLayerSurface));
        QVERIFY(surface.isNull());
    }

    void nativeSessionLockDestroyedFirst()
    {
        wlr_session_lock_v1 nativeLock = {};
        wl_list_init(&nativeLock.surfaces);
        wl_signal_init(&nativeLock.events.new_surface);
        wl_signal_init(&nativeLock.events.unlock);
        wl_signal_init(&nativeLock.events.destroy);

        QPointer<WSessionLock> lock = new WSessionLock(&nativeLock);
        // Owner rule: the test acts as the creator and monitors the native
        // destroy signal to release the wrapper.
        bool lockDestroyed = false;
        QObject::connect(lock, &QObject::destroyed, this, [&lockDestroyed] { lockDestroyed = true; });
        WScopedListener lockDestroyListener;
        lockDestroyListener.init(&nativeLock.events.destroy, this,
            [lock, &lockDestroyListener](void *) { lockDestroyListener.disconnect(); delete lock; });
        QCOMPARE(WSessionLock::fromHandle(&nativeLock), lock.data());
        QSignalSpy canceled(lock, &WSessionLock::canceled);
        wl_signal_emit_mutable(&nativeLock.events.destroy, &nativeLock);

        QCOMPARE(canceled.count(), 1);
        QVERIFY(lockDestroyed);
        QVERIFY(!WSessionLock::fromHandle(&nativeLock));
        QVERIFY(lock.isNull());
    }

    void nativeSessionLockSurfaceDestroyedFirst()
    {
        wlr_surface nativeSurface;
        initSurface(&nativeSurface);
        wlr_session_lock_surface_v1 nativeLockSurface = {};
        nativeLockSurface.surface = &nativeSurface;
        wl_signal_init(&nativeLockSurface.events.destroy);

        QPointer<WSessionLockSurface> surface = new WSessionLockSurface(&nativeLockSurface);
        // Owner rule: the test acts as the creator and monitors the native
        // destroy signal to release the wrapper.
        bool surfaceDestroyed = false;
        QObject::connect(surface, &QObject::destroyed, this, [&surfaceDestroyed] { surfaceDestroyed = true; });
        WScopedListener surfaceDestroyListener;
        surfaceDestroyListener.init(&nativeLockSurface.events.destroy, this,
            [surface, &surfaceDestroyListener](void *) { surfaceDestroyListener.disconnect(); delete surface; });
        QCOMPARE(WSessionLockSurface::fromHandle(&nativeLockSurface), surface.data());
        wl_signal_emit_mutable(&nativeLockSurface.events.destroy, &nativeLockSurface);

        QVERIFY(surfaceDestroyed);
        QVERIFY(!WSessionLockSurface::fromHandle(&nativeLockSurface));
        QVERIFY(surface.isNull());
    }
};

QTEST_MAIN(NativeLifecycleTest)
#include "main.moc"
