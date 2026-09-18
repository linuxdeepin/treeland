// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "regionwatchmanagerinterfacev1.h"
#include "qwayland-server-treeland-region-watch-unstable-v1.h"
#include "seat/helper.h"

#include <woutput.h>

#include <string.h>

#include <wayland-server.h>
#include <wlr/types/wlr_output.h>

static QList<RegionWatchV1 *> s_regionWatches;

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
    outputDestroyConnection = QObject::connect(wOutput, &WOutput::beforeDestroy, q, [this] {
        // The associated output is gone: the watcher becomes inert and must
        // not send enter/leave/output_removed until the next set_region.
        region = QRect();
        stateSent = false;
        lastOverlapped = false;
        QObject::disconnect(outputDestroyConnection);
        outputDestroyConnection = QMetaObject::Connection();
        send_output_removed();
    });

    // The protocol requires evaluating the overlap state right after each
    // set_region and sending the matching enter/leave event.
    stateSent = false;
    if (auto *helper = Helper::instance())
        evaluate(helper->regionWatchWindowRects());
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
    s_regionWatches.append(watch);

    QObject::connect(watch, &QObject::destroyed, [watch]() {
        s_regionWatches.removeOne(watch);
    });

    Q_EMIT q->regionWatchCreated(watch);
}

void TreelandRegionWatchManagerInterfaceV1Private::checkConflict(const QList<QRect> &windowRects)
{
    for (auto *watch : std::as_const(s_regionWatches)) {
        watch->d->evaluate(windowRects);
    }
}

TreelandRegionWatchManagerInterfaceV1::TreelandRegionWatchManagerInterfaceV1(QObject *parent)
    : QObject(parent)
    , d(new TreelandRegionWatchManagerInterfaceV1Private(this))
{
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

RegionWatchV1::RegionWatchV1(wl_resource *resource)
    : d(new RegionWatchV1Private(this, resource))
{
}

RegionWatchV1::~RegionWatchV1() = default;

QRect RegionWatchV1::region() const
{
    return d->region;
}
