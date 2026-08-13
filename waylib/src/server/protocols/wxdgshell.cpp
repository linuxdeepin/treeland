// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxdgshell.h"
#include "wxdgtoplevelsurface.h"
#include "wxdgpopupsurface.h"
#include "wscoplistener.h"
#include "private/wglobal_p.h"

#include <wlr_all.h>

#include <QPointer>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WXdgShellPrivate : public WObjectPrivate
{
public:
    WXdgShellPrivate(WXdgShell *qq)
        : WObjectPrivate(qq)
    {

    }

    // begin slot function
    void onNewXdgToplevelSurface(wlr_xdg_toplevel *toplevel);
    void onToplevelSurfaceDestroy(WXdgToplevelSurface *surface);
    void onNewXdgPopupSurface(wlr_xdg_popup *popup);
    void onPopupSurfaceDestroy(WXdgPopupSurface *surface);
    // end slot function

    W_DECLARE_PUBLIC(WXdgShell)

    QVector<WXdgToplevelSurface*> toplevelSurfaceList;
    QVector<WXdgPopupSurface*> popupSurfaceList;
    uint32_t version = 0;
};

void WXdgShellPrivate::onNewXdgToplevelSurface(wlr_xdg_toplevel *toplevel)
{
    W_Q(WXdgShell);
    auto surface = new WXdgToplevelSurface(toplevel);
    auto *xdgSurface = toplevel->base;
    // Register the destroy/new_popup listeners on the surface wrapper
    // (owner = this shell): released automatically by ~WObject when the
    // wrapper is deleted.
    surface->listeners(q_ptr)->add(&toplevel->events.destroy, this,
        [this, surface] (void *) {
        // Detach our own listener group: safe from inside a callback of
        // this list (the closure outlives the emission).
        surface->removeListeners(q_ptr);
        onToplevelSurfaceDestroy(surface);
    });
    surface->listeners(q_ptr)->add(&xdgSurface->events.new_popup, this,
        &WXdgShellPrivate::onNewXdgPopupSurface);
    toplevelSurfaceList.append(surface);
    Q_EMIT q->toplevelSurfaceAdded(surface);
}

void WXdgShellPrivate::onToplevelSurfaceDestroy(WXdgToplevelSurface *surface)
{
    Q_ASSERT(surface);
    bool ok = toplevelSurfaceList.removeOne(surface);
    Q_ASSERT(ok);
    Q_EMIT q_func()->toplevelSurfaceRemoved(surface);
    delete surface;
}

void WXdgShellPrivate::onNewXdgPopupSurface(wlr_xdg_popup *popup)
{
    W_Q(WXdgShell);
    auto surface = new WXdgPopupSurface(popup);
    auto *xdgSurface = popup->base;
    // Register the destroy/new_popup listeners on the surface wrapper
    // (owner = this shell): released automatically by ~WObject when the
    // wrapper is deleted.
    surface->listeners(q_ptr)->add(&popup->events.destroy, this,
        [this, surface] (void *) {
        // Detach our own listener group: safe from inside a callback of
        // this list (the closure outlives the emission).
        surface->removeListeners(q_ptr);
        onPopupSurfaceDestroy(surface);
    });
    surface->listeners(q_ptr)->add(&xdgSurface->events.new_popup, this,
        &WXdgShellPrivate::onNewXdgPopupSurface);
    popupSurfaceList.append(surface);
    Q_EMIT q->popupSurfaceAdded(surface);
}

void WXdgShellPrivate::onPopupSurfaceDestroy(WXdgPopupSurface *surface)
{
    Q_ASSERT(surface);
    bool ok = popupSurfaceList.removeOne(surface);
    Q_ASSERT(ok);
    Q_EMIT q_func()->popupSurfaceRemoved(surface);
    delete surface;
}

WXdgShell::WXdgShell(uint32_t version)
    : WObject(*new WXdgShellPrivate(this))
{
    W_D(WXdgShell);
    d->version = version;
}

QVector<WXdgToplevelSurface*> WXdgShell::toplevelSurfaceList() const
{
    W_DC(WXdgShell);
    return d->toplevelSurfaceList;
}

QByteArrayView WXdgShell::interfaceName() const
{
    return "xdg_wm_base";
}

wlr_xdg_shell *WXdgShell::handle() const
{
    return reinterpret_cast<wlr_xdg_shell*>(m_handle);
}

void WXdgShell::initializeNewXdgPopupSurface(wlr_xdg_popup *popup)
{
    W_D(WXdgShell);
    d->onNewXdgPopupSurface(popup);
}

void WXdgShell::create(WServer *server)
{
    W_D(WXdgShell);
    // free follow display
    auto xdg_shell = wlr_xdg_shell_create(server->handle(), d->version);
    Q_ASSERT(xdg_shell);
    listeners()->add(&xdg_shell->events.new_toplevel, d,
        &WXdgShellPrivate::onNewXdgToplevelSurface);

    // When layer_surface is an xdg_popup's parent, the popup should created via xdg_surface::get_popup with the parent set to NULL,
    // and invoke 'zwlr_layer_surface_v1::get_popup' to set parent before committing the popup's initial state.

    // Popups are delivered through each xdg_surface's `new_popup` event (see
    // onNewXdgToplevelSurface/onNewXdgPopupSurface). Do NOT also listen on
    // xdg_shell->events.new_popup: wlroots emits it for every popup, so every
    // popup would be wrapped twice.
    m_handle = xdg_shell;
}

void WXdgShell::destroy([[maybe_unused]] WServer *server)
{
    W_D(WXdgShell);

    QVector<WXdgToplevelSurface*> toplevelList;
    QVector<WXdgPopupSurface*> popupList;
    d->toplevelSurfaceList.swap(toplevelList);
    d->popupSurfaceList.swap(popupList);

    for (auto surface : std::as_const(toplevelList)) {
        Q_EMIT toplevelSurfaceRemoved(surface);
        delete surface;
    }
    for (auto surface : std::as_const(popupList)) {
        Q_EMIT popupSurfaceRemoved(surface);
        delete surface;
    }

    // Clear the dangling handle now: the wlr_xdg_shell is reclaimed by
    // display.reset() in WServer::stop(), but nulling m_handle immediately
    // makes handle()/global() return null instead of a dangling pointer.
    m_handle = nullptr;
}

wl_global *WXdgShell::global() const
{
    if (!m_handle)
        return nullptr;
    auto handle = reinterpret_cast<wlr_xdg_shell*>(m_handle);
    return handle->global;
}

WAYLIB_SERVER_END_NAMESPACE
