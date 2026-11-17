// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wextimagecapturesourcev1impl.h"
#include "woutputviewport.h"
#include "woutputrenderwindow.h"
#include "wsgtextureprovider.h"
#include "wpointer.h"
#include "wtools.h"
#include "wayliblogging.h"

#include <wlr_all.h>

#include <QHash>
#include <cstdlib>
#include <time.h>
#include <drm_fourcc.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

// QObject is not standard-layout, so the native callbacks use a registry
// instead of container_of on the capture source object.
static QHash<wlr_ext_image_capture_source_v1 *, WExtImageCaptureSourceV1Impl *> s_captureSourceMap;

struct CaptureFrameEvent {
    wlr_ext_image_capture_source_v1_frame_event base;
    wlr_buffer *buffer;
    timespec when;
};

struct SourceResourceDestroyListener {
    wl_listener listener;
    WExtImageCaptureSourceV1Impl *self;
};

const wlr_ext_image_capture_source_v1_interface WExtImageCaptureSourceV1Impl::impl = {
    .start = WExtImageCaptureSourceV1Impl::start,
    .stop = WExtImageCaptureSourceV1Impl::stop,
    .request_frame = WExtImageCaptureSourceV1Impl::request_frame,
    .copy_frame = WExtImageCaptureSourceV1Impl::copy_frame,
    .get_pointer_cursor = WExtImageCaptureSourceV1Impl::get_pointer_cursor,
};

WExtImageCaptureSourceV1Impl::WExtImageCaptureSourceV1Impl(QObject *parent)
    : QObject(parent)
{
    wlr_ext_image_capture_source_v1_init(&source, &impl);
    s_captureSourceMap.insert(&source, this);
}

WExtImageCaptureSourceV1Impl::~WExtImageCaptureSourceV1Impl()
{
    m_inDestructor = true;
    invalidate();
    s_captureSourceMap.remove(&source);
}

void WExtImageCaptureSourceV1Impl::invalidate()
{
    if (m_finished)
        return;
    m_finished = true;
    // The client resources may outlive this object. Detach their listeners
    // before finish() so their later destruction cannot reach freed memory.
    detachResourceListeners();
    // Finishing the source stops every session and makes its resources inert.
    // Detach the viewport while the output and its render window still exist.
    wlr_ext_image_capture_source_v1_finish(&source);
    releaseViewport();
    // The source is now inert; the compositor no longer needs this wrapper.
    if (!m_inDestructor)
        deleteLater();
}

WOutputViewport *WExtImageCaptureSourceV1Impl::ensureViewport()
{
    if (m_viewport)
        return m_viewport.data();

    auto *viewport = createViewport();
    if (!viewport)
        return nullptr;
    Q_ASSERT(viewport->offscreen() && !viewport->hideSource());
    auto *renderWindow = viewport->outputRenderWindow();
    if (!viewport->output() || !renderWindow) {
        qCWarning(lcWlImageCapture) << "Capture viewport is not attached to an output render window";
        delete viewport;
        return nullptr;
    }
    viewport->setParent(this);
    viewport->setVisible(false);
    m_viewport = viewport;

    m_viewportConnections << connect(viewport, &WOutputViewport::frameRendered,
                                    this, &WExtImageCaptureSourceV1Impl::handleFrameRendered)
                          << connect(viewport, &WOutputViewport::bufferSizeChanged,
                                     this, &WExtImageCaptureSourceV1Impl::updateConstraints)
                          << connect(viewport->output(), &WOutput::beforeDestroy,
                                     this, &WExtImageCaptureSourceV1Impl::invalidate)
                          << connect(viewport->input(), &QObject::destroyed,
                                     this, &WExtImageCaptureSourceV1Impl::invalidate)
                          << connect(renderWindow, &QQuickWindow::sceneGraphInvalidated,
                                     this, &WExtImageCaptureSourceV1Impl::invalidate);
    // wlroots sends the session constraints right after start() returns, so
    // they must be valid before the viewport has rendered anything.
    updateConstraints();
    return viewport;
}

void WExtImageCaptureSourceV1Impl::releaseViewport()
{
    if (!m_viewport)
        return;

    for (const auto &connection : std::as_const(m_viewportConnections))
        disconnect(connection);
    m_viewportConnections.clear();

    m_viewport->setVisible(false);
    delete m_viewport.data();
    m_viewport = nullptr;
    // Keep width/height/shm_formats: the next session must report constraints
    // before its first render. Only the swapchain-derived formats are stale.
    m_constraintsFromSwapchain = false;
}

