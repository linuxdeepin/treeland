// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "regionwatchmanagerinterfacev1.h"
#include "qwayland-server-treeland-region-watch-unstable-v1.h"
#include "surface/surfacewrapper.h"

#include <woutput.h>

#include <QTimer>

#include <string.h>

#include <wayland-server.h>
#include <wlr/types/wlr_output.h>

// Errors of treeland_region_watch_v1, mirroring the protocol XML.
enum RegionWatchError {
    InvalidAnchor = 0,
    InvalidSize = 1,
};

class RegionWatchV1Private : public QtWaylandServer::treeland_region_watch_v1
{
public:
    explicit RegionWatchV1Private(RegionWatchV1 *_q, wl_resource *resource)
        : QtWaylandServer::treeland_region_watch_v1(resource)
        , q(_q)
    {
    }

    RegionWatchV1 *q = nullptr;

    void evaluate(const QList<QRect> &windowRects);
    void onOutputDestroyed();

    // Monitored region in global layout coordinates (output position +
    // strip); empty when the watcher is
    // inert (no successful set_region yet, or the associated output was
    // removed).
    QRect region;
    QMetaObject::Connection outputDestroyConnection;
    // Overlap state tracking. stateSent is cleared by set_region (the
    // protocol requires sending the current state after each set_region)
    // and on output removal; afterwards events are only sent on changes.
    bool stateSent = false;
    bool lastOverlapped = false;

protected:
    void destroy_resource(Resource *resource) override;
    void destroy(Resource *resource) override;
    void set_region(Resource *resource,
                    int32_t width,
                    int32_t height,
                    uint32_t anchor,
                    struct ::wl_resource *output) override;
};

void RegionWatchV1Private::evaluate(const QList<QRect> &windowRects)
{
    if (region.isEmpty())
        return;

    bool overlapped = false;
    for (const QRect &rect : windowRects) {
        if (rect.intersects(region)) {
            overlapped = true;
            break;
        }
    }

    if (stateSent && overlapped == lastOverlapped)
        return;

    stateSent = true;
    lastOverlapped = overlapped;

    if (overlapped)
        send_enter();
    else
        send_leave();
}

void RegionWatchV1Private::onOutputDestroyed()
{
    // The associated output is gone: the watcher becomes inert and must
    // not send enter/leave/output_removed until the next set_region.
    region = QRect();
    stateSent = false;
    lastOverlapped = false;
    QObject::disconnect(outputDestroyConnection);
    outputDestroyConnection = QMetaObject::Connection();
    send_output_removed();
}

void RegionWatchV1Private::destroy_resource([[maybe_unused]] Resource *resource)
{
    // The list entry is removed by the QObject::destroyed lambda registered
    // in get_region_watch.
    delete q;
}

