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
    virtual void on_commit();
    void on_client_commit();
    // end slot function

    void connect();
    void updateOutputs();
    void setPrimaryOutput(WOutput *output);
    void setBuffer(QW_NAMESPACE::QWBuffer *newBuffer);
    void updateBuffer();

    W_DECLARE_PUBLIC(WSurface)

    QW_NAMESPACE::QWSurface *handle = nullptr;
    QW_NAMESPACE::QWBuffer *buffer = nullptr;
    mutable QW_NAMESPACE::QWTexture *texture = nullptr;
    WServer *server = nullptr;
    QVector<WOutput*> outputs;
    WOutput *primaryOutput = nullptr;
    QMetaObject::Connection frameDoneConnection;

    WSurfaceHandler *handler = nullptr;
    QObject *shell = nullptr;
};

WAYLIB_SERVER_END_NAMESPACE
