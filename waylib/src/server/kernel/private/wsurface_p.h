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
    WSurfacePrivate(WSurface *qq, QW_NAMESPACE::QWSurface *handle);
    ~WSurfacePrivate();

    wl_client *waylandClient() const override;
    wlr_surface *nativeHandle() const;

    // begin slot function
    void on_commit();
    void on_client_commit();
    // end slot function

    void init();
    void connect();
    void instantRelease();    // release qwobject etc.
    void updateOutputs();
    void setPrimaryOutput(WOutput *output);
    void setBuffer(QW_NAMESPACE::QWBuffer *newBuffer);
    void updateBuffer();
    void updatePreferredBufferScale();
    void preferredBufferScaleChange();

    WSurface *ensureSubsurface(wlr_subsurface *subsurface);
    void setSubsurface(QW_NAMESPACE::QWSubsurface *newSubsurface);
    void setHasSubsurface(bool newHasSubsurface);
    void updateHasSubsurface();

    W_DECLARE_PUBLIC(WSurface)

    QPointer<QW_NAMESPACE::QWSurface> handle;
    QPointer<QW_NAMESPACE::QWSubsurface> subsurface;
    bool hasSubsurface = false;
    bool isSubsurface = false;  // qpointer would be null due to qwsubsurface' destroy, cache here
    uint32_t preferredBufferScale = 1;
    uint32_t explicitPreferredBufferScale = 0;

    QW_NAMESPACE::QWBuffer *buffer = nullptr;
    QVector<WOutput*> outputs;
    WOutput *primaryOutput = nullptr;
    QMetaObject::Connection frameDoneConnection;
};

WAYLIB_SERVER_END_NAMESPACE
