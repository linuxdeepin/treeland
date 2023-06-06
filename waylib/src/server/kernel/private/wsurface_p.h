// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wsurface.h"
#include "wsignalconnector.h"
#include "wsurfacehandler_p.h"

#include <QPointer>

struct wlr_surface;

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSurfacePrivate : public WObjectPrivate {
public:
    WSurfacePrivate(WSurface *qq, WServer *server);
    ~WSurfacePrivate();

    // begin slot function
    void on_commit(void *);
    void on_client_commit(void *);
    // end slot function

    void connect();
    void updateOutputs();

    W_DECLARE_PUBLIC(WSurface)

    wlr_surface *handle = nullptr;
    WServer *server = nullptr;
    QVector<WOutput*> outputs;
    WSurfaceHandler *handler = nullptr;

    WSignalConnector sc;
};

WAYLIB_SERVER_END_NAMESPACE
