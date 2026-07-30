// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wextimagecapturesourcev1impl.h"
#include <wpointer.h>
#include "wsurfaceitem.h"
#include "wsgtextureprovider.h"
#include "woutputrenderwindow.h"
#include "woutput.h"
#include "wtools.h"
#include "wayliblogging.h"
#include "wbufferrenderer_p.h"

#include <wlr_all.h>
#include <wcontainerof.h>

extern "C" {
#include <pixman.h>
#include <drm_fourcc.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
}

WAYLIB_SERVER_BEGIN_NAMESPACE

// The wlr_ext_image_capture_source_v1 has no data field and
// WExtImageCaptureSourceV1Impl is not standard-layout (QObject base), so a
// container_of lookup is not possible; use a registry instead.
static QHash<wlr_ext_image_capture_source_v1 *, WExtImageCaptureSourceV1Impl *> s_captureSourceMap;

// Helper for constraint building
struct ConstraintBuilder {
    wlr_ext_image_capture_source_v1 *source;
    WOutput *output;

    ConstraintBuilder(wlr_ext_image_capture_source_v1 *src, WOutput *out)
        : source(src), output(out) {}

    void setSize(int width, int height) {
        source->width = width;
        source->height = height;
    }

    void buildShmFormats() {
        auto renderer = output->renderer();
        auto swapchain = output->swapchain();
        uint32_t format = DRM_FORMAT_ARGB8888; // fallback

        if (renderer && swapchain) {
            if (struct wlr_buffer *buffer = wlr_swapchain_acquire(swapchain)) {
                WBufferUnlockPtr bufferGuard(buffer);
                WUniquePointer<wlr_texture> texture(
                    wlr_texture_from_buffer(renderer, buffer));

                if (texture) {
                    uint32_t shm_format = wlr_texture_preferred_read_format(texture.get());
                    if (shm_format != DRM_FORMAT_INVALID) {
                        format = shm_format;
                    }
                }
            }
        }

        // wlroots frees shm_formats with free() (ext_image_capture_source_v1),
        // so the allocation must use the C allocator — new[]/delete[] would be
        // an alloc/dealloc mismatch (UB, ASan alloc-dealloc-mismatch).
        free(source->shm_formats);
        source->shm_formats = static_cast<uint32_t*>(malloc(sizeof(uint32_t)));
        if (source->shm_formats) {
            source->shm_formats[0] = format;
            source->shm_formats_len = 1;
        } else {
            source->shm_formats_len = 0;
        }

        qCDebug(lcWlImageCapture) << "Set SHM format:" << format;
    }

    void buildDmabufFormats() {
        auto renderer = output->renderer();
        auto swapchain = output->swapchain();

        if (!renderer || !swapchain) return;

        int drm_fd = wlr_renderer_get_drm_fd(renderer);
        if (swapchain->allocator &&
            (swapchain->allocator->buffer_caps & WLR_BUFFER_CAP_DMABUF) &&
            drm_fd >= 0) {

            struct stat dev_stat;
            if (fstat(drm_fd, &dev_stat) == 0) {
                source->dmabuf_device = dev_stat.st_rdev;

                // Clean up old DMA-BUF formats
                wlr_drm_format_set_finish(&source->dmabuf_formats);
                source->dmabuf_formats = (struct wlr_drm_format_set){};

                // Copy DMA-BUF formats from swapchain
                for (size_t i = 0; i < swapchain->format.len; i++) {
                    wlr_drm_format_set_add(&source->dmabuf_formats,
                        swapchain->format.format, swapchain->format.modifiers[i]);
                }
                qCDebug(lcWlImageCapture) << "Set DMA-BUF constraints";
            }
        }
    }

    void apply() {
        wl_signal_emit_mutable(&source->events.constraints_update, nullptr);
    }
};

const struct wlr_ext_image_capture_source_v1_interface WExtImageCaptureSourceV1Impl::impl = {
    .start = WExtImageCaptureSourceV1Impl::start,
    .stop = WExtImageCaptureSourceV1Impl::stop,
    .schedule_frame = WExtImageCaptureSourceV1Impl::schedule_frame,
    .copy_frame = WExtImageCaptureSourceV1Impl::copy_frame,
    .get_pointer_cursor = WExtImageCaptureSourceV1Impl::get_pointer_cursor,
};

