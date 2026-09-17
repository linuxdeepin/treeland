// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "decorationmanagerinterfacev1.h"

#include "qwayland-server-treeland-decoration-unstable-v1.h"

#include "common/treelandlogging.h"
#include "surface/surfacewrapper.h"

#include <wayland-server-core.h>
#include <wlr/types/wlr_compositor.h>

#include <wtoplevelsurface.h>

#include <cstring>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

constexpr uint32_t InvalidTitlebarMode = 2; // only 0 (show) and 1 (hide) are valid

// Upper bound for the per-window radius / width values (corner radius,
// shadow blur radius, border width). Kept coarse for now; may be split into
// per-property limits later if the compositor's clamping policy changes.
constexpr uint32_t MaxDecorationSize = 1000;

// Color components are defined as [0, 255] by the protocol.
constexpr uint32_t MaxColorComponent = 255;

} // namespace

// ---------------------------------------------------------------------------
// DecorationContextV1Private
// ---------------------------------------------------------------------------

class DecorationContextV1Private : public QtWaylandServer::treeland_decoration_context_v1
{
public:
    DecorationContextV1Private(DecorationContextV1 *_q, wl_resource *resource, wlr_surface *surface);
    ~DecorationContextV1Private() override;

    static void handleSurfaceDestroyed(struct wl_listener *listener, void *data);

    DecorationContextV1 *q = nullptr;
    /// The decorated wl_surface (wlr_surface), used for identity matching.
    wlr_surface *surface = nullptr;
    /// Set when the underlying wl_surface is destroyed; requests are ignored.
    bool inert = false;
    struct wl_listener surfaceDestroyListener = {};

    // "set or default" semantics (see protocol spec, "each property is
    // opt-in"): std::optional encodes both the value and the "was set" state
    // in one place — nullopt = unset, engaged = client override.
    std::optional<int32_t> cornerRadius;
    std::optional<Shadow> shadow;
    std::optional<Border> border;
    std::optional<uint32_t> titlebarMode;

protected:
    void destroy_resource(Resource *resource) override;
    void destroy(Resource *resource) override;
    void set_corner_radius(Resource *resource, uint32_t radius) override;
    void set_shadow(Resource *resource,
                    uint32_t radius,
                    int32_t offset_x,
                    int32_t offset_y,
                    uint32_t r,
                    uint32_t g,
                    uint32_t b,
                    uint32_t a) override;
    void set_border(Resource *resource,
                    uint32_t width,
                    uint32_t r,
                    uint32_t g,
                    uint32_t b,
                    uint32_t a) override;
    void set_titlebar_mode(Resource *resource, uint32_t mode) override;
};

void DecorationContextV1Private::handleSurfaceDestroyed(struct wl_listener *listener,
                                                       [[maybe_unused]] void *data)
{
    DecorationContextV1Private *d = wl_container_of(listener, d, surfaceDestroyListener);
    // Detach the listener as soon as the surface resource is torn down: the
    // context may outlive that resource. wl_list_remove() on an already
    // disconnected (self-referential) link is a no-op, so this is safe even
    // when libwayland's final_emit has already unlinked us.
    wl_list_remove(&listener->link);
    wl_list_init(&listener->link);
    d->inert = true;
    d->surface = nullptr;
}

DecorationContextV1Private::DecorationContextV1Private(DecorationContextV1 *_q,
                                                       wl_resource *resource,
                                                       wlr_surface *_surface)
    : QtWaylandServer::treeland_decoration_context_v1(resource)
    , q(_q)
    , surface(_surface)
{
    // Match wlroots' wl_resource_add_destroy_listener() convention: init the
    // link before attaching it, so wl_list_remove() in the destructor is always
    // a well-defined no-op even if the listener was never actually linked.
    wl_list_init(&surfaceDestroyListener.link);
    surfaceDestroyListener.notify = &DecorationContextV1Private::handleSurfaceDestroyed;
    wl_resource_add_destroy_listener(_surface->resource, &surfaceDestroyListener);
}

