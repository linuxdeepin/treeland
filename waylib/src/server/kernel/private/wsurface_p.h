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

    WSubsurface *ensureSubsurface(wlr_subsurface *subsurface);
    WSubsurface *addRemoteSubsurface(wlr_surface *childHandle);
    void removeSubsurface(WSubsurface *subsurface);
    void releaseSubsurface(WSubsurface *subsurface);
    void updateStandardSubsurfaces();
    void setSubsurfaceOrder(const QList<WSubsurface *> &remoteBelow,
                            const QList<WSubsurface *> &remoteAbove);
    void rebuildTotalSubsurfaces();
    void setHasSubsurface(bool newHasSubsurface);

    W_DECLARE_PUBLIC(WSurface)

    friend class WRemoteSubsurfaceManagerV1Private;

    // Logical subsurface relations (standard + remote) exposed via WSurface API.
    QList<WSubsurface *> standardBelow;
    QList<WSubsurface *> standardAbove;
    QList<WSubsurface *> remoteBelow;
    QList<WSubsurface *> remoteAbove;
    QList<WSubsurface *> subsurfaces;
    QPointer<WSubsurface> subsurface;
    bool hasSubsurface = false;
    uint32_t preferredBufferScale = 1;
    uint32_t explicitPreferredBufferScale = 0;

    bool needsFrame = false;
    WBufferUnlockPtr buffer;
    QList<WOutput*> outputs;
    WOutput *framePacingOutput = nullptr;
    QMetaObject::Connection frameDoneConnection;
    QPoint bufferOffset;

private:
    // The surface owner destroys this handle after notifying the wrapper.
    // Keep the address stable through owner callbacks.
    wlr_surface *m_handle = nullptr;
};

WAYLIB_SERVER_END_NAMESPACE
