// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "layershellextensionmanagerinterfacev1.h"

#include "common/treelandlogging.h"
#include "core/rootsurfacecontainer.h"
#include "qwayland-server-treeland-layer-shell-extension-unstable-v1.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"

#include <wayland-server.h>
#include <wlayersurface.h>
#include <wlr_all.h>
#include <wscoplistener.h>
#include <wseat.h>

static constexpr struct EdgeMapEntry
{
    uint32_t protocolBit;
    Qt::Edge qtEdge;
} kEdgeMap[] = {
    { 1, Qt::TopEdge },    // protocol top    -> Qt::TopEdge
    { 2, Qt::BottomEdge }, // protocol bottom -> Qt::BottomEdge
    { 4, Qt::LeftEdge },   // protocol left   -> Qt::LeftEdge
    { 8, Qt::RightEdge },  // protocol right  -> Qt::RightEdge
};

// All valid edge bits: top|bottom|left|right.
constexpr uint32_t kValidEdgeBits = 1 | 2 | 4 | 8; // = 15

class LayerShellExtensionManagerInterfaceV1Private
    : public QtWaylandServer::treeland_layer_shell_extension_manager_v1
{
public:
    explicit LayerShellExtensionManagerInterfaceV1Private(
        LayerShellExtensionManagerInterfaceV1 *_q);
    wl_global *global() const;

    LayerShellExtensionManagerInterfaceV1 *q;

    QList<LayerShellExtensionObjectV1 *> m_objects;
    void get_layer_shell_extension_object(Resource *resource,
                                          uint32_t id,
                                          struct ::wl_resource *surface) override;
};

class LayerShellExtensionObjectV1Private
    : public QtWaylandServer::treeland_layer_shell_extension_object_v1
{
public:
    LayerShellExtensionObjectV1Private(LayerShellExtensionObjectV1 *_q,
                                       wl_resource *surface,
                                       wl_resource *resource);
    ~LayerShellExtensionObjectV1Private();

    LayerShellExtensionObjectV1 *q;
    wl_resource *nativeSurface = nullptr;
    WSeat *resizingSeat = nullptr;

    bool clampRegistered = false;
    void registerClamp(WSeat *seat, qreal minW, qreal maxW, qreal minH, qreal maxH);
    void clearClamp();

    void onSurfaceDestroyed(struct wlr_surface *surface);
    WScopedListener m_surfaceDestroy;

    void destroy_resource([[maybe_unused]] Resource *resource) override;
    void destroy([[maybe_unused]] Resource *resource) override;
    void begin_resize(Resource *resource,
                      struct ::wl_resource *seat,
                      uint32_t serial,
                      uint32_t edges,
                      int32_t min_width,
                      int32_t min_height,
                      int32_t max_width,
                      int32_t max_height) override;
};

LayerShellExtensionManagerInterfaceV1Private::LayerShellExtensionManagerInterfaceV1Private(
    LayerShellExtensionManagerInterfaceV1 *_q)
    : QtWaylandServer::treeland_layer_shell_extension_manager_v1()
    , q(_q)
{
}

wl_global *LayerShellExtensionManagerInterfaceV1Private::global() const
{
    return m_global;
}

LayerShellExtensionManagerInterfaceV1::LayerShellExtensionManagerInterfaceV1(QObject *parent)
    : QObject(parent)
    , d(new LayerShellExtensionManagerInterfaceV1Private(this))
{
}

LayerShellExtensionManagerInterfaceV1::~LayerShellExtensionManagerInterfaceV1() = default;

void LayerShellExtensionManagerInterfaceV1::create(WServer *server)
{
    d->init(server->handle(), InterfaceVersion);
}

void LayerShellExtensionManagerInterfaceV1::destroy([[maybe_unused]] WServer *server)
{
    d->globalRemove();
}

wl_global *LayerShellExtensionManagerInterfaceV1::global() const
{
    return d->global();
}

