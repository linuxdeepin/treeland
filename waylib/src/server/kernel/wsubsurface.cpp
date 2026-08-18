// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsubsurface.h"

#include "private/wsubsurface_p.h"
#include "wsurface.h"

WAYLIB_SERVER_BEGIN_NAMESPACE

WSubsurfacePrivate::WSubsurfacePrivate(WSubsurface *q,
                                       WSubsurface::Type type,
                                       WSurface *parentSurface,
                                       WSurface *surface)
    : WObjectPrivate(q)
    , type(type)
    , mapped(surface && surface->mapped())
    , parentSurface(parentSurface)
    , surface(surface)
{
}

WSubsurface::WSubsurface(Type type,
                         WSurface *parentSurface,
                         WSurface *surface)
    : QObject(nullptr)
    , WObject(*new WSubsurfacePrivate(this, type, parentSurface, surface))
{
}

WSubsurface::~WSubsurface() = default;

WSubsurface::Type WSubsurface::type() const
{
    W_DC(WSubsurface);
    return d->type;
}

WSubsurface::Place WSubsurface::place() const
{
    W_DC(WSubsurface);
    return d->place;
}

QPointF WSubsurface::position() const
{
    W_DC(WSubsurface);
    return d->position;
}

bool WSubsurface::isMapped() const
{
    W_DC(WSubsurface);
    return d->mapped;
}

WSurface *WSubsurface::parentSurface() const
{
    W_DC(WSubsurface);
    return d->parentSurface;
}

WSurface *WSubsurface::surface() const
{
    W_DC(WSubsurface);
    return d->surface;
}

void WSubsurface::setPlace(Place place)
{
    W_D(WSubsurface);
    if (d->place == place)
        return;
    d->place = place;
    Q_EMIT placeChanged(place);
}

void WSubsurface::setPosition(const QPointF &position)
{
    W_D(WSubsurface);
    if (d->position == position)
        return;
    d->position = position;
    Q_EMIT positionChanged(position);
}

void WSubsurface::setMapped(bool mapped)
{
    W_D(WSubsurface);
    if (d->mapped == mapped)
        return;
    d->mapped = mapped;
    Q_EMIT mappedChanged(mapped);
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_wsubsurface.cpp"