DecorationContextV1Private::~DecorationContextV1Private()
{
    // Tolerate an already-disconnected listener: if the surface was destroyed
    // first, handleSurfaceDestroyed() has already unlinked and re-initialized
    // the link, making this wl_list_remove() a harmless no-op.
    wl_list_remove(&surfaceDestroyListener.link);
    wl_list_init(&surfaceDestroyListener.link);
}

void DecorationContextV1Private::destroy_resource([[maybe_unused]] Resource *resource)
{
    delete q;
}

void DecorationContextV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void DecorationContextV1Private::set_corner_radius([[maybe_unused]] Resource *resource,
                                                    uint32_t radius)
{
    if (inert)
        return;
    if (radius > MaxDecorationSize) {
        qCInfo(lcTlProtocol) << "Decoration set_corner_radius ignored: radius" << radius
                             << "exceeds max" << MaxDecorationSize;
        return;
    }
    cornerRadius = static_cast<int32_t>(radius);
    Q_EMIT q->cornerRadiusChanged();
}

void DecorationContextV1Private::set_shadow([[maybe_unused]] Resource *resource,
                                             uint32_t radius,
                                             int32_t offset_x,
                                             int32_t offset_y,
                                             uint32_t r,
                                             uint32_t g,
                                             uint32_t b,
                                             uint32_t a)
{
    if (inert)
        return;
    if (radius > MaxDecorationSize) {
        qCInfo(lcTlProtocol) << "Decoration set_shadow ignored: radius" << radius
                             << "exceeds max" << MaxDecorationSize;
        return;
    }
    if (r > MaxColorComponent || g > MaxColorComponent || b > MaxColorComponent
        || a > MaxColorComponent) {
        qCInfo(lcTlProtocol) << "Decoration set_shadow ignored: color component out of [0,255]:"
                             << r << g << b << a;
        return;
    }
    shadow = Shadow{ static_cast<int32_t>(radius),
                     QPoint{ offset_x, offset_y },
                     QColor{ static_cast<int>(r), static_cast<int>(g), static_cast<int>(b),
                             static_cast<int>(a) } };
    Q_EMIT q->shadowChanged();
}

void DecorationContextV1Private::set_border([[maybe_unused]] Resource *resource,
                                             uint32_t width,
                                             uint32_t r,
                                             uint32_t g,
                                             uint32_t b,
                                             uint32_t a)
{
    if (inert)
        return;
    if (width > MaxDecorationSize) {
        qCInfo(lcTlProtocol) << "Decoration set_border ignored: width" << width
                             << "exceeds max" << MaxDecorationSize;
        return;
    }
    if (r > MaxColorComponent || g > MaxColorComponent || b > MaxColorComponent
        || a > MaxColorComponent) {
        qCInfo(lcTlProtocol) << "Decoration set_border ignored: color component out of [0,255]:"
                             << r << g << b << a;
        return;
    }
    border = Border{ static_cast<int32_t>(width),
                     QColor{ static_cast<int>(r), static_cast<int>(g), static_cast<int>(b),
                             static_cast<int>(a) } };
    Q_EMIT q->borderChanged();
}

void DecorationContextV1Private::set_titlebar_mode([[maybe_unused]] Resource *resource,
                                                    uint32_t mode)
{
    if (inert)
        return;
    // A mode value outside the titlebar_mode enum range is ignored.
    if (mode >= InvalidTitlebarMode) {
        qCInfo(lcTlProtocol) << "Decoration set_titlebar_mode ignored: invalid mode" << mode;
        return;
    }
    titlebarMode = mode;
    Q_EMIT q->titlebarModeChanged();
}

// ---------------------------------------------------------------------------
// DecorationContextV1
// ---------------------------------------------------------------------------

static QList<DecorationContextV1 *> s_contexts;

DecorationContextV1::DecorationContextV1(wl_resource *resource,
                                         wlr_surface *surface,
                                         QObject *parent)
    : QObject(parent)
    , d(std::make_unique<DecorationContextV1Private>(this, resource, surface))
{
    s_contexts.append(this);
}