bool WExtImageCaptureSourceV1Impl::accept(
    wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request *request)
{
    if (m_finished)
        return false;
    // The request's new_id is private to wlroots, so the accept helper is the
    // only supported way to create the client resource.
    if (!wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request_accept(request, &source))
        return false;

    attachResourceListeners();
    return true;
}

void WExtImageCaptureSourceV1Impl::attachResourceListeners()
{
    struct wl_resource *resource;
    wl_resource_for_each(resource, &source.resources) {
        if (m_resourceListeners.contains(resource))
            continue;
        auto *entry = static_cast<SourceResourceDestroyListener *>(
            std::calloc(1, sizeof(SourceResourceDestroyListener)));
        if (!entry)
            continue;
        entry->self = this;
        entry->listener.notify = &WExtImageCaptureSourceV1Impl::resourceDestroyed;
        m_resourceListeners.insert(resource, entry);
        wl_resource_add_destroy_listener(resource, &entry->listener);
    }
}

void WExtImageCaptureSourceV1Impl::detachResourceListeners()
{
    for (auto *entry : std::as_const(m_resourceListeners)) {
        wl_list_remove(&entry->listener.link);
        std::free(entry);
    }
    m_resourceListeners.clear();
}

void WExtImageCaptureSourceV1Impl::resourceDestroyed(wl_listener *listener, void *data)
{
    SourceResourceDestroyListener *entry;
    entry = wl_container_of(listener, entry, listener);
    auto *self = entry->self;
    auto *resource = static_cast<wl_resource *>(data);
    self->m_resourceListeners.remove(resource);

    // wl_resource_destroy() notifies destroy listeners before wlroots unlinks
    // the resource from source.resources. Unlink it now so the recycle check
    // below sees the client's last resource as gone. wlroots' own destructor
    // then removes an already initialized link, which is a no-op.
    wl_list_remove(wl_resource_get_link(resource));
    wl_list_init(wl_resource_get_link(resource));
    std::free(entry);

    self->maybeRecycle();
}

void WExtImageCaptureSourceV1Impl::maybeRecycle()
{
    if (m_finished || m_recycleScheduled)
        return;
    // The client owns the source resource; its sessions own the pipeline.
    // Keep this object alive while either of them still exists.
    if (m_numStarted > 0 || !wl_list_empty(&source.resources))
        return;

    // stop() and resource destruction run inside wlroots' session teardown.
    // Finishing the source there would re-enter session_destroy() for the
    // session that is still on the stack, so recycle on the next event loop
    // iteration instead.
    m_recycleScheduled = true;
    QMetaObject::invokeMethod(this, &WExtImageCaptureSourceV1Impl::invalidate,
                              Qt::QueuedConnection);
}

void WExtImageCaptureSourceV1Impl::updateConstraints()
{
    if (m_finished || !m_viewport)
        return;

    const auto size = m_viewport->effectiveBufferSize();
    if (size.isEmpty())
        return;
    auto *swapchain = m_viewport->swapchain();
    const bool matchingSwapchain = swapchain && swapchain->width == size.width()
        && swapchain->height == size.height();
    const bool sizeChanged = source.width != uint32_t(size.width())
        || source.height != uint32_t(size.height());
    if (sizeChanged)
        m_constraintsFromSwapchain = false;
    if (!sizeChanged && (!matchingSwapchain || m_constraintsFromSwapchain)
        && source.shm_formats_len > 0)
        return;

    if (matchingSwapchain) {
        // Advertise the formats of the capture swapchain, never the screen's.
        if (wlr_ext_image_capture_source_v1_set_constraints_from_swapchain(
                &source, swapchain, m_viewport->output()->renderer())) {
            m_constraintsFromSwapchain = true;
            return;
        }
        qCWarning(lcWlImageCapture) << "Failed to update viewport capture constraints" << size;
    }

    source.width = size.width();
    source.height = size.height();
    if (source.shm_formats_len == 0) {
        // The first render is asynchronous. SHM is usable before DMA-BUF
        // constraints can be obtained from the viewport's own swapchain.
        source.shm_formats = static_cast<uint32_t *>(std::malloc(sizeof(uint32_t)));
        if (source.shm_formats) {
            source.shm_formats[0] = DRM_FORMAT_ARGB8888;
            source.shm_formats_len = 1;
        }
    }
    wl_signal_emit_mutable(&source.events.constraints_update, nullptr);
}

void WExtImageCaptureSourceV1Impl::start(wlr_ext_image_capture_source_v1 *source, bool with_cursors)
{
    auto *self = s_captureSourceMap.value(source);
    Q_ASSERT(self);
    self->start(with_cursors);
}

