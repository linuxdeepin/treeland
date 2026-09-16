// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "xwindowcontrolinterfacev1.h"
#include "qwayland-server-treeland-xwindow-control-unstable-v1.h"

#include "seat/helper.h"

#include <wserver.h>
#include <wsurface.h>

#include <wayland-server-core.h>

#include <cstring>

class XWindowControlInterfaceV1Private : public QtWaylandServer::treeland_xwindow_control_v1
{
public:
    XWindowControlInterfaceV1Private(XWindowControlInterfaceV1 *_q);
    wl_global *global() const;

    XWindowControlInterfaceV1 *q;

protected:
    void destroy(Resource *resource) override;
    void set_xwindow_position_relative(Resource *resource, uint32_t callback, uint32_t wid,
                                       struct ::wl_resource *anchor, wl_fixed_t dx, wl_fixed_t dy) override;
};

XWindowControlInterfaceV1Private::XWindowControlInterfaceV1Private(XWindowControlInterfaceV1 *_q)
    : QtWaylandServer::treeland_xwindow_control_v1()
    , q(_q)
{
}

wl_global *XWindowControlInterfaceV1Private::global() const
{
    return m_global;
}

void XWindowControlInterfaceV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XWindowControlInterfaceV1Private::set_xwindow_position_relative(Resource *resource,
                                                                      uint32_t callback,
                                                                      uint32_t wid,
                                                                      struct ::wl_resource *anchor,
                                                                      wl_fixed_t dx,
                                                                      wl_fixed_t dy)
{
    WSurface *wsurface = nullptr;
    if (anchor && strcmp(wl_resource_get_class(anchor), "wl_surface") == 0)
        wsurface = WSurface::fromHandle(wlr_surface_from_resource(anchor));
    uint32_t ok = (wsurface && Helper::instance()->setXWindowPositionRelative(wid, wsurface, dx, dy)) ? 0 : 1;
    wl_resource *cb = wl_resource_create(resource->client(), &wl_callback_interface, 1, callback);
    if (!cb) {
        wl_client_post_no_memory(resource->client());
        return;
    }
    wl_callback_send_done(cb, ok);
    wl_resource_destroy(cb);
}

XWindowControlInterfaceV1::XWindowControlInterfaceV1(QObject *parent)
    : QObject(parent)
    , d(new XWindowControlInterfaceV1Private(this))
{
}

XWindowControlInterfaceV1::~XWindowControlInterfaceV1() = default;

void XWindowControlInterfaceV1::create(WServer *server)
{
    d->init(server->handle(), InterfaceVersion);
}

void XWindowControlInterfaceV1::destroy([[maybe_unused]] WServer *server)
{
    d->globalRemove();
}

wl_global *XWindowControlInterfaceV1::global() const
{
    return d->global();
}

QByteArrayView XWindowControlInterfaceV1::interfaceName() const
{
    return d->interfaceName();
}
