// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "compositoractioninterfacev1.h"

#include "qwayland-server-treeland-compositor-action-unstable-v1.h"

#include <wayland-server-core.h>

// ---------------------------------------------------------------------------
// CompositorActionInterfaceV1Private
// ---------------------------------------------------------------------------

class CompositorActionInterfaceV1Private
    : public QtWaylandServer::treeland_compositor_action_v1
{
public:
    explicit CompositorActionInterfaceV1Private(CompositorActionInterfaceV1 *_q);
    ~CompositorActionInterfaceV1Private() override = default;

    wl_global *global() const { return m_global; }

    CompositorActionInterfaceV1 *q = nullptr;

protected:
    void destroy(Resource *resource) override;
    void trigger(Resource *resource, uint32_t action) override;
};

CompositorActionInterfaceV1Private::CompositorActionInterfaceV1Private(CompositorActionInterfaceV1 *_q)
    : QtWaylandServer::treeland_compositor_action_v1()
    , q(_q)
{
}

void CompositorActionInterfaceV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void CompositorActionInterfaceV1Private::trigger([[maybe_unused]] Resource *resource,
                                                 uint32_t action)
{
    // One-shot fire-and-forget: forward the raw action value; no reply is
    // ever sent. The compositor routes it to the matching implementation and
    // ignores actions it cannot perform.
    Q_EMIT q->triggered(action);
}

// ---------------------------------------------------------------------------
// CompositorActionInterfaceV1
// ---------------------------------------------------------------------------

struct CompositorActionInterfaceV1::Private
{
    std::unique_ptr<CompositorActionInterfaceV1Private> server;
};

CompositorActionInterfaceV1::CompositorActionInterfaceV1(QObject *parent)
    : QObject(parent)
    , WServerInterface()
    , d(std::make_unique<Private>())
{
    d->server = std::make_unique<CompositorActionInterfaceV1Private>(this);
}

CompositorActionInterfaceV1::~CompositorActionInterfaceV1() = default;

QByteArrayView CompositorActionInterfaceV1::interfaceName() const
{
    return d->server->interfaceName();
}

void CompositorActionInterfaceV1::create(WServer *server)
{
    d->server->init(server->handle(), InterfaceVersion);
}

void CompositorActionInterfaceV1::destroy([[maybe_unused]] WServer *server)
{
    d->server->globalRemove();
}

wl_global *CompositorActionInterfaceV1::global() const
{
    return d->server->global();
}