void WExtImageCaptureSourceV1Impl::start([[maybe_unused]] bool with_cursors)
{
    if (m_finished)
        return;
    ++m_numStarted;
    if (m_numStarted > 1)
        return;

    if (!ensureViewport()) {
        // wlroots has not attached the session's destroy listener yet, so the
        // failure must be reported once session creation has finished.
        qCWarning(lcWlImageCapture) << "No capture viewport available for a new session";
        QMetaObject::invokeMethod(this, &WExtImageCaptureSourceV1Impl::invalidate,
                                  Qt::QueuedConnection);
        return;
    }
    m_viewport->setVisible(true);
    m_viewport->outputRenderWindow()->update(m_viewport);
}

void WExtImageCaptureSourceV1Impl::stop(wlr_ext_image_capture_source_v1 *source)
{
    auto *self = s_captureSourceMap.value(source);
    Q_ASSERT(self);
    self->stop();
}

void WExtImageCaptureSourceV1Impl::stop()
{
    Q_ASSERT(m_numStarted > 0);
    if (--m_numStarted > 0 || m_finished)
        return;

    // No session uses the pipeline anymore: the client (not the toplevel) owns
    // it, so release everything until a new session asks for a viewport.
    releaseViewport();
    maybeRecycle();
}

void WExtImageCaptureSourceV1Impl::request_frame(wlr_ext_image_capture_source_v1 *source, bool schedule_frame)
{
    auto *self = s_captureSourceMap.value(source);
    Q_ASSERT(self);
    self->schedule_frame(schedule_frame);
}

void WExtImageCaptureSourceV1Impl::schedule_frame([[maybe_unused]] bool schedule_frame)
{
    if (m_finished || m_numStarted == 0 || !m_viewport)
        return;
    // wlroots passes false when the last source frame has not been consumed by
    // a ready frame yet. That hint fits sources which render on every output
    // frame; this viewport only renders on demand, so honoring it would leave
    // the next request waiting for a render that never happens.
    m_viewport->outputRenderWindow()->update(m_viewport);
}

void WExtImageCaptureSourceV1Impl::handleFrameRendered()
{
    if (m_finished || m_numStarted == 0 || !m_viewport)
        return;

    auto *provider = m_viewport->wTextureProvider();
    auto *buffer = provider ? provider->wlrBuffer() : nullptr;
    if (!buffer)
        return;
    WBufferUnlockPtr bufferGuard(wlr_buffer_lock(buffer));
    updateConstraints();

    WPixmanRegion damage(0, 0, buffer->width, buffer->height);
    CaptureFrameEvent event {
        .base = { .damage = damage.get() },
        .buffer = buffer,
        .when = {},
    };
    clock_gettime(CLOCK_MONOTONIC, &event.when);
    wl_signal_emit_mutable(&source.events.frame, &event.base);
}

void WExtImageCaptureSourceV1Impl::copy_frame(wlr_ext_image_capture_source_v1 *source,
                                             wlr_ext_image_copy_capture_frame_v1 *dst_frame,
                                             wlr_ext_image_capture_source_v1_frame_event *frame_event)
{
    auto *self = s_captureSourceMap.value(source);
    Q_ASSERT(self);
    self->copy_frame(dst_frame, frame_event);
}

void WExtImageCaptureSourceV1Impl::copy_frame(wlr_ext_image_copy_capture_frame_v1 *dst_frame,
                                             wlr_ext_image_capture_source_v1_frame_event *frame_event)
{
    if (m_finished || m_numStarted == 0 || !m_viewport) {
        wlr_ext_image_copy_capture_frame_v1_fail(dst_frame, EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_STOPPED);
        return;
    }

    // base is the first member; the event's buffer is locked throughout the
    // synchronous frame signal, including all sessions copying this frame.
    auto *event = reinterpret_cast<CaptureFrameEvent *>(frame_event);
    if (wlr_ext_image_copy_capture_frame_v1_copy_buffer(
            dst_frame, event->buffer, m_viewport->output()->renderer())) {
        wlr_ext_image_copy_capture_frame_v1_ready(dst_frame, WL_OUTPUT_TRANSFORM_NORMAL, &event->when);
    }
    // copy_buffer already sends failed and destroys the native frame on error.
    // Sending another failure here would use a freed frame (notably on resize).
}

wlr_ext_image_capture_source_v1_cursor *WExtImageCaptureSourceV1Impl::get_pointer_cursor(
    [[maybe_unused]] wlr_ext_image_capture_source_v1 *source, [[maybe_unused]] wlr_seat *seat)
{
    // Cursor capture is not implemented for toplevel sources yet.
    return nullptr;
}

WAYLIB_SERVER_END_NAMESPACE