void RegionWatchV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void RegionWatchV1Private::set_region(Resource *resource,
                                      int32_t width,
                                      int32_t height,
                                      uint32_t anchor,
                                      wl_resource *outputResource)
{
    if (width <= 0 || height <= 0) {
        wl_resource_post_error(resource->handle,
                               InvalidSize,
                               "invalid treeland_region_watch_v1::set_region size: %dx%d",
                               width,
                               height);
        return;
    }

    // wlr_output_from_resource() asserts that the resource is a genuine
    // wl_output; a client may pass any live object id (or an inert one),
    // which would abort the compositor before the checks below. Validate
    // the object class first and route invalid references to the error path.
    if (!outputResource
        || strcmp(wl_resource_get_class(outputResource), wl_output_interface.name) != 0) {
        wl_resource_post_error(resource->handle,
                               InvalidAnchor,
                               "set_region requires a valid wl_output resource");
        return;
    }

    struct wlr_output *output = wlr_output_from_resource(outputResource);
    auto *wOutput = output ? WOutput::fromHandle(output) : nullptr;
    if (!wOutput) {
        wl_resource_post_error(resource->handle,
                               InvalidAnchor,
                               "wlr_output_from_resource failed in treeland_region_watch_v1::set_region");
        return;
    }

    const QSizeF wSize = wOutput->size() / wOutput->scale();
    switch (anchor) {
    case RegionWatchV1::Anchor::Top:
        region = QRect(0, 0, wSize.width(), height);
        break;
    case RegionWatchV1::Anchor::Bottom:
        region = QRect(0, wSize.height() - height, wSize.width(), height);
        break;
    case RegionWatchV1::Anchor::Left:
        region = QRect(0, 0, width, wSize.height());
        break;
    case RegionWatchV1::Anchor::Right:
        region = QRect(wSize.width() - width, 0, width, wSize.height());
        break;
    default:
        wl_resource_post_error(resource->handle,
                               InvalidAnchor,
                               "invalid treeland_region_watch_v1::set_region anchor: %u",
                               anchor);
        return;
    }

    // Window rects are tracked in global layout coordinates (SurfaceWrapper
    // positions), so translate the output-local strip by the output's
    // position in the layout.
    region.translate(wOutput->position());

    if (outputDestroyConnection) {
        QObject::disconnect(outputDestroyConnection);
        outputDestroyConnection = QMetaObject::Connection();
    }
    outputDestroyConnection =
        QObject::connect(wOutput, &WOutput::beforeDestroy, q, &RegionWatchV1::onOutputDestroyed);

    // The protocol requires evaluating the overlap state right after each
    // set_region and sending the matching enter/leave event.
    stateSent = false;
    if (auto *manager = q->manager())
        manager->recheck();
}

class TreelandRegionWatchManagerInterfaceV1Private
    : public QtWaylandServer::treeland_region_watch_manager_v1
{
public:
    explicit TreelandRegionWatchManagerInterfaceV1Private(TreelandRegionWatchManagerInterfaceV1 *_q)
        : q(_q)
    {
    }

    TreelandRegionWatchManagerInterfaceV1 *q = nullptr;

    wl_global *global() const
    {
        return m_global;
    }

    void checkConflict(const QList<QRect> &windowRects);
    void addSurface(SurfaceWrapper *wrapper);
    void removeSurface(SurfaceWrapper *wrapper);
    QList<QRect> windowRects() const;
    void scheduleRecheck();

    QTimer recheckTimer;
    QList<SurfaceWrapper *> surfaces;
    QList<RegionWatchV1 *> regionWatches;

protected:
    void destroy(Resource *resource) override;
    void get_region_watch(Resource *resource, uint32_t id) override;
};

