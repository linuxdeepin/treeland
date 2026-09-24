// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "snapv1impl.h"

#include <wseat.h>
#include <wlr_all.h>

WAYLIB_SERVER_USE_NAMESPACE

static const struct treeland_snap_target_v1_interface snap_impl = {
    .destroy = handle_treeland_snap_target_v1_destroy,
    .start = handle_treeland_snap_target_v1_start,
    .stop = handle_treeland_snap_target_v1_stop,
};

static treeland_snap_target_v1 *snap_target_from_resource(wl_resource *resource)
{
    Q_ASSERT(wl_resource_instance_of(resource, &treeland_snap_target_v1_interface, &snap_impl));
    return static_cast<treeland_snap_target_v1 *>(wl_resource_get_user_data(resource));
}

treeland_snap_target_v1 *treeland_snap_target_v1_create_resource(wl_client *client,
                                                                   uint32_t version,
                                                                   uint32_t id)
{
    auto *snap = new treeland_snap_target_v1;

    wl_resource *resource =
        wl_resource_create(client, &treeland_snap_target_v1_interface, version, id);
    if (!resource) {
        wl_client_post_no_memory(client);
        delete snap;
        return nullptr;
    }
    wl_resource_set_implementation(resource,
                                   &snap_impl,
                                   snap,
                                   treeland_snap_target_v1_resource_destroy);
    snap->resource = resource;
    return snap;
}

void treeland_snap_target_v1_resource_destroy(wl_resource *resource)
{
    auto snap = snap_target_from_resource(resource);
    if (!snap)
        return;
    Q_EMIT snap->beforeDestroy();
    snap->resource = nullptr;
    snap->deleteLater();
}

void handle_treeland_snap_target_v1_destroy([[maybe_unused]] wl_client *client,
                                             wl_resource *resource)
{
    wl_resource_destroy(resource);
}

void handle_treeland_snap_target_v1_start([[maybe_unused]] wl_client *client,
                                           wl_resource *resource,
                                           wl_resource *seatResource,
                                           uint32_t events)
{
    auto snap = snap_target_from_resource(resource);
    Q_ASSERT(snap);

    WSeat *seat = nullptr;
    if (seatResource) {
        auto *seatClient =
            static_cast<struct wlr_seat_client *>(wl_resource_get_user_data(seatResource));
        if (seatClient)
            seat = WSeat::fromHandle(seatClient->seat);
    }

    Q_EMIT snap->startRequested(seat, events);
}

void handle_treeland_snap_target_v1_stop([[maybe_unused]] wl_client *client, wl_resource *resource)
{
    auto snap = snap_target_from_resource(resource);
    Q_ASSERT(snap);
    Q_EMIT snap->stopRequested();
}

void treeland_snap_target_v1::sendSnapRegion(int32_t x, int32_t y, uint32_t width, uint32_t height)
{
    Q_ASSERT(resource);
    treeland_snap_target_v1_send_snap_region(resource, x, y, width, height);
}

void treeland_snap_target_v1::sendPidfd(int pidfd)
{
    Q_ASSERT(resource);
    treeland_snap_target_v1_send_pidfd(resource, pidfd);
}

void treeland_snap_target_v1::sendFailed(uint32_t reason)
{
    Q_ASSERT(resource);
    treeland_snap_target_v1_send_failed(resource, reason);
}
