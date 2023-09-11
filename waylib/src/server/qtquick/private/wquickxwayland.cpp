// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquickxwayland_p.h"
#include "wquickwaylandserver.h"
#include "wseat.h"
#include "wxwaylandsurface.h"
#include "wxwayland.h"
#include "wquickobserver.h"

#include <qwxwayland.h>
#include <qwxwaylandsurface.h>
#include <qwxwaylandshellv1.h>

#include <QQmlInfo>
#include <private/qquickitem_p.h>

extern "C" {
#define class className
#include <wlr/xwayland.h>
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

void WXWaylandShellV1::create()
{
    m_shell = QWXWaylandShellV1::create(server()->handle(), 1);
    if (!m_shell)
        Q_EMIT shellChanged();
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

    QObject::connect(surface, &WXWaylandSurface::isToplevelChanged,
                     qq, &WQuickXWayland::onIsToplevelChanged);
    QObject::connect(surface->handle(), &QWXWaylandSurface::associate,
                     qq, [this, surface] {
        qq->addSurface(surface);
    });
    QObject::connect(surface->handle(), &QWXWaylandSurface::dissociate,
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

wl_client *WQuickXWayland::client() const
{
    if (!xwayland || !xwayland->isValid())
        return nullptr;
    return xwayland->handle()->handle()->server->client;
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

void WQuickXWayland::create()
{
    WQuickWaylandServerInterface::create();
    tryCreateXWayland();
}

void WQuickXWayland::tryCreateXWayland()
{
    if (!m_compositor)
        return;

    Q_ASSERT(!xwayland);

    xwayland = server()->attach<XWayland>(this);
    connect(xwayland->handle(), &QWXWayland::ready, this, &WQuickXWayland::ready);

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

WXWaylandSurfaceItem::WXWaylandSurfaceItem(QQuickItem *parent)
    : WSurfaceItem(parent)
{

}

WXWaylandSurfaceItem::~WXWaylandSurfaceItem()
{
    enableObserver(false);
}

WXWaylandSurface *WXWaylandSurfaceItem::surface() const
{
    return m_surface;
}

void WXWaylandSurfaceItem::setSurface(WXWaylandSurface *surface)
{
    if (m_surface == surface)
        return;

    if (m_surface) {
        bool ok = disconnect(m_surface, &WXWaylandSurface::surfaceChanged, this, nullptr);
        Q_ASSERT(ok);
    }

    m_surface = surface;

    if (surface) {
        Q_ASSERT(surface->surface());
        WSurfaceItem::setSurface(surface->surface());
        connect(surface, &WXWaylandSurface::surfaceChanged, this, [this] {
            WSurfaceItem::setSurface(m_surface->surface());
        });
    } else {
        enableObserver(false);
        WSurfaceItem::setSurface(nullptr);
    }

    Q_EMIT surfaceChanged();
}

QSize WXWaylandSurfaceItem::maximumSize() const
{
    return m_maximumSize;
}

QSize WXWaylandSurfaceItem::minimumSize() const
{
    return m_minimumSize;
}

bool WXWaylandSurfaceItem::autoConfigurePosition() const
{
    return m_autoConfigurePosition;
}

void WXWaylandSurfaceItem::setAutoConfigurePosition(bool newAutoConfigurePosition)
{
    if (m_autoConfigurePosition == newAutoConfigurePosition)
        return;
    m_autoConfigurePosition = newAutoConfigurePosition;
    Q_EMIT autoConfigurePositionChanged();

    if (m_surface && m_surface->surface())
        enableObserver(newAutoConfigurePosition);
}

void WXWaylandSurfaceItem::onSurfaceCommit()
{
    WSurfaceItem::onSurfaceCommit();

    QSize minSize = m_surface->minSize();
    if (!minSize.isValid())
        minSize = QSize(0, 0);

    QSize maxSize = m_surface->maxSize();
    if (maxSize.isValid())
        maxSize = QSize(INT_MAX, INT_MAX);

    if (m_minimumSize != minSize) {
        m_minimumSize = minSize;
        Q_EMIT minimumSizeChanged();
    }

    if (m_maximumSize != maxSize) {
        m_maximumSize = maxSize;
        Q_EMIT maximumSizeChanged();
    }
}

void WXWaylandSurfaceItem::initSurface()
{
    WSurfaceItem::initSurface();
    Q_ASSERT(m_surface);
    enableObserver(m_autoConfigurePosition);
}

bool WXWaylandSurfaceItem::resizeSurface(const QSize &newSize)
{
    if (!m_surface->checkNewSize(newSize))
        return false;

    if (observer) {
        m_surface->configure(QRect(observer->globalPosition().toPoint(), newSize));
    } else {
        m_surface->configure(QRect(m_surface->geometry().topLeft(), newSize));
    }
    return true;
}

QRectF WXWaylandSurfaceItem::getContentGeometry() const
{
    return m_surface->getContentGeometry();
}

QSizeF WXWaylandSurfaceItem::getContentSize() const
{
    return size() - QSizeF(leftPadding() + rightPadding(), topPadding() + bottomPadding());
}

void WXWaylandSurfaceItem::updateSurfaceGeometry()
{
    Q_ASSERT(observer);
    Q_ASSERT(m_autoConfigurePosition);
    Q_ASSERT(m_surface);

    const QPoint newPos = observer->globalPosition().toPoint();
    if (m_surface->geometry().topLeft() == newPos)
        return;

    m_surface->configure(QRect(observer->globalPosition().toPoint(),
                               m_surface->geometry().size()));
}

void WXWaylandSurfaceItem::enableObserver(bool on)
{
    if (bool(observer) == on)
        return;

    if (observer) {
        observer->disconnect(this);
        observer->deleteLater();
    } else {
        observer = new WQuickObserver(this);
        observer->setSize(QSizeF(1, 1));
        auto d = QQuickItemPrivate::get(observer);
        auto cd = QQuickItemPrivate::get(contentItem());
        d->anchors()->setTop(cd->anchors()->top());
        d->anchors()->setLeft(cd->anchors()->left());

        connect(m_surface, &WXWaylandSurface::destroyed,
                observer, [this] {
                    // for maybeGlobalPositionChanged signal
                    observer->disconnect(this);
                    observer->deleteLater();
                });
        connect(observer, &WQuickObserver::maybeGlobalPositionChanged,
                this, &WXWaylandSurfaceItem::updateSurfaceGeometry);
    }
}

WAYLIB_SERVER_END_NAMESPACE
