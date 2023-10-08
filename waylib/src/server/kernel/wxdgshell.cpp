// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxdgshell.h"
#include "wxdgsurface.h"

#include <qwxdgshell.h>
#include <qwcompositor.h>

#include <QPointer>

extern "C" {
#define static
#include <wlr/types/wlr_xdg_shell.h>
#undef static
#include <wlr/util/edges.h>
}

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class WXdgShellPrivate : public WObjectPrivate
{
public:
    WXdgShellPrivate(WXdgShell *qq)
        : WObjectPrivate(qq)
    {

    }

    // begin slot function
    void on_new_xdg_surface(wlr_xdg_surface *wlr_surface);
    void on_surface_destroy(QObject *data);
    // end slot function

    W_DECLARE_PUBLIC(WXdgShell)

    QVector<WXdgSurface*> surfaceList;
};

void WXdgShellPrivate::on_new_xdg_surface(wlr_xdg_surface *wlr_surface)
{
    auto server = q_func()->server();
    // TODO: QWXdgSurface::from(wlr_surface)
    QWXdgSurface *xdgSurface = QWXdgSurface::from(QWSurface::from(wlr_surface->surface));
    auto surface = new WXdgSurface(xdgSurface, server);
    surface->setParent(server);
    Q_ASSERT(surface->parent() == server);
    QObject::connect(xdgSurface, &QWXdgSurface::beforeDestroy, q_func(), [this] (QObject *data) {
        on_surface_destroy(static_cast<QWXdgSurface*>(data));
    });

    surfaceList.append(surface);
    q_func()->surfaceAdded(surface);
}

void WXdgShellPrivate::on_surface_destroy(QObject *data)
{
    QWXdgSurface *wlr_surface = qobject_cast<QWXdgSurface*>(data);
    auto surface = WXdgSurface::fromHandle(wlr_surface);
    Q_ASSERT(surface);
    bool ok = surfaceList.removeOne(surface);
    Q_ASSERT(ok);
    q_func()->surfaceRemoved(surface);
    surface->deleteLater();
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

void WXdgShell::surfaceAdded(WXdgSurface *)
{

}

void WXdgShell::surfaceRemoved(WXdgSurface *)
{

}

void WXdgShell::create(WServer *server)
{
    W_D(WXdgShell);
    // free follow display

    auto xdg_shell = QWXdgShell::create(server->handle(), 2);
    QObject::connect(xdg_shell, &QWXdgShell::newSurface, this, [this] (wlr_xdg_surface *surface) {
        d_func()->on_new_xdg_surface(surface);
    });
    m_handle = xdg_shell;
}

void WXdgShell::destroy(WServer *server)
{
    Q_UNUSED(server);
    W_D(WXdgShell);

    auto list = d->surfaceList;
    d->surfaceList.clear();

    for (auto surface : list) {
        surfaceRemoved(surface);
        surface->deleteLater();
    }
}

wl_global *WXdgShell::global() const
{
    auto handle = nativeInterface<QWXdgShell>();
    return handle->handle()->global;
}

WAYLIB_SERVER_END_NAMESPACE