WExtImageCaptureSourceV1Impl::WExtImageCaptureSourceV1Impl(QQuickItem *surfaceItem, WOutput *output)
    : QObject(surfaceItem)
    , m_surfaceItem(surfaceItem)
    , m_output(output)
    , m_capturing(false)
{
    Q_ASSERT(m_surfaceItem);
    Q_ASSERT(m_output);

    // Initialize wlr_ext_image_capture_source_v1
    wlr_ext_image_capture_source_v1_init(&source, &impl);
    s_captureSourceMap.insert(&source, this);

    // Set initial constraints from the surfaceItem's bounding rect * dpr
    const auto pixelSize = computePixelSize();
    if (pixelSize.isValid() && !pixelSize.isEmpty()) {
        ConstraintBuilder builder(&source, m_output);
        builder.setSize(pixelSize.width(), pixelSize.height());
        builder.buildShmFormats();
        builder.buildDmabufFormats();
        builder.apply();

        qCDebug(lcWlImageCapture) << "Initial constraints set:" << pixelSize;
    } else {
        qCWarning(lcWlImageCapture) << "Invalid surface dimensions for constraints:" << pixelSize;
    }
}

WExtImageCaptureSourceV1Impl::~WExtImageCaptureSourceV1Impl()
{
    if (m_capturing) {
        stop();
    }
    // WBufferRenderer has no source items or proxy — just delete it.
    delete m_captureRenderer;

    wlr_ext_image_capture_source_v1_finish(&source);
    s_captureSourceMap.remove(&source);
}

WOutputRenderWindow *WExtImageCaptureSourceV1Impl::renderWindow() const
{
    if (!m_surfaceItem)
        return nullptr;
    return qobject_cast<WOutputRenderWindow *>(m_surfaceItem->window());
}

qreal WExtImageCaptureSourceV1Impl::computeDpr() const
{
    auto rw = renderWindow();
    return rw ? rw->effectiveDevicePixelRatio() : 1.0;
}

QSize WExtImageCaptureSourceV1Impl::computePixelSize() const
{
    if (!m_surfaceItem)
        return { };

    const auto sz = m_surfaceItem->size();
    if (sz.isEmpty())
        return { };

    const qreal dpr = computeDpr();
    return QSize(qCeil(sz.width() * dpr), qCeil(sz.height() * dpr));
}

void WExtImageCaptureSourceV1Impl::updateConstraints(const QSize &pixelSize)
{
    if (!pixelSize.isValid() || pixelSize.isEmpty())
        return;

    ConstraintBuilder builder(handle(), m_output);
    builder.setSize(pixelSize.width(), pixelSize.height());
    builder.buildShmFormats();
    builder.buildDmabufFormats();
    builder.apply();
}

void WExtImageCaptureSourceV1Impl::start(struct wlr_ext_image_capture_source_v1 *source, bool with_cursors)
{
    auto *self = s_captureSourceMap.value(source);
    Q_ASSERT(self);
    self->start(with_cursors);
}

void WExtImageCaptureSourceV1Impl::start(bool with_cursors)
{
    m_capturing = true;
    qCDebug(lcWlImageCapture) << "WExtImageCaptureSourceV1Impl::start() with_cursors:" << with_cursors;

    auto rw = renderWindow();
    if (!rw) {
        qCWarning(lcWlImageCapture) << "No render window available for start";
        return;
    }

    if (!m_captureRenderer) {
        m_captureRenderer = new WBufferRenderer(rw->contentItem());
        m_captureRenderer->setOutput(m_output);
        m_captureRenderer->setVisible(false);
    }

    m_afterRenderingConnection = connect(rw,
                                         &QQuickWindow::afterRendering,
                                         this,
                                         &WExtImageCaptureSourceV1Impl::doOffscreenRender,
                                         Qt::AutoConnection);

    m_renderEndConnection = connect(rw,
                                    &WOutputRenderWindow::renderEnd,
                                    this,
                                    &WExtImageCaptureSourceV1Impl::handleRenderEnd,
                                    Qt::AutoConnection);

    // Trigger first frame
    wlr_output_update_needs_frame(m_output->handle());
}

void WExtImageCaptureSourceV1Impl::stop(struct wlr_ext_image_capture_source_v1 *source)
{
    auto *self = s_captureSourceMap.value(source);
    Q_ASSERT(self);
    self->stop();
}

