// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <QObject>

extern "C" {
#include <wlr/interfaces/wlr_ext_image_capture_source_v1.h>
}

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSurfaceItemContent;
class WOutput;

class WAYLIB_SERVER_EXPORT WExtImageCaptureSourceV1Impl : public QObject
{
    Q_OBJECT
public:
    explicit WExtImageCaptureSourceV1Impl(WSurfaceItemContent *surfaceContent, WOutput *output);
    ~WExtImageCaptureSourceV1Impl();

    wlr_ext_image_capture_source_v1 *handle();

    void start(bool with_cursors);
    void stop();
    void scheduleFrame();
    void copyFrame(wlr_ext_image_copy_capture_frame_v1 *dst_frame,
                   wlr_ext_image_capture_source_v1_frame_event *frame_event);
    wlr_ext_image_capture_source_v1_cursor *getPointerCursor(wlr_seat *seat);

private Q_SLOTS:
    void handleRenderEnd();

private:
    static WExtImageCaptureSourceV1Impl *fromHandle(wlr_ext_image_capture_source_v1 *source);
    static void startCallback(wlr_ext_image_capture_source_v1 *source, bool with_cursors);
    static void stopCallback(wlr_ext_image_capture_source_v1 *source);
    static void scheduleFrameCallback(wlr_ext_image_capture_source_v1 *source);
    static void copyFrameCallback(wlr_ext_image_capture_source_v1 *source,
                                  wlr_ext_image_copy_capture_frame_v1 *dst_frame,
                                  wlr_ext_image_capture_source_v1_frame_event *frame_event);
    static wlr_ext_image_capture_source_v1_cursor *getPointerCursorCallback(
        wlr_ext_image_capture_source_v1 *source, wlr_seat *seat);

    wlr_ext_image_capture_source_v1 m_handle;
    QPointer<WSurfaceItemContent> m_surfaceContent;
    WOutput *m_output;
    bool m_capturing;
    QMetaObject::Connection m_renderEndConnection;
};

WAYLIB_SERVER_END_NAMESPACE
