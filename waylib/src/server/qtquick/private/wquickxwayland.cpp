// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquickxwayland_p.h"
#include "wquickwaylandserver.h"
#include "wseat.h"
#include "wxwaylandsurface.h"
#include "wxwayland.h"

#include <qwxwayland.h>
#include <qwxwaylandsurface.h>
#include <qwxwaylandshellv1.h>

#include <QQmlInfo>

extern "C" {
#define class className
#include <wlr/xwayland.h>
#include <wlr/xwayland/shell.h>
#undef class
}

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

WXWaylandShellV1::WXWaylandShellV1(QObject *parent)
    : WQuickWaylandServerInterface(parent)
{

}

QWXWaylandShellV1 *WXWaylandShellV1::shell() const
{
    return m_shell;
}

WServerInterface *WXWaylandShellV1::create()
{
    m_shell = QWXWaylandShellV1::create(server()->handle(), 1);
    if (!m_shell)
        return nullptr;

    Q_EMIT shellChanged();
    return new WServerInterface(m_shell, m_shell->handle()->global);
}

class XWayland : public WXWayland
{
public:
    XWayland(WQuickXWayland *qq)
        : WXWayland(qq->compositor(), qq->lazy())
        , qq(qq)
    {

    }

    void surfaceAdded(WXWaylandSurface *surface) override;
    void surfaceRemoved(WXWaylandSurface *surface) override;
    void create(WServer *server) override;

    WQuickXWayland *qq;
};

void XWayland::surfaceAdded(WXWaylandSurface *surface)
{
    WXWayland::surfaceAdded(surface);

    surface->safeConnect(&WXWaylandSurface::isToplevelChanged,
                     qq, &WQuickXWayland::onIsToplevelChanged);
    surface->safeConnect(&QWXWaylandSurface::associate,
                     qq, [this, surface] {
        qq->addSurface(surface);
    });
    surface->safeConnect(&QWXWaylandSurface::dissociate,
                     qq, [this, surface] {
        qq->removeSurface(surface);
    });

    if (surface->surface())
        qq->addSurface(surface);
}

void XWayland::surfaceRemoved(WXWaylandSurface *surface)
{
    WXWayland::surfaceRemoved(surface);
    Q_EMIT qq->surfaceRemoved(surface);

    qq->removeToplevel(surface);
}

void XWayland::create(WServer *server)
{
    WXWayland::create(server);
    setSeat(qq->seat());
}

WQuickXWayland::WQuickXWayland(QObject *parent)
    : WQuickWaylandServerInterface(parent)
{

}

bool WQuickXWayland::lazy() const
{
    return m_lazy;
}

void WQuickXWayland::setLazy(bool newLazy)
{
    if (m_lazy == newLazy)
        return;

    if (xwayland && xwayland->isValid()) {
        qmlWarning(this) << "Can't change \"lazy\" after xwayland created";
        return;
    }

    m_lazy = newLazy;
    Q_EMIT lazyChanged();
}

QWCompositor *WQuickXWayland::compositor() const
{
    return m_compositor;
}

void WQuickXWayland::setCompositor(QWLRoots::QWCompositor *compositor)
{
    if (m_compositor == compositor)
        return;

    if (xwayland && xwayland->isValid()) {
        qmlWarning(this) << "Can't change \"compositor\" after xwayland created";
        return;
    }

    m_compositor = compositor;
    Q_EMIT compositorChanged();

    if (isPolished())
        tryCreateXWayland();
}

WClient *WQuickXWayland::client() const
{
    if (!xwayland || !xwayland->isValid())
        return nullptr;
    return xwayland->waylandClient();
}

pid_t WQuickXWayland::pid() const
{
    if (!xwayland || !xwayland->isValid())
        return 0;
    return xwayland->handle()->handle()->server->pid;
}

QByteArray WQuickXWayland::displayName() const
{
    if (!xwayland)
        return {};
    return xwayland->displayName();
}

WSeat *WQuickXWayland::seat() const
{
    return m_seat;
}

void WQuickXWayland::setSeat(WSeat *newSeat)
{
    if (m_seat == newSeat)
        return;
    m_seat = newSeat;

    if (xwayland && xwayland->isValid())
        xwayland->setSeat(m_seat);

    Q_EMIT seatChanged();
}

WServerInterface *WQuickXWayland::create()
{
    tryCreateXWayland();
    return xwayland;
}

void WQuickXWayland::tryCreateXWayland()
{
    if (!m_compositor)
        return;

    Q_ASSERT(!xwayland);

    xwayland = server()->attach<XWayland>(this);
    xwayland->safeConnect(&QWXWayland::ready, this, &WQuickXWayland::ready);

    Q_EMIT displayNameChanged();
}

void WQuickXWayland::onIsToplevelChanged()
{
    auto surface = qobject_cast<WXWaylandSurface*>(sender());
    Q_ASSERT(surface);

    if (!surface->surface())
        return;

    if (surface->isToplevel()) {
        addToplevel(surface);
    } else {
        removeToplevel(surface);
    }
}

void WQuickXWayland::addSurface(WXWaylandSurface *surface)
{
    Q_EMIT surfaceAdded(surface);

    if (surface->isToplevel())
        addToplevel(surface);
}

void WQuickXWayland::removeSurface(WXWaylandSurface *surface)
{
    Q_EMIT surfaceRemoved(surface);

    if (surface->isToplevel())
        removeToplevel(surface);
}

void WQuickXWayland::addToplevel(WXWaylandSurface *surface)
{
    if (toplevelSurfaces.contains(surface))
        return;
    toplevelSurfaces.append(surface);
    Q_EMIT toplevelAdded(surface);
}

void WQuickXWayland::removeToplevel(WXWaylandSurface *surface)
{
    if (toplevelSurfaces.removeOne(surface))
        Q_EMIT toplevelRemoved(surface);
}

WAYLIB_SERVER_END_NAMESPACE
