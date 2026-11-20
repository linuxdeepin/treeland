// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wlr_fwd.h>
#include <wglobal.h>
#include <wlr_all.h>

#include <QHash>
#include <QList>
#include <QObject>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutputViewport;
struct SourceResourceDestroyListener;

class WAYLIB_SERVER_EXPORT WExtImageCaptureSourceV1Impl : public QObject
{
    Q_OBJECT
public:
    explicit WExtImageCaptureSourceV1Impl(QObject *parent = nullptr);
    ~WExtImageCaptureSourceV1Impl() override;

    wlr_ext_image_capture_source_v1 *handle() { return &source; }

    // Accepts a foreign-toplevel capture request and tracks the client
    // resource, so the source can be recycled once the client destroys it.
    bool accept(wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request *request);

public Q_SLOTS:
    void invalidate();

protected:
    // Creates the offscreen viewport for the next capture session. Called from
    // start(), before wlroots sends the session constraints, and again for
    // every new session after stop() released the previous viewport.
    virtual WOutputViewport *createViewport() = 0;

private:
    static const struct wlr_ext_image_capture_source_v1_interface impl;
    void start(bool with_cursors);
    void stop();
    void schedule_frame(bool schedule_frame);
    void copy_frame(wlr_ext_image_copy_capture_frame_v1 *dst_frame,
                    wlr_ext_image_capture_source_v1_frame_event *frame_event);
    static void start(struct wlr_ext_image_capture_source_v1 *source, bool with_cursors);
    static void stop(struct wlr_ext_image_capture_source_v1 *source);
    static void request_frame(struct wlr_ext_image_capture_source_v1 *source, bool schedule_frame);
    static void copy_frame(struct wlr_ext_image_capture_source_v1 *source,
                           wlr_ext_image_copy_capture_frame_v1 *dst_frame,
                           wlr_ext_image_capture_source_v1_frame_event *frame_event);
    static wlr_ext_image_capture_source_v1_cursor *get_pointer_cursor(
        struct wlr_ext_image_capture_source_v1 *source, struct wlr_seat *seat);

    WOutputViewport *ensureViewport();
    void releaseViewport();
    void attachResourceListeners();
    void detachResourceListeners();
    // Reclaims this source when no client resource and no session are left.
    void maybeRecycle();
    static void resourceDestroyed(wl_listener *listener, void *data);

private Q_SLOTS:
    void handleFrameRendered();
    void updateConstraints();

private:
    wlr_ext_image_capture_source_v1 source;

    QPointer<WOutputViewport> m_viewport;
    QList<QMetaObject::Connection> m_viewportConnections;
    QHash<wl_resource *, SourceResourceDestroyListener *> m_resourceListeners;
    size_t m_numStarted = 0;
    bool m_constraintsFromSwapchain = false;
    bool m_finished = false;
    bool m_recycleScheduled = false;
    bool m_inDestructor = false;
};

WAYLIB_SERVER_END_NAMESPACE
