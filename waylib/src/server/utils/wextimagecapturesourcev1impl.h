// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <qwextimagecapturesourcev1interface.h>

#include <QObject>

Q_DECLARE_LOGGING_CATEGORY(qLcImageCapture)

QW_BEGIN_NAMESPACE
class qw_buffer;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSurfaceItemContent;
class WOutput;

class WAYLIB_SERVER_EXPORT WExtImageCaptureSourceV1Impl : public QObject, public QW_NAMESPACE::qw_ext_image_capture_source_v1_interface
{
    Q_OBJECT
public:
    explicit WExtImageCaptureSourceV1Impl(WSurfaceItemContent *surfaceContent, WOutput *output);
    ~WExtImageCaptureSourceV1Impl();

    QW_INTERFACE(start, void, bool with_cursors);
    QW_INTERFACE(stop, void);
    QW_INTERFACE(schedule_frame, void);
    QW_INTERFACE(copy_frame, void, wlr_ext_image_copy_capture_frame_v1 *dst_frame,
                 wlr_ext_image_capture_source_v1_frame_event *frame_event);
    QW_INTERFACE(get_pointer_cursor, wlr_ext_image_capture_source_v1_cursor *, wlr_seat *seat);

private Q_SLOTS:
    void handleRenderEnd();

private:
    QPointer<WSurfaceItemContent> m_surfaceContent;
    WOutput *m_output;
    bool m_capturing;
    QMetaObject::Connection m_renderEndConnection;
};

WAYLIB_SERVER_END_NAMESPACE
