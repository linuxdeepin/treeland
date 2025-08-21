// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxdgshell.h"
#include "wxdgtoplevelsurface.h"
#include "wxdgpopupsurface.h"
#include "private/wglobal_p.h"

#include <qwxdgshell.h>
#include <qwcompositor.h>
#include <qwdisplay.h>

#include <QPointer>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WXdgShellPrivate : public WObjectPrivate
{
public:
    WXdgShellPrivate(WXdgShell *qq)
        : WObjectPrivate(qq)
    {

    }

    // begin slot function
    void onNewXdgToplevelSurface(qw_xdg_toplevel *toplevel);
    void onToplevelSurfaceDestroy(qw_xdg_toplevel *toplevel);
    void onNewXdgPopupSurface(qw_xdg_popup *popup);
    void onPopupSurfaceDestroy(qw_xdg_popup *popup);
    // end slot function

    W_DECLARE_PUBLIC(WXdgShell)

    QVector<WXdgToplevelSurface*> toplevelSurfaceList;
    QVector<WXdgPopupSurface*> popupSurfaceList;
    uint32_t version;
};

void WXdgShellPrivate::onNewXdgToplevelSurface(qw_xdg_toplevel *toplevel)
{
    W_Q(WXdgShell);
    auto server = q_func()->server();
    auto surface = new WXdgToplevelSurface(toplevel, server);
    surface->setParent(server);
    Q_ASSERT(surface->parent() == server);
    surface->safeConnect(&qw_xdg_surface::before_destroy, q, [this, toplevel] {
       onToplevelSurfaceDestroy(toplevel);
    });
    auto xdgSurface = qw_xdg_surface::from((*toplevel)->base);
    QObject::connect(xdgSurface, &qw_xdg_surface::notify_new_popup, q, &WXdgShell::initializeNewXdgPopupSurface);
    toplevelSurfaceList.append(surface);
    Q_EMIT q_func()->toplevelSurfaceAdded(surface);
}

void WXdgShellPrivate::onToplevelSurfaceDestroy(qw_xdg_toplevel *toplevel)
{
    auto surface = WXdgToplevelSurface::fromHandle(toplevel);
    Q_ASSERT(surface);
    bool ok = toplevelSurfaceList.removeOne(surface);
    Q_ASSERT(ok);
    Q_EMIT q_func()->toplevelSurfaceRemoved(surface);
    surface->safeDeleteLater();
}

void WXdgShellPrivate::onNewXdgPopupSurface(qw_xdg_popup *popup)
{
    W_Q(WXdgShell);
    auto server = q_func()->server();
    auto surface = new WXdgPopupSurface(popup, server);
    surface->setParent(server);
    Q_ASSERT(surface->parent() == server);
    surface->safeConnect(&qw_xdg_popup::before_destroy, q, [this, popup] {
        onPopupSurfaceDestroy(popup);
    });
    auto xdgSurface = qw_xdg_surface::from((*popup)->base);
    QObject::connect(xdgSurface, &qw_xdg_surface::notify_new_popup, q, &WXdgShell::initializeNewXdgPopupSurface);
    popupSurfaceList.append(surface);
    Q_EMIT q_func()->popupSurfaceAdded(surface);
}

void WXdgShellPrivate::onPopupSurfaceDestroy(qw_xdg_popup *popup)
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
    d->onNewXdgPopupSurface(qw_xdg_popup::from(popup));
}

void WXdgShell::create(WServer *server)
{
    W_D(WXdgShell);
    // free follow display
    auto xdg_shell = qw_xdg_shell::create(*server->handle(), d->version);
    QObject::connect(xdg_shell, &qw_xdg_shell::notify_new_toplevel, this, [this] (wlr_xdg_toplevel *toplevel_surface) {
        d_func()->onNewXdgToplevelSurface(qw_xdg_toplevel::from(toplevel_surface));
    });

    // When layer_surface is an xdg_popup's parent, the popup should created via xdg_surface::get_popup with the parent set to NULL,
    // and invoke 'zwlr_layer_surface_v1::get_popup' to set parent before committing the popup's initial state.

    // We should use parent's `notify_new_popup` to avoid get a popup with NULL parent
    // QObject::connect(xdg_shell, &qw_xdg_shell::notify_new_popup, this, [this] (wlr_xdg_popup *xdg_popup) {
    //     d_func()->onNewXdgPopupSurface(qw_xdg_popup::from(xdg_popup));
    // });
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
    auto handle = nativeInterface<qw_xdg_shell>();
    return handle->handle()->global;
}

WAYLIB_SERVER_END_NAMESPACE
