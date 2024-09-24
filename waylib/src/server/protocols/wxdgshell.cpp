// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxdgshell.h"
#include "wxdgsurface.h"
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
    void onNewXdgSurface(qw_xdg_surface *xdgSurface);
    void onSurfaceDestroy(qw_xdg_surface *xdgSurface);
    // end slot function

    W_DECLARE_PUBLIC(WXdgShell)

    QVector<WXdgSurface*> surfaceList;
};

void WXdgShellPrivate::onNewXdgSurface(qw_xdg_surface *xdgSurface)
{
    auto server = q_func()->server();
    // TODO: qw_xdg_surface::from(wlr_surface)
    auto surface = new WXdgSurface(xdgSurface, server);
    surface->setParent(server);
    Q_ASSERT(surface->parent() == server);
    surface->safeConnect(&qw_xdg_surface::before_destroy, q_func(), [this, xdgSurface] {
        onSurfaceDestroy(xdgSurface);
    });
    surfaceList.append(surface);
    q_func()->surfaceAdded(surface);
}

void WXdgShellPrivate::onSurfaceDestroy(qw_xdg_surface *xdgSurface)
{
    auto surface = WXdgSurface::fromHandle(xdgSurface);
    Q_ASSERT(surface);
    bool ok = surfaceList.removeOne(surface);
    Q_ASSERT(ok);
    q_func()->surfaceRemoved(surface);
    surface->safeDeleteLater();
}

WXdgShell::WXdgShell()
    : WObject(*new WXdgShellPrivate(this))
{

}

QVector<WXdgSurface*> WXdgShell::surfaceList() const
{
    W_DC(WXdgShell);
    return d->surfaceList;
}

QByteArrayView WXdgShell::interfaceName() const
{
    return "xdg_wm_base";
}

void WXdgShell::create(WServer *server)
{
    W_D(WXdgShell);
    // free follow display

    auto xdg_shell = qw_xdg_shell::create(*server->handle(), 2);
    QObject::connect(xdg_shell, &qw_xdg_shell::notify_new_surface, this, [this] (wlr_xdg_surface *xdg_surface) {
        d_func()->onNewXdgSurface(qw_xdg_surface::from(xdg_surface));
    });
    m_handle = xdg_shell;
}

void WXdgShell::destroy(WServer *server)
{
    Q_UNUSED(server);
    W_D(WXdgShell);

    auto list = d->surfaceList;
    d->surfaceList.clear();

    for (auto surface : std::as_const(list)) {
        surfaceRemoved(surface);
        surface->safeDeleteLater();
    }
}

wl_global *WXdgShell::global() const
{
    auto handle = nativeInterface<qw_xdg_shell>();
    return handle->handle()->global;
}

WAYLIB_SERVER_END_NAMESPACE
