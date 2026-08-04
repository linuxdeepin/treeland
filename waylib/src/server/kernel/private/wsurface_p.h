// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wsurface.h"
#include "private/wglobal_p.h"


#include <QObject>
#include <QPointer>

struct wlr_surface;
struct wlr_subsurface;

WAYLIB_SERVER_BEGIN_NAMESPACE

struct WBufferUnlocker {
    void operator()(wlr_buffer *buf) const { if (buf) wlr_buffer_unlock(buf); }
};

class Q_DECL_HIDDEN WSurfacePrivate : public WWrapObjectPrivate {
public:
    WSurfacePrivate(WSurface *qq, wlr_surface *handle);
    ~WSurfacePrivate();

    WWRAP_NATIVE_HANDLE_FUNCTIONS(wlr_surface)

    wl_client *waylandClient() const override;

    // begin slot function
    void on_commit();
    void on_client_commit();
    // end slot function

    void init();
    void connect();
    void instantRelease() override;    // release qwobject etc.
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

    WScopedListener m_commitListener;
    WScopedListener m_mapListener;
    WScopedListener m_unmapListener;
    WScopedListener m_newSubsurfaceListener;

    wlr_subsurface *subsurface = nullptr;
    bool hasSubsurface = false;
    uint32_t preferredBufferScale = 1;
    uint32_t explicitPreferredBufferScale = 0;

    bool needsFrame = false;
    std::unique_ptr<wlr_buffer, WBufferUnlocker> buffer;
    QList<WOutput*> outputs;
    WOutput *framePacingOutput = nullptr;
    QMetaObject::Connection frameDoneConnection;
    QPoint bufferOffset;
};

WAYLIB_SERVER_END_NAMESPACE
