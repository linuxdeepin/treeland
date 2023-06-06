// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquickxdgshell_p.h"
#include "wseat.h"
#include "woutput.h"
#include "wxdgshell.h"
#include "wxdgsurface.h"
#include "wsurface.h"

#include <QCoreApplication>

WAYLIB_SERVER_BEGIN_NAMESPACE

class XdgShell : public WXdgShell
{
public:
    XdgShell(WQuickXdgShell *qq)
        : qq(qq) {}

    void surfaceAdded(WXdgSurface *surface) override;
    void surfaceRemoved(WXdgSurface *surface) override;

    WQuickXdgShell *qq;
};

class WQuickXdgShellPrivate : public WObjectPrivate, public WXdgShell
{
public:
    WQuickXdgShellPrivate(WQuickXdgShell *qq)
        : WObjectPrivate(qq)
        , WXdgShell()
    {
    }

    W_DECLARE_PUBLIC(WQuickXdgShell)

    XdgShell *xdgShell = nullptr;
};

void XdgShell::surfaceAdded(WXdgSurface *surface)
{
    QObject::connect(surface, &WSurface::requestMap, qq, [this, surface] {
        Q_EMIT qq->surfaceRequestMap(surface);
    });
    QObject::connect(surface, &WSurface::requestUnmap, qq, [this, surface] {
        Q_EMIT qq->surfaceRequestUnmap(surface);
    });

    Q_EMIT qq->surfaceAdded(surface);
}

void XdgShell::surfaceRemoved(WXdgSurface *surface)
{
    surface->disconnect(surface, nullptr, qq, nullptr);

    Q_EMIT qq->surfaceRemoved(surface);
}

WQuickXdgShell::WQuickXdgShell(QObject *parent)
    : WQuickWaylandServerInterface(parent)
    , WObject(*new WQuickXdgShellPrivate(this), nullptr)
{

}

void WQuickXdgShell::create()
{
    W_D(WQuickXdgShell);

    d->xdgShell = server()->attach<XdgShell>(this);
    WQuickWaylandServerInterface::polish();
}

WAYLIB_SERVER_END_NAMESPACE
