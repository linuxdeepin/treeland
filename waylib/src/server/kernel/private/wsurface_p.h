// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wsurface.h"

#include <qwglobal.h>
#include <QObject>
#include <QPointer>

struct wlr_surface;
struct wlr_subsurface;

QW_BEGIN_NAMESPACE
class QWSurface;
class QWSubsurface;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSurfacePrivate : public WObjectPrivate {
public:
    WSurfacePrivate(WSurface *qq, WServer *server);
    ~WSurfacePrivate();

    wl_client *waylandClient() const override;
    wlr_surface *nativeHandle() const;

    // begin slot function
    void on_commit();
    void on_client_commit();
    // end slot function

    void connect();
    void updateOutputs();
    void setPrimaryOutput(WOutput *output);
    void setBuffer(QW_NAMESPACE::QWBuffer *newBuffer);
    void updateBuffer();

    WSurface *ensureSubsurface(wlr_subsurface *subsurface);
    void setSubsurface(QW_NAMESPACE::QWSubsurface *newSubsurface);
    void setHasSubsurface(bool newHasSubsurface);
    void updateHasSubsurface();

    W_DECLARE_PUBLIC(WSurface)

    QW_NAMESPACE::QWSurface *handle = nullptr;
    QPointer<QW_NAMESPACE::QWSubsurface> subsurface;
    bool hasSubsurface = false;

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
