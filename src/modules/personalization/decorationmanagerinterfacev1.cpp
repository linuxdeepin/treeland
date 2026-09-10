// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "decorationmanagerinterfacev1.h"
#include "surfacewrapper.h"

#include "qwayland-server-treeland-decoration-unstable-v1.h"

#include "common/treelandlogging.h"

#include <wxdgsurface.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <QList>

static QList<DecorationContextV1 *> s_decorationContexts;

// ---------------------------------------------------------------------------
// DecorationContextV1Private
// ---------------------------------------------------------------------------

class DecorationContextV1Private
    : public QtWaylandServer::treeland_decoration_context_v1
{
public:
    DecorationContextV1Private(DecorationContextV1 *_q, wl_resource *resource, wlr_surface *surface);
    ~DecorationContextV1Private() override = default;

    DecorationContextV1 *q = nullptr;
    wlr_surface *surface = nullptr;

    uint32_t cornerRadius = 0;
    Shadow shadow {};
    Border border {};
    bool noTitlebar = false;

protected:
    void destroy_resource(Resource *resource) override;
    void destroy(Resource *resource) override;
    void set_corner_radius(Resource *resource, uint32_t radius) override;
    void set_shadow(Resource *resource, uint32_t radius, int32_t offset_x, int32_t offset_y,
                    uint32_t r, uint32_t g, uint32_t b, uint32_t a) override;
    void set_border(Resource *resource, uint32_t width,
                    uint32_t r, uint32_t g, uint32_t b, uint32_t a) override;
    void set_titlebar_mode(Resource *resource, uint32_t mode) override;
};

DecorationContextV1Private::DecorationContextV1Private(
    DecorationContextV1 *_q, wl_resource *resource, wlr_surface *_surface)
    : QtWaylandServer::treeland_decoration_context_v1(resource)
    , q(_q)
    , surface(_surface)
{
}

void DecorationContextV1Private::destroy_resource([[maybe_unused]] Resource *resource)
{
    delete q;
}

void DecorationContextV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void DecorationContextV1Private::set_corner_radius([[maybe_unused]] Resource *resource, uint32_t radius)
{
    cornerRadius = radius;
    Q_EMIT q->cornerRadiusChanged();
}

void DecorationContextV1Private::set_shadow([[maybe_unused]] Resource *resource,
                                             uint32_t radius, int32_t offset_x, int32_t offset_y,
                                             uint32_t r, uint32_t g, uint32_t b, uint32_t a)
{
    shadow = Shadow{
        static_cast<int32_t>(radius),
        QPoint{offset_x, offset_y},
        QColor{static_cast<int>(r), static_cast<int>(g), static_cast<int>(b), static_cast<int>(a)}
    };
    Q_EMIT q->shadowChanged();
}

void DecorationContextV1Private::set_border([[maybe_unused]] Resource *resource,
                                             uint32_t width,
                                             uint32_t r, uint32_t g, uint32_t b, uint32_t a)
{
    border = Border{
        static_cast<int32_t>(width),
        QColor{static_cast<int>(r), static_cast<int>(g), static_cast<int>(b), static_cast<int>(a)}
    };
    Q_EMIT q->borderChanged();
}

void DecorationContextV1Private::set_titlebar_mode([[maybe_unused]] Resource *resource, uint32_t mode)
{
    noTitlebar = (mode == TREELAND_DECORATION_CONTEXT_V1_TITLEBAR_MODE_HIDE);
    Q_EMIT q->titlebarModeChanged();
}

// ---------------------------------------------------------------------------
// DecorationContextV1
// ---------------------------------------------------------------------------

DecorationContextV1::DecorationContextV1(wl_resource *resource, wlr_surface *surface)
    : QObject(nullptr)
    , d(new DecorationContextV1Private(this, resource, surface))
{
}

DecorationContextV1::~DecorationContextV1() = default;

wl_resource *DecorationContextV1::resource() const
{
    return d->resource()->handle;
}

wlr_surface *DecorationContextV1::surface() const
{
    return d->surface;
}

uint32_t DecorationContextV1::cornerRadius() const
{
    return d->cornerRadius;
}

