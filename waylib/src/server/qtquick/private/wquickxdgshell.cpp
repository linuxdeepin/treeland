// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquickxdgshell_p.h"
#include "wseat.h"
#include "woutput.h"
#include "wxdgshell.h"
#include "wxdgsurface.h"
#include "wsurface.h"

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

void WQuickXdgShell::create()
{
    W_D(WQuickXdgShell);

    d->xdgShell = server()->attach<XdgShell>(this);
    WQuickWaylandServerInterface::polish();
}

WXdgSurfaceItem::WXdgSurfaceItem(QQuickItem *parent)
    : WSurfaceItem(parent)
{

}

WXdgSurfaceItem::~WXdgSurfaceItem()
{

}

WXdgSurface *WXdgSurfaceItem::surface() const
{
    return m_surface;
}

void WXdgSurfaceItem::setSurface(WXdgSurface *surface)
{
    if (m_surface == surface)
        return;

    m_surface = surface;
    WSurfaceItem::setSurface(surface->surface());

    Q_EMIT surfaceChanged();
}

QPointF WXdgSurfaceItem::implicitPosition() const
{
    return m_implicitPosition;
}

void WXdgSurfaceItem::onSurfaceCommit()
{
    WSurfaceItem::onSurfaceCommit();
    if (auto popup = m_surface->handle()->toPopup())
        setImplicitPosition(popup->getPosition() - contentItem()->position());
}

void WXdgSurfaceItem::initSurface()
{
    WSurfaceItem::initSurface();
    Q_ASSERT(m_surface);
}

bool WXdgSurfaceItem::resizeSurface(const QSize &newSize)
{
    if (!m_surface->checkNewSize(newSize))
        return false;
    m_surface->resize(newSize);
    return true;
}

QRectF WXdgSurfaceItem::getContentGeometry() const
{
    return m_surface->getContentGeometry();
}

void WXdgSurfaceItem::setImplicitPosition(const QPointF &newImplicitPosition)
{
    if (m_implicitPosition == newImplicitPosition)
        return;
    m_implicitPosition = newImplicitPosition;
    Q_EMIT implicitPositionChanged();
}

WAYLIB_SERVER_END_NAMESPACE
