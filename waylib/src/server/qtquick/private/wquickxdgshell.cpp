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
    d->xdgShell->setOwnsSocket(ownsSocket());
    WQuickWaylandServerInterface::polish();
}

void WQuickXdgShell::ownsSocketChange()
{
    W_D(WQuickXdgShell);
    if (d->xdgShell)
        d->xdgShell->setOwnsSocket(ownsSocket());
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
    WSurfaceItem::setSurface(surface ? surface->surface() : nullptr);

    Q_EMIT surfaceChanged();
}

QPointF WXdgSurfaceItem::implicitPosition() const
{
    return m_implicitPosition;
}

QSize WXdgSurfaceItem::maximumSize() const
{
    return m_maximumSize;
}

QSize WXdgSurfaceItem::minimumSize() const
{
    return m_minimumSize;
}

inline static int32_t getValidSize(int32_t size, int32_t fallback) {
    return size > 0 ? size : fallback;
}

void WXdgSurfaceItem::onSurfaceCommit()
{
    WSurfaceItem::onSurfaceCommit();
    if (auto popup = m_surface->handle()->toPopup()) {
        setImplicitPosition(popup->getPosition() - contentItem()->position());
    } else if (auto toplevel = m_surface->handle()->topToplevel()) {
        const QSize minSize(getValidSize(toplevel->handle()->current.min_width, 0),
                            getValidSize(toplevel->handle()->current.min_height, 0));
        const QSize maxSize(getValidSize(toplevel->handle()->current.max_width, INT_MAX),
                            getValidSize(toplevel->handle()->current.max_height, INT_MAX));

        if (m_minimumSize != minSize) {
            m_minimumSize = minSize;
            Q_EMIT minimumSizeChanged();
        }

        if (m_maximumSize != maxSize) {
            m_maximumSize = maxSize;
            Q_EMIT maximumSizeChanged();
        }
    }
}

void WXdgSurfaceItem::initSurface()
{
    WSurfaceItem::initSurface();
    Q_ASSERT(m_surface);
    connect(m_surface->handle(), &QWXdgSurface::beforeDestroy,
            this, &WXdgSurfaceItem::releaseResources);
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