DecorationContextV1::~DecorationContextV1()
{
    s_contexts.removeOne(this);
}

wl_resource *DecorationContextV1::resource() const
{
    return d->resource()->handle;
}

wlr_surface *DecorationContextV1::surface() const
{
    return d->surface;
}

std::optional<int32_t> DecorationContextV1::cornerRadius() const
{
    return d->cornerRadius;
}

std::optional<Shadow> DecorationContextV1::shadow() const
{
    return d->shadow;
}

std::optional<Border> DecorationContextV1::border() const
{
    return d->border;
}

std::optional<uint32_t> DecorationContextV1::titlebarMode() const
{
    return d->titlebarMode;
}

DecorationContextV1 *DecorationContextV1::forSurface(wlr_surface *surface)
{
    for (auto *context : std::as_const(s_contexts)) {
        if (context->surface() == surface)
            return context;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// DecorationManagerInterfaceV1Private
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
    void get_decoration_context(Resource *resource,
                                uint32_t id,
                                struct ::wl_resource *surface) override;
};

DecorationManagerInterfaceV1Private::DecorationManagerInterfaceV1Private(
    DecorationManagerInterfaceV1 *_q)
    : QtWaylandServer::treeland_decoration_manager_v1()
    , q(_q)
{
}

void DecorationManagerInterfaceV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void DecorationManagerInterfaceV1Private::get_decoration_context(Resource *resource,
                                                                 uint32_t id,
                                                                 struct ::wl_resource *surface)
{
    if (!surface) {
        wl_resource_post_error(resource->handle,
                               TREELAND_DECORATION_MANAGER_V1_ERROR_INVALID_SURFACE,
                               "surface resource is NULL!");
        return;
    }

    // The surface must be owned by the requesting client.
    if (wl_resource_get_client(surface) != resource->client()) {
        wl_resource_post_error(resource->handle,
                               TREELAND_DECORATION_MANAGER_V1_ERROR_INVALID_SURFACE,
                               "surface is not owned by the requesting client!");
        return;
    }

    if (strcmp(wl_resource_get_class(surface), "wl_surface") != 0) {
        wl_resource_post_error(resource->handle,
                               TREELAND_DECORATION_MANAGER_V1_ERROR_INVALID_SURFACE,
                               "invalid surface!");
        return;
    }

    auto *wlrSurface = wlr_surface_from_resource(surface);

    // At most one active decoration context per surface.
    if (DecorationContextV1::forSurface(wlrSurface)) {
        wl_resource_post_error(resource->handle,
                               TREELAND_DECORATION_MANAGER_V1_ERROR_ALREADY_USED,
                               "a decoration context already exists for this surface!");
        return;
    }

    wl_resource *contextResource = wl_resource_create(resource->client(),
                                                      &treeland_decoration_context_v1_interface,
                                                      resource->version(),
                                                      id);
    if (!contextResource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto *context = new DecorationContextV1(contextResource, wlrSurface);
    Q_EMIT q->contextCreated(context);
}

// ---------------------------------------------------------------------------
// DecorationManagerInterfaceV1
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Decoration (per-window adapter)
// ---------------------------------------------------------------------------

Decoration::Decoration(WToplevelSurface *target,
                       DecorationManagerInterfaceV1 *manager,
                       SurfaceWrapper *parent)
    : QObject(parent)
    , m_target(target)
    , m_manager(manager)
{
    connect(target, &WToplevelSurface::beforeDestroy, this, [this] {
        disconnect(m_manager, &DecorationManagerInterfaceV1::contextCreated, this, nullptr);
    });

    auto update = [this](DecorationContextV1 *context) {
        applyContext(context);
    };

    connect(m_manager, &DecorationManagerInterfaceV1::contextCreated, this, update);

    if (auto *context = DecorationContextV1::forSurface(target->surface()->handle())) {
        applyContext(context);
    }
}

SurfaceWrapper *Decoration::surfaceWrapper() const
{
    return qobject_cast<SurfaceWrapper *>(parent());
}

void Decoration::applyContext(DecorationContextV1 *context)
{
    if (!m_target)
        return;
    if (context->surface() != m_target->surface()->handle())
        return;
    auto *wrapper = surfaceWrapper();
    if (!wrapper)
        return;

    // Corner radius: a set value (including 0 = square corners) overrides the
    // compositor default; an unset value keeps the wrapper default.
    if (auto radius = context->cornerRadius()) {
        wrapper->setRadius(*radius);
    }
    connect(context, &DecorationContextV1::cornerRadiusChanged, this, [wrapper, context] {
        if (auto radius = context->cornerRadius())
            wrapper->setRadius(*radius);
    });

    // Shadow: radius 0 disables the shadow; the color components are [0,255].
    if (auto shadow = context->shadow()) {
        wrapper->setShadowValues(shadow->radius, shadow->offset.x(), shadow->offset.y(),
                                 shadow->color);
        wrapper->setShadowVisible(shadow->radius > 0);
    }
    connect(context, &DecorationContextV1::shadowChanged, this, [wrapper, context] {
        const auto shadow = context->shadow();
        if (!shadow)
            return;
        wrapper->setShadowValues(shadow->radius, shadow->offset.x(), shadow->offset.y(),
                                 shadow->color);
        wrapper->setShadowVisible(shadow->radius > 0);
    });

    // Border: width 0 disables the border.
    if (auto border = context->border()) {
        wrapper->setBorderValues(border->width, border->color);
        wrapper->setBorderVisible(border->width > 0);
    }
    connect(context, &DecorationContextV1::borderChanged, this, [wrapper, context] {
        const auto border = context->border();
        if (!border)
            return;
        wrapper->setBorderValues(border->width, border->color);
        wrapper->setBorderVisible(border->width > 0);
    });

    // Titlebar: hide keeps the rest of the SSD (half-CSD half-SSD).
    //
    // This adapter only records the override and notifies Helper; the actual
    // SurfaceWrapper::noTitleBar write is arbitrated in Helper so the frozen
    // personalization path and the new protocol can't clobber each other.
    if (auto mode = context->titlebarMode()) {
        m_titlebarOverridden = true;
        m_noTitleBar = *mode == TREELAND_DECORATION_CONTEXT_V1_TITLEBAR_MODE_HIDE;
        Q_EMIT titlebarOverrideChanged();
    }
    connect(context, &DecorationContextV1::titlebarModeChanged, this, [this, context] {
        const auto mode = context->titlebarMode();
        if (!mode)
            return;
        m_titlebarOverridden = true;
        m_noTitleBar = *mode == TREELAND_DECORATION_CONTEXT_V1_TITLEBAR_MODE_HIDE;
        Q_EMIT titlebarOverrideChanged();
    });

    // When the context is destroyed the compositor may revert customized
    // properties to their defaults (per protocol spec).
    connect(context, &QObject::destroyed, this, &Decoration::resetProperties);
}

void Decoration::resetProperties()
{
    auto *wrapper = surfaceWrapper();
    if (!wrapper)
        return;

    // Revert to the compositor defaults (matching SurfaceWrapper's initial
    // state and the standard SSD look).
    //
    // The titlebar mode is intentionally NOT reverted here: the protocol spec
    // only says the compositor "may" revert properties on context destroy,
    // and blindly restoring the titlebar would clobber the xdg-decoration
    // CSD/SSD negotiation state managed by Helper/Personalization. A client
    // that wants the default titlebar back sends set_titlebar_mode(show).
    wrapper->setRadius(0);
    wrapper->setShadowValues(40, 0, 10, QColor(0, 0, 0, 102)); // blur 40, offset (0,10), rgba(0,0,0,0.4)
    wrapper->setShadowVisible(true);
    wrapper->setBorderValues(1, QColor(0, 0, 0, 26)); // width 1, rgba(0,0,0,0.1)
    wrapper->setBorderVisible(true);
}
