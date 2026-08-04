// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <QObject>

#include <wlr/interfaces/wlr_ext_image_capture_source_v1.h>

struct wlr_seat;

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSurfaceItemContent;
class WOutput;

class WAYLIB_SERVER_EXPORT WExtImageCaptureSourceV1Impl : public QObject
{
    Q_OBJECT
public:
    explicit WExtImageCaptureSourceV1Impl(WSurfaceItemContent *surfaceContent, WOutput *output);
    ~WExtImageCaptureSourceV1Impl();

    wlr_ext_image_capture_source_v1 *handle() const {
        return const_cast<wlr_ext_image_capture_source_v1*>(&m_source);
    }

private:
    // Interface methods called by static C callbacks
    void start(bool with_cursors);
    void stop();
    void schedule_frame();
    void copy_frame(wlr_ext_image_copy_capture_frame_v1 *dst_frame,
                    wlr_ext_image_capture_source_v1_frame_event *frame_event);
    wlr_ext_image_capture_source_v1_cursor *get_pointer_cursor(wlr_seat *seat);

    // Static C callback dispatchers
    static void impl_start(wlr_ext_image_capture_source_v1 *source, bool with_cursors);
    static void impl_stop(wlr_ext_image_capture_source_v1 *source);
    static void impl_schedule_frame(wlr_ext_image_capture_source_v1 *source);
    static void impl_copy_frame(wlr_ext_image_capture_source_v1 *source,
                                wlr_ext_image_copy_capture_frame_v1 *dst_frame,
                                wlr_ext_image_capture_source_v1_frame_event *frame_event);
    static wlr_ext_image_capture_source_v1_cursor *impl_get_pointer_cursor(
        wlr_ext_image_capture_source_v1 *source, wlr_seat *seat);

    static WExtImageCaptureSourceV1Impl *getImpl(wlr_ext_image_capture_source_v1 *source);
    void handleRenderEnd();

    static const struct wlr_ext_image_capture_source_v1_interface s_impl;

    QPointer<WSurfaceItemContent> m_surfaceContent;
    WOutput *m_output;
    bool m_capturing;
    QMetaObject::Connection m_renderEndConnection;
    struct wlr_ext_image_capture_source_v1 m_source;
};

WAYLIB_SERVER_END_NAMESPACE