void TreelandRegionWatchManagerInterfaceV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void TreelandRegionWatchManagerInterfaceV1Private::get_region_watch(Resource *resource,
                                                                     uint32_t id)
{
    wl_resource *watchResource = wl_resource_create(resource->client(),
                                                    &treeland_region_watch_v1_interface,
                                                    resource->version(),
                                                    id);
    if (!watchResource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto watch = new RegionWatchV1(watchResource);
    watch->m_manager = q;
    regionWatches.append(watch);

    QObject::connect(watch, &QObject::destroyed, [this, watch]() {
        regionWatches.removeOne(watch);
    });

    Q_EMIT q->regionWatchCreated(watch);
}

void TreelandRegionWatchManagerInterfaceV1Private::checkConflict(const QList<QRect> &windowRects)
{
    for (auto *watch : std::as_const(regionWatches)) {
        watch->d->evaluate(windowRects);
    }
}

void TreelandRegionWatchManagerInterfaceV1Private::addSurface(SurfaceWrapper *wrapper)
{
    if (!wrapper || surfaces.contains(wrapper))
        return;

    // The protocol reports overlap with xdg-shell toplevels and the
    // pre-launch splash screen; popups and other transient surfaces must
    // not trigger enter/leave.
    const bool trackedType = wrapper->type() == SurfaceWrapper::Type::XdgToplevel
        || wrapper->type() == SurfaceWrapper::Type::XWayland
        || wrapper->type() == SurfaceWrapper::Type::SplashScreen;
    if (!trackedType)
        return;

    surfaces.append(wrapper);
    QObject::connect(wrapper, &QQuickItem::xChanged, q, [this] { scheduleRecheck(); });
    QObject::connect(wrapper, &QQuickItem::yChanged, q, [this] { scheduleRecheck(); });
    QObject::connect(wrapper, &QQuickItem::widthChanged, q, [this] { scheduleRecheck(); });
    QObject::connect(wrapper, &QQuickItem::heightChanged, q, [this] { scheduleRecheck(); });
    QObject::connect(wrapper, &QQuickItem::visibleChanged, q, [this] { scheduleRecheck(); });
    QObject::connect(wrapper, &SurfaceWrapper::aboutToBeInvalidated, q, [this, wrapper] {
        surfaces.removeOne(wrapper);
        scheduleRecheck();
    });
    scheduleRecheck();
}

void TreelandRegionWatchManagerInterfaceV1Private::removeSurface(SurfaceWrapper *wrapper)
{
    if (surfaces.removeOne(wrapper))
        scheduleRecheck();
}

QList<QRect> TreelandRegionWatchManagerInterfaceV1Private::windowRects() const
{
    QList<QRect> rects;
    rects.reserve(surfaces.size());
    for (const auto wrapper : std::as_const(surfaces)) {
        if (wrapper->isVisible()) {
            rects.append(QRectF{ wrapper->x(), wrapper->y(),
                                  wrapper->width(), wrapper->height() }.toRect());
        }
    }
    return rects;
}

void TreelandRegionWatchManagerInterfaceV1Private::scheduleRecheck()
{
    // Recheck once per burst of geometry changes instead of on every
    // intermediate position, matching the overlap-check debounce.
    if (!recheckTimer.isActive())
        recheckTimer.start();
}

TreelandRegionWatchManagerInterfaceV1::TreelandRegionWatchManagerInterfaceV1(QObject *parent)
    : QObject(parent)
    , d(new TreelandRegionWatchManagerInterfaceV1Private(this))
{
    d->recheckTimer.setSingleShot(true);
    d->recheckTimer.setInterval(300);
    connect(&d->recheckTimer, &QTimer::timeout, this, [this] {
        d->checkConflict(d->windowRects());
    });
}

TreelandRegionWatchManagerInterfaceV1::~TreelandRegionWatchManagerInterfaceV1() = default;

void TreelandRegionWatchManagerInterfaceV1::create(WServer *server)
{
    d->init(server->handle(), InterfaceVersion);
}

void TreelandRegionWatchManagerInterfaceV1::destroy([[maybe_unused]] WServer *server)
{
    d->globalRemove();
}

wl_global *TreelandRegionWatchManagerInterfaceV1::global() const
{
    return d->global();
}

QByteArrayView TreelandRegionWatchManagerInterfaceV1::interfaceName() const
{
    return d->interfaceName();
}

void TreelandRegionWatchManagerInterfaceV1::checkOverlapConflict(const QList<QRect> &windowRects)
{
    d->checkConflict(windowRects);
}

void TreelandRegionWatchManagerInterfaceV1::addSurface(SurfaceWrapper *wrapper)
{
    d->addSurface(wrapper);
}

void TreelandRegionWatchManagerInterfaceV1::removeSurface(SurfaceWrapper *wrapper)
{
    d->removeSurface(wrapper);
}

void TreelandRegionWatchManagerInterfaceV1::recheck()
{
    d->checkConflict(d->windowRects());
}

void TreelandRegionWatchManagerInterfaceV1::scheduleRecheck()
{
    d->scheduleRecheck();
}

RegionWatchV1::RegionWatchV1(wl_resource *resource)
    : d(new RegionWatchV1Private(this, resource))
{
}

RegionWatchV1::~RegionWatchV1() = default;

void RegionWatchV1::onOutputDestroyed()
{
    d->onOutputDestroyed();
}

TreelandRegionWatchManagerInterfaceV1 *RegionWatchV1::manager() const
{
    return m_manager;
}

QRect RegionWatchV1::region() const
{
    return d->region;
}