QByteArrayView LayerShellExtensionManagerInterfaceV1::interfaceName() const
{
    return d->interfaceName();
}

void LayerShellExtensionManagerInterfaceV1Private::get_layer_shell_extension_object(
    Resource *resource,
    uint32_t id,
    struct ::wl_resource *surface)
{
    if (!surface) {
        qCWarning(lcTlLayerShell) << "get_object: NULL surface resource";
        wl_resource_post_error(resource->handle,
                               error_wl_surface_invalid,
                               "surface resource is NULL!");
        return;
    }

    if (!wl_resource_instance_of(surface, &wl_surface_interface, nullptr)) {
        qCWarning(lcTlLayerShell) << "get_object: surface resource is not a wl_surface";
        wl_resource_post_error(resource->handle,
                               error_wl_surface_invalid,
                               "surface resource is not a wl_surface!");
        return;
    }

    for (auto *objectSurface : std::as_const(m_objects)) {
        if (objectSurface->nativeSurface() == surface) {
            qCWarning(lcTlLayerShell)
                << "get_object: extension object already exists for this wl_surface";
            wl_resource_post_error(
                resource->handle,
                error_layer_shell_extension_exists,
                "treeland_layer_shell_extension_object_v1 already exists for this wl_surface");
            return;
        }
    }