Shadow DecorationContextV1::shadow() const
{
    return d->shadow;
}

Border DecorationContextV1::border() const
{
    return d->border;
}

bool DecorationContextV1::noTitlebar() const
{
    return d->noTitlebar;
}

DecorationContextV1 *DecorationContextV1::get(wl_resource *resource)
{
    for (auto *context : std::as_const(s_decorationContexts)) {
        if (context->resource() == resource) {
            return context;
        }
    }
    return nullptr;
}

DecorationContextV1 *DecorationContextV1::getContext(WSurface *surface)
{
    for (auto *context : std::as_const(s_decorationContexts)) {
        if (context->surface() == surface->handle()) {
            return context;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// DecorationManagerInterfaceV1
// ---------------------------------------------------------------------------

class DecorationManagerInterfaceV1Private
    : public QtWaylandServer::treeland_decoration_manager_v1
{
public:
    explicit DecorationManagerInterfaceV1Private(DecorationManagerInterfaceV1 *_q);
    ~DecorationManagerInterfaceV1Private() override = default;

    wl_global *global() const { return m_global; }

    DecorationManagerInterfaceV1 *q = nullptr;

protected:
    void destroy(Resource *resource) override;
    void get_decoration_context(Resource *resource, uint32_t id, struct ::wl_resource *surface) override;
};

DecorationManagerInterfaceV1Private::DecorationManagerInterfaceV1Private(DecorationManagerInterfaceV1 *_q)
    : QtWaylandServer::treeland_decoration_manager_v1()
    , q(_q)
{
}

void DecorationManagerInterfaceV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void DecorationManagerInterfaceV1Private::get_decoration_context(Resource *resource, uint32_t id,
                                                                  struct ::wl_resource *surface)
{
    if (!surface) {
        wl_resource_post_error(resource->handle,
                               TREELAND_DECORATION_MANAGER_V1_ERROR_INVALID_SURFACE,
                               "surface resource is NULL");
        return;
    }

    // Verify the surface belongs to the requesting client.
    if (wl_resource_get_client(surface) != resource->client()) {
        wl_resource_post_error(resource->handle,
                               TREELAND_DECORATION_MANAGER_V1_ERROR_INVALID_SURFACE,
                               "surface not owned by requesting client");
        return;
    }

    auto *wlrSurface = wlr_surface_from_resource(surface);

    // Check for duplicate context
    for (auto *ctx : std::as_const(s_decorationContexts)) {
        if (ctx->surface() == wlrSurface) {
            wl_resource_post_error(resource->handle,
                                   TREELAND_DECORATION_MANAGER_V1_ERROR_ALREADY_USED,
                                   "a decoration context already exists for this surface");
            return;
        }
    }

    wl_resource *ctxResource = wl_resource_create(
        resource->client(),
        &treeland_decoration_context_v1_interface,
        resource->version(), id);
    if (!ctxResource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto *context = new DecorationContextV1(ctxResource, wlrSurface);
    s_decorationContexts.append(context);
    QObject::connect(context, &QObject::destroyed, [](QObject *obj) {
        s_decorationContexts.removeOne(static_cast<DecorationContextV1 *>(obj));
    });

    Q_EMIT q->contextCreated(context);
}

struct DecorationManagerInterfaceV1::Private
{
    std::unique_ptr<DecorationManagerInterfaceV1Private> server;
};

DecorationManagerInterfaceV1::DecorationManagerInterfaceV1(QObject *parent)
    : QObject(parent)
    , WServerInterface()
    , d(std::make_unique<Private>())
{
    d->server = std::make_unique<DecorationManagerInterfaceV1Private>(this);
}

DecorationManagerInterfaceV1::~DecorationManagerInterfaceV1() = default;

QByteArrayView DecorationManagerInterfaceV1::interfaceName() const
{
    return d->server->interfaceName();
}

void DecorationManagerInterfaceV1::create(WServer *server)
{
    d->server->init(server->handle(), InterfaceVersion);
}

void DecorationManagerInterfaceV1::destroy([[maybe_unused]] WServer *server)
{
    d->server->globalRemove();
}

wl_global *DecorationManagerInterfaceV1::global() const
{
    return d->server->global();
}