void WExtImageCaptureSourceV1Impl::stop()
{
    m_capturing = false;
    qCDebug(lcWlImageCapture) << "WExtImageCaptureSourceV1Impl::stop()";

    if (m_afterRenderingConnection) {
        disconnect(m_afterRenderingConnection);
        m_afterRenderingConnection = QMetaObject::Connection();
    }
    if (m_renderEndConnection) {
        disconnect(m_renderEndConnection);
        m_renderEndConnection = QMetaObject::Connection();
    }

    if (m_renderedBuffer) {
        wlr_buffer_unlock(m_renderedBuffer);
    }
    m_renderedBuffer = nullptr;
}

void WExtImageCaptureSourceV1Impl::schedule_frame(struct wlr_ext_image_capture_source_v1 *source)
{
    auto *self = s_captureSourceMap.value(source);
    Q_ASSERT(self);
    self->schedule_frame();
}

void WExtImageCaptureSourceV1Impl::schedule_frame()
{
    qCDebug(lcWlImageCapture) << "WExtImageCaptureSourceV1Impl::schedule_frame()";

    if (!m_capturing) {
        qCWarning(lcWlImageCapture) << "schedule_frame called but not capturing";
        return;
    }

    // Request output update to ensure next frame will be rendered.
    // doOffscreenRender fires via afterRendering, handleRenderEnd via renderEnd.
    wlr_output_update_needs_frame(m_output->handle());
}

void WExtImageCaptureSourceV1Impl::doOffscreenRender()
{
    if (!m_capturing || !m_surfaceItem || !m_captureRenderer)
        return;

    auto rw = renderWindow();
    if (!rw)
        return;

    const auto pixelSize = computePixelSize();
    if (pixelSize.isEmpty()) {
        qCWarning(lcWlImageCapture) << "Invalid pixel size for offscreen render:" << pixelSize;
        return;
    }

    const qreal dpr = computeDpr();

    if (m_renderedBuffer) {
        wlr_buffer_unlock(m_renderedBuffer);
        m_renderedBuffer = nullptr;
    }

    // Render the surface item's subtree (content + subsurfaces) into an
    // offscreen FBO via transient root-node re-parenting.  No layer.enabled
    // on m_surfaceItem — avoids the double-resampling blur.
    m_renderedBuffer = rw->renderItemToBuffer(m_captureRenderer,
                                              m_surfaceItem,
                                              pixelSize,
                                              dpr,
                                              DRM_FORMAT_ARGB8888);

    if (m_renderedBuffer) {
        // Lock to prevent swapchain from recycling before copy_frame
        wlr_buffer_lock(m_renderedBuffer);
    }

    qCDebug(lcWlImageCapture) << "Offscreen render done, buffer:"
                              << (m_renderedBuffer ? m_renderedBuffer.get() : nullptr)
                              << "size:" << pixelSize;
}

void WExtImageCaptureSourceV1Impl::handleRenderEnd()
{
    if (!m_capturing) {
        qCWarning(lcWlImageCapture) << "handleRenderEnd called but not capturing";
        return;
    }

    if (!m_renderedBuffer) {
        qCWarning(lcWlImageCapture) << "No rendered buffer available for frame event";
        return;
    }

    auto wlr_buf = m_renderedBuffer.get();
    const int bufferWidth = wlr_buf->width;
    const int bufferHeight = wlr_buf->height;
    if (bufferWidth <= 0 || bufferHeight <= 0) {
        qCWarning(lcWlImageCapture) << "Invalid buffer size:" << bufferWidth << "x" << bufferHeight;
        return;
    }

    // TODO: partial damage
    WPixmanRegion fullDamage(0, 0, bufferWidth, bufferHeight);

    wlr_ext_image_capture_source_v1_frame_event event {
        .damage = fullDamage.get(),
    };
    wl_signal_emit_mutable(&source.events.frame, &event);

    qCDebug(lcWlImageCapture) << "Frame event emitted with damage:" << bufferWidth << "x"
                              << bufferHeight;
}

