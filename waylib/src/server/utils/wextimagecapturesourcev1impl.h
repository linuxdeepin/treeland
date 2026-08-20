// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wlr_all.h>
#include <wlr_fwd.h>
#include <wpointer.h>

#include <QObject>
#include <QPointer>
#include <QQuickItem>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutput;
class WOutputRenderWindow;
class WBufferRenderer;

class WAYLIB_SERVER_EXPORT WExtImageCaptureSourceV1Impl : public QObject
{
    Q_OBJECT
public:
    // surfaceItem is the WSurfaceItem (includes surface content + subsurfaces + titleBar,
    // but excludes shadow/decoration which live on the parent SurfaceWrapper).
    explicit WExtImageCaptureSourceV1Impl(QQuickItem *surfaceItem, WOutput *output);
    ~WExtImageCaptureSourceV1Impl();

    wlr_ext_image_capture_source_v1 *handle() { return &source; }

private:
    static const struct wlr_ext_image_capture_source_v1_interface impl;
    void start(bool with_cursors);
    void stop();
    void schedule_frame();
    void copy_frame(wlr_ext_image_copy_capture_frame_v1 *dst_frame,
                    wlr_ext_image_capture_source_v1_frame_event *frame_event);
    wlr_ext_image_capture_source_v1_cursor *get_pointer_cursor(wlr_seat *seat);
    static void start(struct wlr_ext_image_capture_source_v1 *source, bool with_cursors);
    static void stop(struct wlr_ext_image_capture_source_v1 *source);
    static void schedule_frame(struct wlr_ext_image_capture_source_v1 *source);
    static void copy_frame(struct wlr_ext_image_capture_source_v1 *source,
                           wlr_ext_image_copy_capture_frame_v1 *dst_frame,
                           wlr_ext_image_capture_source_v1_frame_event *frame_event);
    static wlr_ext_image_capture_source_v1_cursor *get_pointer_cursor(
        struct wlr_ext_image_capture_source_v1 *source, struct wlr_seat *seat);

private Q_SLOTS:
    // Phase 1 (afterRendering): offscreen render via WBufferRenderer
    void doOffscreenRender();
    // Phase 2 (renderEnd): emit frame event, copy_frame uses m_renderedBuffer
    void handleRenderEnd();

private:
    wlr_ext_image_capture_source_v1 source;

    WOutputRenderWindow *renderWindow() const;
    qreal computeDpr() const;
    QSize computePixelSize() const;
    void updateConstraints(const QSize &pixelSize);

    QPointer<QQuickItem> m_surfaceItem;
    WOutput *m_output;
    bool m_capturing;
    QMetaObject::Connection m_afterRenderingConnection;
    QMetaObject::Connection m_renderEndConnection;

    QPointer<WBufferRenderer> m_captureRenderer;
    // Buffer from offscreen render, valid between doOffscreenRender and copy_frame
    WPointer<wlr_buffer> m_renderedBuffer;
};

WAYLIB_SERVER_END_NAMESPACE