    wl_resource *objectResource =
        wl_resource_create(resource->client(),
                           &treeland_layer_shell_extension_object_v1_interface,
                           resource->version(),
                           id);
    if (!objectResource) {
        qCWarning(lcTlLayerShell) << "get_object: failed to create object resource (OOM)";
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto *objectSurface = new LayerShellExtensionObjectV1(surface, objectResource);
    m_objects.append(objectSurface);

    QObject::connect(objectSurface, &QObject::destroyed, q, [this, objectSurface]() {
        m_objects.removeOne(objectSurface);
    });

    Q_EMIT q->objectCreated(objectSurface);
}

void LayerShellExtensionObjectV1Private::onSurfaceDestroyed(struct wlr_surface *surface)
{
    Q_UNUSED(surface);

    m_surfaceDestroy.disconnect();
    q->endResize();
    nativeSurface = nullptr;
}

LayerShellExtensionObjectV1Private::LayerShellExtensionObjectV1Private(
    LayerShellExtensionObjectV1 *_q,
    wl_resource *surface,
    wl_resource *resource)
    : QtWaylandServer::treeland_layer_shell_extension_object_v1(resource)
    , q(_q)
    , nativeSurface(surface)
{
    if (auto *wlrSurface = wlr_surface_from_resource(surface)) {
        m_surfaceDestroy.init(&wlrSurface->events.destroy,
                              this,
                              &LayerShellExtensionObjectV1Private::onSurfaceDestroyed);
    }

    auto *container = Helper::instance()->rootSurfaceContainer();
    if (container) {
        QObject::connect(container,
                         &RootSurfaceContainer::moveResizeFinised,
                         q,
                         [this](SurfaceWrapper *wrapper) {
                             if (!resizingSeat || !wrapper)
                                 return;

                             if (auto *wlrSurface = wlr_surface_from_resource(nativeSurface)) {
                                 if (wrapper->surface() == WSurface::fromHandle(wlrSurface)) {
                                     q->endResize();
                                 }
                             }
                         });
    }
}

LayerShellExtensionObjectV1Private::~LayerShellExtensionObjectV1Private() = default;

void LayerShellExtensionObjectV1Private::destroy_resource([[maybe_unused]] Resource *resource)
{
    delete q;
}

void LayerShellExtensionObjectV1Private::destroy([[maybe_unused]] Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void LayerShellExtensionObjectV1Private::registerClamp(WSeat *seat,
                                                       qreal minW,
                                                       qreal maxW,
                                                       qreal minH,
                                                       qreal maxH)
{
    if (clampRegistered)
        clearClamp();

    auto *container = Helper::instance()->rootSurfaceContainer();
    if (!container)
        return;
    auto *seatContainer = container->getSeatContainerOrDefault(seat);
    if (!seatContainer)
        return;

    // The limits are per-drag session state, stored inside the SeatSurfaceManager
    // and torn down when the drag ends; they are not properties of the surface.
    seatContainer->setResizeClamp(minW, maxW, minH, maxH);
    clampRegistered = true;
    resizingSeat = seat;
}

void LayerShellExtensionObjectV1Private::clearClamp()
{
    if (!clampRegistered)
        return;

    auto *container = Helper::instance()->rootSurfaceContainer();
    if (container) {
        auto *seatContainer = container->getSeatContainerOrDefault(resizingSeat);
        if (seatContainer)
            seatContainer->clearResizeClamp();
    }
    clampRegistered = false;
    resizingSeat = nullptr;
}

void LayerShellExtensionObjectV1Private::begin_resize(Resource *resource,
                                                      struct ::wl_resource *seatResource,
                                                      uint32_t serial,
                                                      uint32_t edges,
                                                      int32_t min_width,
                                                      int32_t min_height,
                                                      int32_t max_width,
                                                      int32_t max_height)
{
    const qreal minW = qMax(qreal(0), qreal(min_width));
    const qreal maxW = qMax(qreal(0), qreal(max_width));
    const qreal minH = qMax(qreal(0), qreal(min_height));
    const qreal maxH = qMax(qreal(0), qreal(max_height));

    // A dimension's min/max range is valid unless it is inverted with a
    // non-zero max; an inverted range is dropped, leaving that axis
    // unconstrained.
    const bool validW = !(minW > maxW && maxW != 0);
    const bool validH = !(minH > maxH && maxH != 0);
    if (!validW || !validH) {
        qCWarning(lcTlLayerShell) << "begin_resize: invalid size limits ignored:" << min_width
                            << min_height << max_width << max_height;
    }
    // Derive the effective limits, zeroing a dimension whose range is
    // inverted (min>max) so the clamp leaves it unrestricted.
    const qreal effectiveMinW = validW ? minW : 0;
    const qreal effectiveMaxW = validW ? maxW : 0;
    const qreal effectiveMinH = validH ? minH : 0;
    const qreal effectiveMaxH = validH ? maxH : 0;

    Q_ASSERT(wl_resource_instance_of(seatResource, &wl_seat_interface, nullptr));
    auto *seatClient = wlr_seat_client_from_resource(seatResource);
    if (!seatClient || !seatClient->seat) {
        qCWarning(lcTlLayerShell) << "begin_resize REJECTED: invalid seat client/resource";
        send_resize_rejected(resource->handle, resize_error_bad_serial);
        return;
    }
    WSeat *seat = WSeat::fromHandle(seatClient->seat);
    if (!seat) {
        qCWarning(lcTlLayerShell) << "begin_resize REJECTED: no WSeat for seat client";
        send_resize_rejected(resource->handle, resize_error_bad_serial);
        return;
    }

    wlr_surface *origin = nativeSurface ? wlr_surface_from_resource(nativeSurface) : nullptr;
    if (!origin) {
        qCWarning(lcTlLayerShell) << "begin_resize REJECTED: native surface invalid/destroyed";
        send_resize_rejected(resource->handle, resize_error_bad_surface);
        return;
    }
    if (!wlr_seat_validate_pointer_grab_serial(seatClient->seat, origin, serial)) {
        qCWarning(lcTlLayerShell) << "begin_resize REJECTED: serial invalid, serial =" << serial;
        send_resize_rejected(resource->handle, resize_error_bad_serial);
        return;
    }

    // Edge must be a non-zero combination of valid edge bits
    if (edges == 0 || (edges & ~kValidEdgeBits) != 0) {
        qCWarning(lcTlLayerShell) << "begin_resize REJECTED: bad edges =" << edges;
        send_resize_rejected(resource->handle, resize_error_bad_edges);
        return;
    }

    auto *container = Helper::instance()->rootSurfaceContainer();
    auto *wrapper = container->getSurface(WSurface::fromHandle(origin));
    if (!wrapper || wrapper->type() != SurfaceWrapper::Type::Layer) {
        qCWarning(lcTlLayerShell) << "begin_resize REJECTED: no Layer wrapper for surface, type ="
                            << (wrapper ? int(wrapper->type()) : -1);
        send_resize_rejected(resource->handle, resize_error_bad_surface);
        return;
    }

    if (wrapper->surfaceState() != SurfaceWrapper::State::Normal || wrapper->isAnimationRunning()) {
        qCWarning(lcTlLayerShell) << "begin_resize REJECTED: state/animation, state ="
                            << int(wrapper->surfaceState())
                            << "animRunning =" << wrapper->isAnimationRunning();
        send_resize_rejected(resource->handle, resize_error_inactive);
        return;
    }

    auto *layerSurface = qobject_cast<WLayerSurface *>(wrapper->shellSurface());
    if (!layerSurface) {
        qCWarning(lcTlLayerShell) << "begin_resize REJECTED: wrapper has no layer shell surface";
        send_resize_rejected(resource->handle, resize_error_bad_surface);
        return;
    }
    const auto anchor = layerSurface->ancher();
    if (edges & uint32_t(anchor)) {
        qCWarning(lcTlLayerShell) << "begin_resize REJECTED: resize edge overlaps anchor"
                            << "edges =" << edges << "anchor =" << int(anchor);
        send_resize_rejected(resource->handle, resize_error_bad_edges);
        return;
    }

    if (resizingSeat) {
        qCWarning(lcTlLayerShell)
            << "begin_resize REJECTED: layer surface is already being resized";
        send_resize_rejected(resource->handle, resize_error_inactive);
        return;
    }

    Qt::Edges qtEdges;
    for (const auto &entry : kEdgeMap) {
        if (edges & entry.protocolBit)
            qtEdges |= entry.qtEdge;
    }
    container->beginMoveResizeForSeat(seat, wrapper, qtEdges);

    auto *seatContainer = container->getSeatContainerOrDefault(seat);
    if (!seatContainer || seatContainer->moveResizeState().surface != wrapper) {
        qCWarning(lcTlLayerShell) << "begin_resize REJECTED: compositor did not take over resize";
        send_resize_rejected(resource->handle, resize_error_inactive);
        return;
    }
    registerClamp(seat, effectiveMinW, effectiveMaxW, effectiveMinH, effectiveMaxH);

    qCDebug(lcTlLayerShell) << "begin_resize accepted: edges =" << edges << "limits min=" << effectiveMinW
                      << "x" << effectiveMinH << "max=" << effectiveMaxW << "x" << effectiveMaxH;
    send_resizing(resource->handle, 1);
}

LayerShellExtensionObjectV1::LayerShellExtensionObjectV1(wl_resource *surface,
                                                         wl_resource *resource)
    : d(new LayerShellExtensionObjectV1Private(this, surface, resource))
{
}

LayerShellExtensionObjectV1::~LayerShellExtensionObjectV1()
{
    Q_EMIT beforeDestroy();
    if (d->clampRegistered)
        d->clearClamp();
}

WSurface *LayerShellExtensionObjectV1::wSurface() const
{
    return WSurface::fromHandle(wlr_surface_from_resource(d->nativeSurface));
}

wl_resource *LayerShellExtensionObjectV1::nativeSurface() const
{
    return d->nativeSurface;
}

void LayerShellExtensionObjectV1::sendResizing(uint32_t resizing)
{
    d->send_resizing(resizing);
}

void LayerShellExtensionObjectV1::endResize()
{
    if (!d->resizingSeat)
        return;
    d->send_resizing(0);
    d->clearClamp();
}
