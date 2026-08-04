// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxdgshell.h"
#include "wxdgtoplevelsurface.h"
#include "wxdgpopupsurface.h"
#include "private/wglobal_p.h"

#include <wlr/types/wlr_xdg_shell.h>

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
    void onToplevelSurfaceDestroy(wlr_xdg_toplevel *toplevel);
    void onNewXdgPopupSurface(wlr_xdg_popup *popup);
    void onPopupSurfaceDestroy(wlr_xdg_popup *popup);
    // end slot function

    W_DECLARE_PUBLIC(WXdgShell)

    QVector<WXdgToplevelSurface*> toplevelSurfaceList;
    QVector<WXdgPopupSurface*> popupSurfaceList;
    uint32_t version;

    WScopedListener m_newToplevelListener;
    WScopedListener m_newPopupListener;
};

void WXdgShellPrivate::onNewXdgToplevelSurface(wlr_xdg_toplevel *toplevel)
{
    W_Q(WXdgShell);
    auto server = q_func()->server();
    auto surface = new WXdgToplevelSurface(toplevel, server);
    surface->setParent(server);
    Q_ASSERT(surface->parent() == server);
    surface->safeConnect(&WXdgToplevelSurface::aboutToBeInvalidated, q, [this, toplevel] {
       onToplevelSurfaceDestroy(toplevel);
    });
    toplevelSurfaceList.append(surface);
    Q_EMIT q_func()->toplevelSurfaceAdded(surface);
}

void WXdgShellPrivate::onToplevelSurfaceDestroy(wlr_xdg_toplevel *toplevel)
{
    auto surface = WXdgToplevelSurface::fromHandle(toplevel);
    Q_ASSERT(surface);
    bool ok = toplevelSurfaceList.removeOne(surface);
    Q_ASSERT(ok);
    Q_EMIT q_func()->toplevelSurfaceRemoved(surface);
    surface->safeDeleteLater();
}

void WXdgShellPrivate::onNewXdgPopupSurface(wlr_xdg_popup *popup)
{
    W_Q(WXdgShell);
    auto server = q_func()->server();
    auto surface = new WXdgPopupSurface(popup, server);
    surface->setParent(server);
    Q_ASSERT(surface->parent() == server);
    surface->safeConnect(&WXdgPopupSurface::aboutToBeInvalidated, q, [this, popup] {
        onPopupSurfaceDestroy(popup);
    });
    popupSurfaceList.append(surface);
    Q_EMIT q_func()->popupSurfaceAdded(surface);
}

void WXdgShellPrivate::onPopupSurfaceDestroy(wlr_xdg_popup *popup)
{
    auto surface = WXdgPopupSurface::fromHandle(popup);
    Q_ASSERT(surface);
    bool ok = popupSurfaceList.removeOne(surface);
    Q_ASSERT(ok);
    Q_EMIT q_func()->popupSurfaceRemoved(surface);
    surface->safeDeleteLater();
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

void WXdgShell::initializeNewXdgPopupSurface(wlr_xdg_popup *popup)
{
    W_D(WXdgShell);
    d->onNewXdgPopupSurface(popup);
}

void WXdgShell::create(WServer *server)
{
    W_D(WXdgShell);
    m_handle = wlr_xdg_shell_create(server->handle(), d->version);
    auto *shell = static_cast<wlr_xdg_shell*>(m_handle);

    d->m_newToplevelListener.connect(&shell->events.new_toplevel, [](wl_listener *listener, void *data) {
        auto *self = WScopedListener::owner<WXdgShellPrivate, &WXdgShellPrivate::m_newToplevelListener>(listener);
        self->onNewXdgToplevelSurface(static_cast<wlr_xdg_toplevel*>(data));
    });

    d->m_newPopupListener.connect(&shell->events.new_popup, [d](wl_listener *listener, void *data) {
        auto *self = WScopedListener::owner<WXdgShellPrivate, &WXdgShellPrivate::m_newPopupListener>(listener);
        // When layer_surface is an xdg_popup's parent, the popup should created via xdg_surface::get_popup
        // with the parent set to NULL, and invoke 'zwlr_layer_surface_v1::get_popup' to set parent
        // before committing the popup's initial state.
        // We use parent's notify_new_popup to avoid getting a popup with NULL parent
        auto *xdgSurface = static_cast<wlr_xdg_surface*>(data);
        if (xdgSurface->popup) {
            self->onNewXdgPopupSurface(xdgSurface->popup);
        }
    });
}

void WXdgShell::destroy([[maybe_unused]] WServer *server)
{
    W_D(WXdgShell);

    d->m_newToplevelListener.remove();
    d->m_newPopupListener.remove();

    QVector<WXdgToplevelSurface*> toplevelList;
    QVector<WXdgPopupSurface*> popupList;

    d->toplevelSurfaceList.swap(toplevelList);
    d->popupSurfaceList.swap(popupList);

    for (auto surface : std::as_const(toplevelList)) {
        toplevelSurfaceRemoved(surface);
        surface->safeDeleteLater();
    }
    for (auto surface : std::as_const(popupList)) {
        popupSurfaceRemoved(surface);
        surface->safeDeleteLater();
    }
}

wl_global *WXdgShell::global() const
{
    return static_cast<wlr_xdg_shell*>(m_handle)->global;
}

WAYLIB_SERVER_END_NAMESPACE
