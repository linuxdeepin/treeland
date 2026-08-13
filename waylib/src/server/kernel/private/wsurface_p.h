// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wlr_fwd.h>
#include "wsurface.h"
#include "private/wwaylandresource_p.h"
#include "wpointer.h"
#include "wscoplistener.h"

#include <wlr_all.h>

#include <QObject>
#include <QPointer>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WSurfacePrivate : public WWaylandResourcePrivate {
public:
    WSurfacePrivate(WSurface *qq, wlr_surface *handle);
    ~WSurfacePrivate();

    inline wlr_surface *handle() const {
        return m_handle;
    }

    wl_client *waylandClient() const override;

    // begin slot function
    void on_commit();
    void on_client_commit();
    void init();
    void connect();
    void updateOutputs();
    void setBuffer(wlr_buffer *newBuffer);
    void updateBuffer();
    void updateBufferOffset();
    void updatePreferredBufferScale();
    void preferredBufferScaleChange();

    WSurface *ensureSubsurface(wlr_subsurface *subsurface);
    void setHasSubsurface(bool newHasSubsurface);
    void updateHasSubsurface();

    W_DECLARE_PUBLIC(WSurface)

    bool hasSubsurface = false;
    uint32_t preferredBufferScale = 1;
    uint32_t explicitPreferredBufferScale = 0;

    bool needsFrame = false;
    WBufferUnlockPtr buffer;
    QList<WOutput*> outputs;
    QList<WSurface*> subSurfaces;
    WOutput *framePacingOutput = nullptr;
    QMetaObject::Connection frameDoneConnection;
    QPoint bufferOffset;

private:
    // The surface owner destroys this handle after notifying the wrapper.
    // Keep the address stable through owner callbacks.
    wlr_surface *m_handle = nullptr;
};

WAYLIB_SERVER_END_NAMESPACE
