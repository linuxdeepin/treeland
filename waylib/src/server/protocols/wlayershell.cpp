// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wlayershell.h"
#include "wlayersurface.h"
#include "woutput.h"
#include "wscoplistener.h"
#include "wayliblogging.h"
#include "private/wglobal_p.h"
#include "wxdgshell.h"

#include <wlr_all.h>

#include <QVector>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WLayerShellPrivate : public WObjectPrivate
{
public:
    WLayerShellPrivate(WLayerShell *qq)
        : WObjectPrivate(qq)
    {

    }

    // begin slot function
    void onNewSurface(wlr_layer_surface_v1 *layerSurface);
    void onSurfaceDestroy(WLayerSurface *surface);
    // end slot function

    W_DECLARE_PUBLIC(WLayerShell)

    QVector<WLayerSurface*> surfaceList;
    QPointer<WXdgShell> xdgShell;
};

void WLayerShellPrivate::onNewSurface(wlr_layer_surface_v1 *layerSurface)
{
    W_Q(WLayerShell);
    auto surface = new WLayerSurface(layerSurface);
    // Register the destroy/new_popup listeners on the surface wrapper
    // (owner = this shell): each surface owns its own WScopedListenerList
    // in WObject::attachedListenerLists, released automatically by ~WObject
    // when the wrapper is deleted.
    surface->listeners(q_ptr)->add(&layerSurface->events.destroy, this,
        [this, surface] (void *) {
        // Detach our own listener group: wlr_layer_surface destroy asserts
        // the listener lists are empty after emitting. Safe from inside a
        // callback of this list (the closure outlives the emission).
        surface->removeListeners(q_ptr);
        onSurfaceDestroy(surface);
    });
    surface->listeners(q_ptr)->add(&layerSurface->events.new_popup, this,
        [this] (wlr_xdg_popup *popup) {
        if (xdgShell)
            xdgShell->initializeNewXdgPopupSurface(popup);
        else
            qCWarning(lcWlLayerShell) << "Xdg shell not set, will ignore the layer surface's popup request!";
    });
    surfaceList.append(surface);
    Q_EMIT q->surfaceAdded(surface);
}

void WLayerShellPrivate::onSurfaceDestroy(WLayerSurface *surface)
{
    bool ok = surfaceList.removeOne(surface);
    Q_ASSERT(ok);
    Q_EMIT q_func()->surfaceRemoved(surface);
    delete surface;
}

WLayerShell::WLayerShell(WXdgShell *xdgshell):
    QObject(nullptr),
    WObject(*new WLayerShellPrivate(this))
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

wlr_layer_shell_v1 *WLayerShell::handle() const
{
    return reinterpret_cast<wlr_layer_shell_v1*>(m_handle);
}

void WLayerShell::create(WServer *server)
{
    W_D(WLayerShell);

    auto *layer_shell = wlr_layer_shell_v1_create(server->handle(), InterfaceVersion);
    Q_ASSERT(layer_shell);
    listeners()->add(&layer_shell->events.new_surface, d,
        &WLayerShellPrivate::onNewSurface);
    m_handle = layer_shell;
}

void WLayerShell::destroy([[maybe_unused]] WServer *server)
{
    W_D(WLayerShell);

    auto list = d->surfaceList;
    d->surfaceList.clear();
    for (auto surface : std::as_const(list)) {
        Q_EMIT surfaceRemoved(surface);
        delete surface;
    }
    // Clear the dangling handle now: the wlr_layer_shell_v1 is reclaimed by
    // display.reset() in WServer::stop(), but nulling m_handle immediately
    // makes handle()/global() return null instead of a dangling pointer.
    m_handle = nullptr;
}

wl_global *WLayerShell::global() const
{
    if (!m_handle)
        return nullptr;
    auto handle = reinterpret_cast<wlr_layer_shell_v1*>(m_handle);
    return handle->global;
}

WAYLIB_SERVER_END_NAMESPACE