void WExtImageCaptureSourceV1Impl::copy_frame(struct wlr_ext_image_capture_source_v1 *source,
                                              wlr_ext_image_copy_capture_frame_v1 *dst_frame,
                                              wlr_ext_image_capture_source_v1_frame_event *frame_event)
{
    auto *self = s_captureSourceMap.value(source);
    Q_ASSERT(self);
    self->copy_frame(dst_frame, frame_event);
}

void WExtImageCaptureSourceV1Impl::copy_frame(
    wlr_ext_image_copy_capture_frame_v1 *dst_frame,
    [[maybe_unused]] wlr_ext_image_capture_source_v1_frame_event *frame_event)
{
    qCDebug(lcWlImageCapture) << "WExtImageCaptureSourceV1Impl::copy_frame()";

    if (!m_capturing) {
        qCWarning(lcWlImageCapture) << "copy_frame called but not capturing";
        wlr_ext_image_copy_capture_frame_v1_fail(dst_frame, EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_STOPPED);
        return;
    }

    if (!m_renderedBuffer) {
        qCWarning(lcWlImageCapture) << "No rendered buffer available for copy";
        wlr_ext_image_copy_capture_frame_v1_fail(dst_frame, EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_UNKNOWN);
        return;
    }

    auto renderer = m_output->renderer();
    if (!renderer) {
        qCWarning(lcWlImageCapture) << "No renderer available";
        wlr_ext_image_copy_capture_frame_v1_fail(dst_frame, EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_UNKNOWN);
        return;
    }
    auto src = m_renderedBuffer.get();
    // Buffer is already locked in doOffscreenRender; unlock after copy.
    // copy_buffer calls fail() + frame_destroy + free internally on failure,
    // so do NOT touch dst_frame afterwards.

    if (!dst_frame || !dst_frame->buffer) {
        qCWarning(lcWlImageCapture) << "Destination frame or buffer is null";
        if (dst_frame) {
            wlr_ext_image_copy_capture_frame_v1_fail(dst_frame, EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_BUFFER_CONSTRAINTS);
        }
        wlr_buffer_unlock(src);
        m_renderedBuffer = nullptr;
        return;
    }

    // Validate buffer dimensions
    if (dst_frame->buffer->width != src->width || dst_frame->buffer->height != src->height) {
        qCWarning(lcWlImageCapture) << "Buffer size mismatch (dst:" << dst_frame->buffer->width
                                    << "x" << dst_frame->buffer->height << ", src:" << src->width
                                    << "x" << src->height << "), updating constraints";

        updateConstraints(QSize(src->width, src->height));

        if (dst_frame->buffer->width != src->width || dst_frame->buffer->height != src->height) {
            qCDebug(lcWlImageCapture) << "Buffer size still mismatched after constraint update";
            wlr_ext_image_copy_capture_frame_v1_fail(dst_frame, EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_BUFFER_CONSTRAINTS);
            wlr_buffer_unlock(src);
            m_renderedBuffer = nullptr;
            return;
        }
    }

    bool success = wlr_ext_image_copy_capture_frame_v1_copy_buffer(dst_frame, src, renderer);
    qCDebug(lcWlImageCapture) << "Copy result:" << success;

    // Unlock regardless of success/failure.
    wlr_buffer_unlock(src);
    m_renderedBuffer = nullptr;

    if (success) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        wlr_ext_image_copy_capture_frame_v1_ready(dst_frame, WL_OUTPUT_TRANSFORM_NORMAL, &now);
        qCDebug(lcWlImageCapture) << "Frame copy successful";
    } else {
        qCWarning(lcWlImageCapture) << "Failed to copy frame buffer";
    }
}

wlr_ext_image_capture_source_v1_cursor *WExtImageCaptureSourceV1Impl::get_pointer_cursor(
    [[maybe_unused]] struct wlr_ext_image_capture_source_v1 *source, [[maybe_unused]] struct wlr_seat *seat)
{
    auto *self = s_captureSourceMap.value(source);
    Q_ASSERT(self);
    return self->get_pointer_cursor(seat);
}

wlr_ext_image_capture_source_v1_cursor *WExtImageCaptureSourceV1Impl::get_pointer_cursor([[maybe_unused]] wlr_seat *seat)
{
    qCDebug(lcWlImageCapture) << "WExtImageCaptureSourceV1Impl::get_pointer_cursor()";
    return nullptr;
}

WAYLIB_SERVER_END_NAMESPACE
