// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wsurface.h"
#include "private/wglobal_p.h"

#include <QObject>
#include <QPointer>

struct wlr_buffer;
struct wlr_surface;
struct wlr_subsurface;

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WSurfacePrivate : public WWrapObjectPrivate {
public:
    WSurfacePrivate(WSurface *qq, wlr_surface *handle);
    ~WSurfacePrivate();

    inline wlr_surface *handle() const { return surfaceHandle; }

    wl_client *waylandClient() const override;

    // begin slot function
    void on_commit();
    void on_client_commit();
    // end slot function

    void init();
    struct NativeListener {
        using Callback = void (*)(WSurfacePrivate *, void *);

        wl_listener listener;
        WSurfacePrivate *owner = nullptr;
        Callback callback = nullptr;
    };

    void addListener(NativeListener &listener, wl_signal *signal, NativeListener::Callback callback);
    static void handleNativeEvent(wl_listener *listener, void *data);
    void connectNativeEvents();
    void disconnectNativeEvents();
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

    wlr_surface *surfaceHandle = nullptr;
    NativeListener commitListener;
    NativeListener mapListener;
    NativeListener unmapListener;
    NativeListener newSubsurfaceListener;
    NativeListener destroyListener;
    bool hasSubsurface = false;
    uint32_t preferredBufferScale = 1;
    uint32_t explicitPreferredBufferScale = 0;

    bool needsFrame = false;
    struct BufferUnlocker {
        void operator()(wlr_buffer *buffer) const;
    };
    std::unique_ptr<wlr_buffer, BufferUnlocker> buffer;
    QList<WOutput*> outputs;
    WOutput *framePacingOutput = nullptr;
    QMetaObject::Connection frameDoneConnection;
    QPoint bufferOffset;
};

WAYLIB_SERVER_END_NAMESPACE
