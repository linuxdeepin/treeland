// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "treeland-capture-snap-unstable-v1-protocol.h"

#include <QObject>

void handle_treeland_capture_snap_v1_destroy([[maybe_unused]] wl_client *client,
                                             wl_resource *resource);
void handle_treeland_capture_snap_v1_start([[maybe_unused]] wl_client *client,
                                           wl_resource *resource);
void handle_treeland_capture_snap_v1_stop([[maybe_unused]] wl_client *client,
                                          wl_resource *resource);

treeland_capture_snap_v1 *treeland_capture_snap_v1_create_resource(wl_client *client,
                                                                   uint32_t version,
                                                                   uint32_t id);

void treeland_capture_snap_v1_resource_destroy(wl_resource *resource);

struct treeland_capture_snap_v1 : public QObject
{
    Q_OBJECT
public:
    wl_resource *resource{ nullptr };

    void sendSnapRegion(int32_t x, int32_t y, uint32_t width, uint32_t height);
    void sendFailed(uint32_t reason);

Q_SIGNALS:
    void startRequested();
    void stopRequested();
    void beforeDestroy();
};
