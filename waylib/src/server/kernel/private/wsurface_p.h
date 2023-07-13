// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wsurface.h"

#include <qwglobal.h>
#include <QObject>
#include <QPointer>

struct wlr_surface;

QW_BEGIN_NAMESPACE
class QWSurface;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSurfacePrivate : public WObjectPrivate {
public:
    WSurfacePrivate(WSurface *qq, WServer *server);
    ~WSurfacePrivate();

    wlr_surface *nativeHandle() const;

    // begin slot function
    void on_commit();
    void on_client_commit();
    // end slot function

    void connect();
    void updateOutputs();

    W_DECLARE_PUBLIC(WSurface)

    QW_NAMESPACE::QWSurface *handle = nullptr;
    WServer *server = nullptr;
    QVector<WOutput*> outputs;
    WSurfaceHandler *handler = nullptr;
    QObject *shell = nullptr;
};

WAYLIB_SERVER_END_NAMESPACE
