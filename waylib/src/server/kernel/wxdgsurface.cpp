/*
 * Copyright (C) 2021 zkyd
 *
 * Author:     zkyd <zkyd@zjide.org>
 *
 * Maintainer: zkyd <zkyd@zjide.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "wxdgsurface.h"
#include "private/wsurface_p.h"
#include "wsurfacelayout.h"

#include <QDebug>

extern "C" {
#include <wlr/types/wlr_xdg_shell.h>
}

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WXdgSurfacePrivate : public WSurfacePrivate {
public:
    WXdgSurfacePrivate(WXdgSurface *qq, void *handle);

    // begin slot function
    void on_configure(void *data);
    void on_ack_configure(void *data);
    // end slot function

    void init();
    void connect();

#if WLR_VERSION_MINOR > 15
    inline wlr_xdg_toplevel *getHandle() const {
        return handle->toplevel;
    }
#else
    inline wlr_xdg_surface *getHandle() const {
        return handle;
    }
#endif

    W_DECLARE_PUBLIC(WXdgSurface)

    wlr_xdg_surface *handle;
};

WXdgSurfacePrivate::WXdgSurfacePrivate(WXdgSurface *qq, void *hh)
    : WSurfacePrivate(qq)
    , handle(reinterpret_cast<wlr_xdg_surface*>(hh))
{

}

void WXdgSurfacePrivate::on_configure(void *data)
{
//    auto config = reinterpret_cast<wlr_xdg_surface_configure*>(data);
}

void WXdgSurfacePrivate::on_ack_configure(void *data)
{
//    auto config = reinterpret_cast<wlr_xdg_surface_configure*>(data);
}

void WXdgSurfacePrivate::init()
{
    W_Q(WXdgSurface);
    handle->data = q;
    q->setHandle(reinterpret_cast<WSurfaceHandle*>(handle->surface));

    connect();
}

void WXdgSurfacePrivate::connect()
{
    sc.connect(&handle->events.configure,
               this, &WXdgSurfacePrivate::on_configure);
    sc.connect(&handle->events.ack_configure,
               this, &WXdgSurfacePrivate::on_ack_configure);
}

WXdgSurface::WXdgSurface(WXdgSurfaceHandle *handle, QObject *parent)
    : WSurface(*new WXdgSurfacePrivate(this, handle), parent)
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
    if (d->handle->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL)
        return toplevelType();
    else if (d->handle->role == WLR_XDG_SURFACE_ROLE_POPUP)
        return popupType();

    return noneType();
}

bool WXdgSurface::testAttribute(WSurface::Attribute attr) const
{
    W_DC(WXdgSurface);

    if (attr == Attribute::Immovable) {
        return d->handle->role == WLR_XDG_SURFACE_ROLE_POPUP;
    } else if (attr == Attribute::DoesNotAcceptFocus) {
        return d->handle->role == WLR_XDG_SURFACE_ROLE_NONE;
    }

    return WSurface::testAttribute(attr);
}

WXdgSurfaceHandle *WXdgSurface::handle() const
{
    W_DC(WXdgSurface);
    return reinterpret_cast<WXdgSurfaceHandle*>(d->handle);
}

WSurfaceHandle *WXdgSurface::inputTargetAt(qreal scale, QPointF &globalPos) const
{
    W_DC(WXdgSurface);
    // find a wlr_suface object who can receive the events
    const QPointF &pos = positionFromGlobal(fromEffectivePos(scale, globalPos));
    auto sur = wlr_xdg_surface_surface_at(d->handle, pos.x(), pos.y(),
                                          &globalPos.rx(), &globalPos.ry());

    return reinterpret_cast<WSurfaceHandle*>(sur);
}

WXdgSurface *WXdgSurface::fromHandle(WXdgSurfaceHandle *handle)
{
    auto *data = reinterpret_cast<wlr_xdg_surface*>(handle)->data;
    return reinterpret_cast<WXdgSurface*>(data);
}

bool WXdgSurface::inputRegionContains(qreal scale, const QPointF &globalPos) const
{
    W_DC(WXdgSurface);
    const QPointF &pos = positionFromGlobal(fromEffectivePos(scale, globalPos));
    return wlr_xdg_surface_surface_at(d->handle, pos.x(), pos.y(),
                                      nullptr, nullptr);
}

void WXdgSurface::resize(const QSize &size)
{
    W_D(WXdgSurface);

    if (d->handle->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL)
        return;

    wlr_xdg_toplevel_set_size(d->getHandle(), size.width(), size.height());
}

bool WXdgSurface::resizeing() const
{
    W_DC(WXdgSurface);
    return d->handle->toplevel->current.resizing;
}

QPointF WXdgSurface::position() const
{
    W_DC(WXdgSurface);
    if (d->handle->role == WLR_XDG_SURFACE_ROLE_POPUP) {
        double sx = 0, sy = 0;
        wlr_xdg_popup_get_position(d->handle->popup, &sx, &sy);
        return QPointF(sx, sy);
    }

    return WSurface::position();
}

WSurface *WXdgSurface::parentSurface() const
{
    W_DC(WXdgSurface);

    wlr_xdg_surface *parent = nullptr;
    if (d->handle->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
#if WLR_VERSION_MINOR > 15
        parent = d->handle->toplevel->parent ? d->handle->toplevel->parent->base : nullptr;
#else
        parent = d->handle->toplevel->parent;
#endif
    } else if (d->handle->role == WLR_XDG_SURFACE_ROLE_POPUP) {
        parent = wlr_xdg_surface_from_wlr_surface(d->handle->popup->parent);
    }

    Q_ASSERT(!parent || parent->data != d->handle->data);
    return parent ? fromHandle(parent) : nullptr;
}

void WXdgSurface::setResizeing(bool resizeing)
{
    W_D(WXdgSurface);
    if (d->handle->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL)
        return;
    wlr_xdg_toplevel_set_resizing(d->getHandle(), resizeing);
}

void WXdgSurface::setMaximize(bool on)
{
    W_D(WXdgSurface);
    if (d->handle->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL)
        return;
    wlr_xdg_toplevel_set_maximized(d->getHandle(), on);
}

void WXdgSurface::setActivate(bool on)
{
    W_D(WXdgSurface);
    if (d->handle->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL)
        return;
    wlr_xdg_toplevel_set_activated(d->getHandle(), on);
}

void WXdgSurface::notifyChanged(ChangeType type)
{
    WSurface::notifyChanged(type);
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
