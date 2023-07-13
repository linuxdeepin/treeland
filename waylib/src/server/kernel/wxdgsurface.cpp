// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxdgsurface.h"
#include "private/wsurface_p.h"
#include "wseat.h"

#include <qwxdgshell.h>
#include <qwseat.h>
#include <qwcompositor.h>

#include <QDebug>

extern "C" {
#define static
#include <wlr/types/wlr_xdg_shell.h>
#undef static
#include <wlr/util/edges.h>
}

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WXdgSurfacePrivate : public WSurfacePrivate {
public:
    WXdgSurfacePrivate(WXdgSurface *qq, QWXdgSurface *handle, WServer *server);

    inline wlr_xdg_surface *nativeHandle() const {
        Q_ASSERT(handle);
        return handle->handle();
    };

    // begin slot function
    void on_configure(wlr_xdg_surface_configure *event);
    void on_ack_configure(wlr_xdg_surface_configure *event);
    void on_map();
    void on_unmap();
    // end slot function

    void init();
    void connect();

    W_DECLARE_PUBLIC(WXdgSurface)

    QWXdgSurface *handle;
};

WXdgSurfacePrivate::WXdgSurfacePrivate(WXdgSurface *qq, QWXdgSurface *hh, WServer *server)
    : WSurfacePrivate(qq, server)
    , handle(hh)
{
}

void WXdgSurfacePrivate::on_configure(wlr_xdg_surface_configure *event)
{
    Q_UNUSED(event)
//    auto config = reinterpret_cast<wlr_xdg_surface_configure*>(data);
}

void WXdgSurfacePrivate::on_ack_configure(wlr_xdg_surface_configure *event)
{
    Q_UNUSED(event)
//    auto config = reinterpret_cast<wlr_xdg_surface_configure*>(data);
}

void WXdgSurfacePrivate::on_map()
{
    Q_EMIT q_func()->requestMap();
}

void WXdgSurfacePrivate::on_unmap()
{
    Q_EMIT q_func()->requestUnmap();
}

void WXdgSurfacePrivate::init()
{
    W_Q(WXdgSurface);
    handle->setData(this, q);
    q->setHandle(handle->surface());

    connect();
}

void WXdgSurfacePrivate::connect()
{
    QObject::connect(handle, &QWXdgSurface::configure, q_func(), [this] (wlr_xdg_surface_configure *event) {
        on_configure(event);
    });
    QObject::connect(handle, &QWXdgSurface::ackConfigure, q_func(), [this] (wlr_xdg_surface_configure *event) {
        on_ack_configure(event);
    });

    QObject::connect(handle->surface(), &QWSurface::map, q_func(), [this] {
        on_map();
    });
    QObject::connect(handle->surface(), &QWSurface::unmap, q_func(), [this] {
        on_unmap();
    });
}

WXdgSurface::WXdgSurface(QWXdgSurface *handle, WServer *server, QObject *parent)
    : WSurface(*new WXdgSurfacePrivate(this, handle, server), parent)
{
    d_func()->init();
}

WXdgSurface::~WXdgSurface()
{

}

WSurface::Type *WXdgSurface::toplevelType()
{
    static Type type;
    return &type;
}

WSurface::Type *WXdgSurface::popupType()
{
    static Type type;
    return &type;
}

WSurface::Type *WXdgSurface::noneType()
{
    return nullptr;
}

WSurface::Type *WXdgSurface::type() const
{
    W_DC(WXdgSurface);
    if (d->nativeHandle()->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL)
        return toplevelType();
    else if (d->nativeHandle()->role == WLR_XDG_SURFACE_ROLE_POPUP)
        return popupType();

    return noneType();
}

bool WXdgSurface::testAttribute(WSurface::Attribute attr) const
{
    W_DC(WXdgSurface);

    if (attr == Attribute::Immovable) {
        return d->nativeHandle()->role == WLR_XDG_SURFACE_ROLE_POPUP;
    } else if (attr == Attribute::DoesNotAcceptFocus) {
        return d->nativeHandle()->role == WLR_XDG_SURFACE_ROLE_NONE;
    }

    return WSurface::testAttribute(attr);
}

QWXdgSurface *WXdgSurface::handle() const
{
    W_DC(WXdgSurface);
    return d->handle;
}

QWSurface *WXdgSurface::inputTargetAt(QPointF &localPos) const
{
    W_DC(WXdgSurface);
    // find a wlr_suface object who can receive the events
    const QPointF pos = localPos;
    auto sur = d->handle->surfaceAt(pos, &localPos);

    return sur;
}

WXdgSurface *WXdgSurface::fromHandle(QWXdgSurface *handle)
{
    return handle->getData<WXdgSurface>();
}

bool WXdgSurface::inputRegionContains(const QPointF &localPos) const
{
    W_DC(WXdgSurface);
    return d->handle->surfaceAt(localPos, nullptr);
}

void WXdgSurface::resize(const QSize &size)
{
    W_D(WXdgSurface);

    if (auto toplevel = d->handle->topToplevel()) {
        toplevel->setSize(size);
    }
}

bool WXdgSurface::resizeing() const
{
    W_DC(WXdgSurface);
    return d->nativeHandle()->toplevel->current.resizing;
}

QPointF WXdgSurface::position() const
{
    W_DC(WXdgSurface);
    if (auto popup = d->handle->toPopup()) {
        return popup->getPosition();
    }

    return WSurface::position();
}

WSurface *WXdgSurface::parentSurface() const
{
    W_DC(WXdgSurface);

    if (auto toplevel = d->handle->topToplevel()) {
        auto parent = toplevel->handle()->parent;
        if (!parent)
            return nullptr;
        return fromHandle(QWXdgToplevel::from(parent));
    } else if (auto popup = d->handle->toPopup()) {
        auto parent = popup->handle()->parent;
        if (!parent)
            return nullptr;
        return fromHandle(QWXdgSurface::from(QWSurface::from(parent)));
    }

    return nullptr;
}

void WXdgSurface::setResizeing(bool resizeing)
{
    W_D(WXdgSurface);
    if (auto toplevel = d->handle->topToplevel()) {
        toplevel->setResizing(resizeing);
    }
}

void WXdgSurface::setMaximize(bool on)
{
    W_D(WXdgSurface);
    if (auto toplevel = d->handle->topToplevel()) {
        toplevel->setMaximized(on);
    }
}

void WXdgSurface::setActivate(bool on)
{
    W_D(WXdgSurface);
    if (auto toplevel = d->handle->topToplevel()) {
        toplevel->setActivated(on);
    }
}

void WXdgSurface::notifyChanged(ChangeType type, std::any oldValue, std::any newValue)
{
    WSurface::notifyChanged(type, oldValue, newValue);
}

void WXdgSurface::notifyBeginState(State state)
{
    if (state == State::Resize) {
        setResizeing(true);
    } else if (state == State::Maximize) {
        setMaximize(true);
    } else if (state == State::Activate) {
        setActivate(true);
    }

    WSurface::notifyBeginState(state);
}

void WXdgSurface::notifyEndState(State state)
{
    if (state == State::Resize) {
        setResizeing(false);
    } else if (state == State::Maximize) {
        setMaximize(false);
    } else if (state == State::Activate) {
        setActivate(false);
    }

    WSurface::notifyEndState(state);
}

WAYLIB_SERVER_END_NAMESPACE
