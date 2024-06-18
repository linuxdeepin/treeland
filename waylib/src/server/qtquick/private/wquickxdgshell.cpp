// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquickxdgshell_p.h"
#include "wxdgshell.h"
#include "wxdgsurface.h"
#include "private/wglobal_p.h"

#include <qwxdgshell.h>
#include <qwseat.h>

#include <QCoreApplication>

extern "C" {
#define static
#include <wlr/types/wlr_xdg_shell.h>
#undef static
#include <wlr/util/edges.h>
}

QW_USE_NAMESPACE
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
    WXdgShell::surfaceAdded(surface);
    Q_EMIT qq->surfaceAdded(surface);
}

void XdgShell::surfaceRemoved(WXdgSurface *surface)
{
    WXdgShell::surfaceRemoved(surface);
    Q_EMIT qq->surfaceRemoved(surface);
}

WQuickXdgShell::WQuickXdgShell(QObject *parent)
    : WQuickWaylandServerInterface(parent)
    , WObject(*new WQuickXdgShellPrivate(this), nullptr)
{

}

WServerInterface *WQuickXdgShell::create()
{
    W_D(WQuickXdgShell);

    d->xdgShell = server()->attach<XdgShell>(this);

    return d->xdgShell;
}

WAYLIB_SERVER_END_NAMESPACE
