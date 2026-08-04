// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <WSurface>
#include <WLayerSurface>
#include <WSessionLock>
#include <WSessionLockSurface>
#include <wxdgpopupsurface.h>
#include <wxdgtoplevelsurface.h>

#include <QPointer>
#include <QSignalSpy>
#include <QTest>

extern "C" {
#include <wlr/types/wlr_compositor.h>
#define namespace scope
#include <wlr/types/wlr_layer_shell_v1.h>
#undef namespace
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_session_lock_v1.h>
}

WAYLIB_SERVER_USE_NAMESPACE

namespace {
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
    void nativeSurfaceDestroyedFirst()
    {
        wlr_surface nativeSurface;
        initSurface(&nativeSurface);
        QPointer<WSurface> surface = new WSurface(&nativeSurface);
        QCOMPARE(WSurface::fromHandle(&nativeSurface), surface.data());

        QSignalSpy aboutToBeInvalidated(surface, &WSurface::aboutToBeInvalidated);
        QSignalSpy invalidated(surface, &WSurface::invalidated);
        wl_signal_emit_mutable(&nativeSurface.events.destroy, &nativeSurface);

        QCOMPARE(aboutToBeInvalidated.count(), 1);
        QCOMPARE(invalidated.count(), 1);
        QVERIFY(!WSurface::fromHandle(&nativeSurface));
        QVERIFY(surface);
        QVERIFY(surface->isInvalidated());

        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
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
        QCOMPARE(WXdgToplevelSurface::fromHandle(&nativeToplevel), surface.data());
        QSignalSpy invalidated(surface, &WWrapObject::invalidated);

        wl_signal_emit_mutable(&nativeToplevel.events.destroy, &nativeToplevel);

        QCOMPARE(invalidated.count(), 1);
        QVERIFY(!WXdgToplevelSurface::fromHandle(&nativeToplevel));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
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
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
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
        QCOMPARE(WXdgPopupSurface::fromHandle(&nativePopup), surface.data());
        wl_signal_emit_mutable(&nativePopup.events.destroy, &nativePopup);

        QVERIFY(!WXdgPopupSurface::fromHandle(&nativePopup));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
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
        QCOMPARE(WLayerSurface::fromHandle(&nativeLayerSurface), surface.data());
        wl_signal_emit_mutable(&nativeLayerSurface.events.destroy, &nativeLayerSurface);

        QVERIFY(!WLayerSurface::fromHandle(&nativeLayerSurface));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
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
        QCOMPARE(WSessionLock::fromHandle(&nativeLock), lock.data());
        QSignalSpy canceled(lock, &WSessionLock::canceled);
        wl_signal_emit_mutable(&nativeLock.events.destroy, &nativeLock);

        QCOMPARE(canceled.count(), 1);
        QVERIFY(!WSessionLock::fromHandle(&nativeLock));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
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
        QCOMPARE(WSessionLockSurface::fromHandle(&nativeLockSurface), surface.data());
        wl_signal_emit_mutable(&nativeLockSurface.events.destroy, &nativeLockSurface);

        QVERIFY(!WSessionLockSurface::fromHandle(&nativeLockSurface));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(surface.isNull());
    }
};

QTEST_MAIN(NativeLifecycleTest)
#include "main.moc"
