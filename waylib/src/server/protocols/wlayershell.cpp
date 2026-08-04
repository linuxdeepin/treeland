// Copyright (C) 2023-2024 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wlayershell.h"
#include "wlayersurface.h"
#include "woutput.h"
#include "wayliblogging.h"
#include "private/wglobal_p.h"
#include "wxdgshell.h"

#include <wlr/types/wlr_layer_shell_v1.h>

#include <QVector>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WLayerShellPrivate : public WWrapObjectPrivate
{
public:
    WLayerShellPrivate(WLayerShell *qq)
        : WWrapObjectPrivate(qq)
    {

    }

    // begin slot function
    void onNewSurface(wlr_layer_surface_v1 *layerSurface);
    void onSurfaceDestroy(wlr_layer_surface_v1 *layerSurface);
    // end slot function

    W_DECLARE_PUBLIC(WLayerShell)

    WScopedListener m_newSurfaceListener;
    QVector<WLayerSurface*> surfaceList;
    QPointer<WXdgShell> xdgShell;
};

void WLayerShellPrivate::onNewSurface(wlr_layer_surface_v1 *layerSurface)
{
    W_Q(WLayerShell);

    auto server = q->server();
    auto surface = new WLayerSurface(layerSurface, server);
    surface->setParent(server);
    Q_ASSERT(surface->parent() == server);

    surface->safeConnect(&WLayerSurface::aboutToBeInvalidated, q, [this, layerSurface] {
        onSurfaceDestroy(layerSurface);
    });
    QObject::connect(surface, &WLayerSurface::newPopup, q, [this] (wlr_xdg_popup *popup) {
        if (xdgShell)
            xdgShell->initializeNewXdgPopupSurface(popup);
        else
            qCWarning(lcWlLayerShell) << "Xdg shell not set, will ignore the layer surface's popup request!";
    });

    surfaceList.append(surface);
    Q_EMIT q->surfaceAdded(surface);
}

void WLayerShellPrivate::onSurfaceDestroy(wlr_layer_surface_v1 *layerSurface)
{
    auto surface = WLayerSurface::fromHandle(layerSurface);
    Q_ASSERT(surface);
    bool ok = surfaceList.removeOne(surface);
    Q_ASSERT(ok);
    Q_EMIT q_func()->surfaceRemoved(surface);
    surface->safeDeleteLater();
}

WLayerShell::WLayerShell(WXdgShell *xdgshell, [[maybe_unused]] QObject *parent):
    WWrapObject(*new WLayerShellPrivate(this), nullptr)
{
    W_D(WLayerShell);
    d->xdgShell = xdgshell;
}

QVector<WLayerSurface*> WLayerShell::surfaceList() const
{
    W_DC(WLayerShell);
    return d->surfaceList;
}

QByteArrayView WLayerShell::interfaceName() const
{
    return "zwlr_layer_shell_v1";
}

void WLayerShell::create(WServer *server)
{
    W_D(WLayerShell);

    m_handle = wlr_layer_shell_v1_create(server->handle(), 4);
    auto *layer_shell = static_cast<wlr_layer_shell_v1*>(m_handle);
    d->m_newSurfaceListener.connect(&layer_shell->events.new_surface, [d](wl_listener *, void *data) {
        d->onNewSurface(static_cast<wlr_layer_surface_v1*>(data));
    });
}

void WLayerShell::destroy([[maybe_unused]] WServer *server)
{
    W_D(WLayerShell);
    d->m_newSurfaceListener.remove();

    auto list = d->surfaceList;
    d->surfaceList.clear();

    for (auto surface : std::as_const(list)) {
        surfaceRemoved(surface);
        surface->safeDeleteLater();
    }
}

wl_global *WLayerShell::global() const
{
    return static_cast<wlr_layer_shell_v1*>(m_handle)->global;
}

WAYLIB_SERVER_END_NAMESPACE
