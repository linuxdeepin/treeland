// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <WSurface>

#include <QPointer>
#include <QSignalSpy>
#include <QTest>

extern "C" {
#include <wlr/types/wlr_compositor.h>
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
};

QTEST_MAIN(NativeLifecycleTest)
#include "main.moc"
