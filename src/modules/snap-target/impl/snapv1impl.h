// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "treeland-snap-target-unstable-v1-protocol.h"

#include <wglobal.h>

#include <QObject>

WAYLIB_SERVER_BEGIN_NAMESPACE
class WSeat;
WAYLIB_SERVER_END_NAMESPACE

void handle_treeland_snap_target_v1_destroy([[maybe_unused]] wl_client *client,
                                             wl_resource *resource);
void handle_treeland_snap_target_v1_start([[maybe_unused]] wl_client *client,
                                           wl_resource *resource,
                                           wl_resource *seat,
                                           uint32_t events);
void handle_treeland_snap_target_v1_stop([[maybe_unused]] wl_client *client,
                                          wl_resource *resource);

treeland_snap_target_v1 *treeland_snap_target_v1_create_resource(wl_client *client,
                                                                   uint32_t version,
                                                                   uint32_t id);

void treeland_snap_target_v1_resource_destroy(wl_resource *resource);

struct treeland_snap_target_v1 : public QObject
{
    Q_OBJECT
public:
    wl_resource *resource{ nullptr };

    void sendSnapRegion(int32_t x, int32_t y, uint32_t width, uint32_t height);
    void sendPidfd(int pidfd);
    void sendFailed(uint32_t reason);

Q_SIGNALS:
    void startRequested(WAYLIB_SERVER_NAMESPACE::WSeat *seat, uint32_t events);
    void stopRequested();
    void beforeDestroy();
};
