// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wextimagecapturesourcev1impl.h"
#include "wsurfaceitem.h"
#include "wsgtextureprovider.h"
#include "woutputrenderwindow.h"
#include "woutput.h"
#include "wtools.h"
#include "wayliblogging.h"

#include <qwextimagecopycapturev1.h>
#include <qwrenderer.h>
#include <qwbuffer.h>
#include <qwswapchain.h>
#include <qwallocator.h>
#include <qwcompositor.h>
#include <qwtexture.h>

#include <memory>

extern "C" {
#include <wlr/interfaces/wlr_output.h>
#include <wlr/types/wlr_buffer.h>
#include <pixman.h>
#include <drm_fourcc.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
}

QW_USE_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

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
            struct wlr_buffer *buffer = swapchain->acquire();
            if (buffer) {
                auto *qbuffer = qw_buffer::from(buffer);
                qw_texture *texture = qw_texture::from_buffer(*renderer, *qbuffer);
                qbuffer->unlock();
                
                if (texture) {
                    uint32_t shm_format = texture->preferred_read_format();
                    delete texture;
                    if (shm_format != DRM_FORMAT_INVALID) {
                        format = shm_format;
                    }
                }
            }
        }
        
        // Use unique_ptr for safer memory management
        auto formats = std::unique_ptr<uint32_t[]>(new uint32_t[1]);
        formats[0] = format;
        
        free(source->shm_formats);
        source->shm_formats = formats.release();
        source->shm_formats_len = 1;
        
        qCDebug(lcWlImageCapture) << "Set SHM format:" << format;
    }
    
    void buildDmabufFormats() {
        auto renderer = output->renderer();
        auto swapchain = output->swapchain();
        
        if (!renderer || !swapchain) return;
        
        int drm_fd = renderer->get_drm_fd();
        if (swapchain->handle()->allocator && 
            (swapchain->handle()->allocator->buffer_caps & WLR_BUFFER_CAP_DMABUF) && 
            drm_fd >= 0) {
            
            struct stat dev_stat;
            if (fstat(drm_fd, &dev_stat) == 0) {
                source->dmabuf_device = dev_stat.st_rdev;
                
                // Clean up old DMA-BUF formats
                wlr_drm_format_set_finish(&source->dmabuf_formats);
                source->dmabuf_formats = (struct wlr_drm_format_set){};
                
                // Copy DMA-BUF formats from swapchain
                for (size_t i = 0; i < swapchain->handle()->format.len; i++) {
                    wlr_drm_format_set_add(&source->dmabuf_formats,
                        swapchain->handle()->format.format, swapchain->handle()->format.modifiers[i]);
                }
                qCDebug(lcWlImageCapture) << "Set DMA-BUF constraints";
            }
        }
    }
    
    void apply() {
        wl_signal_emit_mutable(&source->events.constraints_update, nullptr);
    }
};

WExtImageCaptureSourceV1Impl::WExtImageCaptureSourceV1Impl(WSurfaceItemContent *surfaceContent, WOutput *output)
    : QObject(surfaceContent) // TODO: Check if Qt object tree destruction timing is appropriate
    , m_surfaceContent(surfaceContent)
    , m_output(output)
    , m_capturing(false)
    , m_renderEndConnection()
{
    Q_ASSERT(m_surfaceContent);

    // Initialize wlr_ext_image_capture_source_v1
    wlr_ext_image_capture_source_v1_init(handle(), impl());
    
    // Get actual surface size and set constraints directly
    auto surface = m_surfaceContent->surface();
    if (surface && surface->handle()) {
        auto wlr_surface = surface->handle()->handle();
        int width = wlr_surface->current.width;
        int height = wlr_surface->current.height;
        
        // Validate dimensions before setting constraints
        if (width > 0 && height > 0) {
            // Use constraint builder helper directly
            ConstraintBuilder builder(handle(), m_output);
            builder.setSize(width, height);
            builder.buildShmFormats();
            builder.buildDmabufFormats();
            builder.apply();
            
            qCDebug(lcWlImageCapture) << "Initial constraints set successfully:";
            qCDebug(lcWlImageCapture) << "  - Width:" << width;
            qCDebug(lcWlImageCapture) << "  - Height:" << height;
        } else {
            qCWarning(lcWlImageCapture) << "Invalid surface dimensions for constraints:" << width << "x" << height;
        }
    } else {
        qCWarning(lcWlImageCapture) << "No valid surface available for setting initial constraints";
    }
}

WExtImageCaptureSourceV1Impl::~WExtImageCaptureSourceV1Impl()
{
    if (m_capturing) {
        qCDebug(lcWlImageCapture) << "WExtImageCaptureSourceV1Impl destroyed while capturing";
    }
}

void WExtImageCaptureSourceV1Impl::start([[maybe_unused]] bool with_cursors)
{
    // TODO: Implement cursor capture if needed
    m_capturing = true;
    qCDebug(lcWlImageCapture) << "WExtImageCaptureSourceV1Impl::start() with_cursors:" << with_cursors;

    // TODO: Optimize multiple clients capturing the same window
    // Currently each client creates its own WExtImageCaptureSourceV1Impl instance,
    // which means multiple render listeners for the same surface. Consider implementing
    // a manager to share render events among multiple capture sources.

    if (!m_surfaceContent) {
        qCWarning(lcWlImageCapture) << "No surface content available for capture";
        return;
    }
    
    // Get render window
    auto textureProvider = m_surfaceContent->wTextureProvider();
    if (!textureProvider) {
        qCWarning(lcWlImageCapture) << "No texture provider available for start";
        return;
    }
    
    auto renderWindow = textureProvider->window();
    if (!renderWindow) {
        qCWarning(lcWlImageCapture) << "No render window available for start";
        return;
    }
    
    // Connect to renderEnd signal
    m_renderEndConnection = connect(renderWindow,
                                   &WOutputRenderWindow::renderEnd,
                                   this,
                                   &WExtImageCaptureSourceV1Impl::handleRenderEnd,
                                   Qt::AutoConnection);
    
    if (!m_renderEndConnection) {
        qCWarning(lcWlImageCapture) << "Cannot connect to render end of output render window";
    }
    
    // If not currently rendering, trigger immediately
    if (!renderWindow->inRendering()) {
        QMetaObject::invokeMethod(this, &WExtImageCaptureSourceV1Impl::handleRenderEnd, Qt::AutoConnection);
    }
}

void WExtImageCaptureSourceV1Impl::stop()
{
    m_capturing = false;
    qCDebug(lcWlImageCapture) << "WExtImageCaptureSourceV1Impl::stop()";
    
    // Disconnect render end connection
    if (m_renderEndConnection) {
        disconnect(m_renderEndConnection);
        m_renderEndConnection = QMetaObject::Connection();
    }
}

void WExtImageCaptureSourceV1Impl::schedule_frame()
{
    qCDebug(lcWlImageCapture) << "WExtImageCaptureSourceV1Impl::schedule_frame()";
    
    if (!m_capturing) {
        qCWarning(lcWlImageCapture) << "schedule_frame called but not capturing";
        return;
    }
    
    if (!m_surfaceContent) {
        qCWarning(lcWlImageCapture) << "No surface content available for frame scheduling";
        return;
    }
    
    // Request output update to ensure next frame will be rendered
    wlr_output_update_needs_frame(m_output->nativeHandle());
    
    // Get render window to check if currently rendering
    auto textureProvider = m_surfaceContent->wTextureProvider();
    if (textureProvider) {
        auto renderWindow = textureProvider->window();
        if (renderWindow && !renderWindow->inRendering()) {
            QMetaObject::invokeMethod(this, &WExtImageCaptureSourceV1Impl::handleRenderEnd, Qt::AutoConnection);
        }
    }
    
    qCDebug(lcWlImageCapture) << "Scheduled frame capture";
}

void WExtImageCaptureSourceV1Impl::handleRenderEnd()
{
    qCDebug(lcWlImageCapture) << "WExtImageCaptureSourceV1Impl::handleRenderEnd() - triggering frame event";
    
    if (!m_capturing) {
        qCWarning(lcWlImageCapture) << "handleRenderEnd called but not capturing";
        return;
    }
    
    // Get surface size and validate it
    QSize surfaceSize = m_surfaceContent->size().toSize();
    if (surfaceSize.width() <= 0 || surfaceSize.height() <= 0) {
        qCWarning(lcWlImageCapture) << "Invalid surface size for damage region:" << surfaceSize;
        return;
    }
    
    // Create damage region with RAII
    WPixmanRegion fullDamage(0, 0, surfaceSize.width(), surfaceSize.height());
    
    // Create frame event and Q_EMIT
    wlr_ext_image_capture_source_v1_frame_event event {
        .damage = fullDamage.get(),
    };
    wl_signal_emit_mutable(&handle()->events.frame, &event);
    
    qCDebug(lcWlImageCapture) << "Frame event emitted with damage region:" << surfaceSize;
}

void WExtImageCaptureSourceV1Impl::copy_frame(wlr_ext_image_copy_capture_frame_v1 *dst_frame, 
                                              [[maybe_unused]] wlr_ext_image_capture_source_v1_frame_event *frame_event)
{
    qCDebug(lcWlImageCapture) << "WExtImageCaptureSourceV1Impl::copy_frame()";
    
    if (!m_capturing) {
        qCWarning(lcWlImageCapture) << "copy_frame called but not capturing";
        qw_ext_image_copy_capture_frame_v1::from(dst_frame)->fail(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_STOPPED);
        return;
    }
    
    if (!m_surfaceContent) {
        qCWarning(lcWlImageCapture) << "No surface content available for frame copy";
        qw_ext_image_copy_capture_frame_v1::from(dst_frame)->fail(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_UNKNOWN);
        return;
    }
    
    // Get texture provider
    auto textureProvider = m_surfaceContent->wTextureProvider();
    if (!textureProvider) {
        qCWarning(lcWlImageCapture) << "No texture provider available";
        qw_ext_image_copy_capture_frame_v1::from(dst_frame)->fail(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_UNKNOWN);
        return;
    }

    auto buffer = textureProvider->qwBuffer();
    if (!buffer || !buffer->handle()) {
        qCWarning(lcWlImageCapture) << "No internal buffer available";
        qw_ext_image_copy_capture_frame_v1::from(dst_frame)->fail(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_UNKNOWN);
        return;
    }

    // Lock the buffer for the duration of the copy to prevent races during resize
    if (!buffer->lock()) {
        qCWarning(lcWlImageCapture) << "Failed to lock internal buffer";
        qw_ext_image_copy_capture_frame_v1::from(dst_frame)->fail(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_UNKNOWN);
        return;
    }
    std::unique_ptr<qw_buffer, qw_buffer::unlocker> bufferGuard(buffer);
    
    // Get renderer
    auto renderWindow = textureProvider->window();
    if (!renderWindow) {
        qCWarning(lcWlImageCapture) << "No render window available";
        qw_ext_image_copy_capture_frame_v1::from(dst_frame)->fail(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_UNKNOWN);
        return;
    }
    
    auto renderer = m_output->renderer();
    if (!renderer) {
        qCWarning(lcWlImageCapture) << "No renderer available";
        qw_ext_image_copy_capture_frame_v1::from(dst_frame)->fail(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_UNKNOWN);
        return;
    }
    
    // Prefer the client buffer source if present
    wlr_buffer *src = buffer->handle();
    if (auto clientBuf = wlr_client_buffer_get(*buffer)) {
        src = clientBuf->source;
    }

    // Critical safety checks: validate all required pointers and buffers before copying
    if (!src) {
        qCWarning(lcWlImageCapture) << "Source buffer is null, cannot copy frame";
        if (dst_frame) {
            qw_ext_image_copy_capture_frame_v1::from(dst_frame)->fail(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_BUFFER_CONSTRAINTS);
        }
        return;
    }

    if (!dst_frame || !dst_frame->buffer) {
        qCWarning(lcWlImageCapture) << "Destination frame or buffer is null, cannot copy";
        if (dst_frame) {
            qw_ext_image_copy_capture_frame_v1::from(dst_frame)->fail(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_BUFFER_CONSTRAINTS);
        }
        return;
    }

    // Validate buffer dimensions to prevent crashes during resize
    if (dst_frame->buffer->width != src->width || dst_frame->buffer->height != src->height) {
        qCWarning(lcWlImageCapture) << "Buffer size mismatch during resize (dst:" << dst_frame->buffer->width << "x" << dst_frame->buffer->height
                                   << ", src:" << src->width << "x" << src->height << "), updating constraints";
        
        // Update constraints when we detect a size mismatch
        ConstraintBuilder builder(handle(), m_output);
        builder.setSize(src->width, src->height);
        builder.buildShmFormats();
        builder.buildDmabufFormats();
        builder.apply();
        
        qCDebug(lcWlImageCapture) << "Constraints updated to new size:" << src->width << "x" << src->height;
        
        // Check again after constraints update - the client might have already provided a correctly sized buffer
        if (dst_frame->buffer->width != src->width || dst_frame->buffer->height != src->height) {
            qCDebug(lcWlImageCapture) << "Buffer size still mismatched after constraint update, skipping frame";
            qw_ext_image_copy_capture_frame_v1::from(dst_frame)->fail(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_BUFFER_CONSTRAINTS);
            return;
        }
        
        qCDebug(lcWlImageCapture) << "Buffer size now matches after constraint update, proceeding with copy";
    }

    // Use wlroots image copy function with validated buffers
    bool success = qw_ext_image_copy_capture_frame_v1::copy_buffer(dst_frame, src, renderer->handle());
    qCDebug(lcWlImageCapture) << "Copy result:" << success;
    
    if (success) {
        // Successfully copied, mark frame as ready
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        
        qw_ext_image_copy_capture_frame_v1::from(dst_frame)->ready(WL_OUTPUT_TRANSFORM_NORMAL, &now);
        qCDebug(lcWlImageCapture) << "Frame copy successful";
    } else {
        qCWarning(lcWlImageCapture) << "Failed to copy frame buffer";
        qCWarning(lcWlImageCapture) << "Possible reasons:";
        qCWarning(lcWlImageCapture) << "  - Buffer size mismatch";
        qCWarning(lcWlImageCapture) << "  - Unsupported buffer format";
        qCWarning(lcWlImageCapture) << "  - Renderer issues";
        qCWarning(lcWlImageCapture) << "  - Memory access problems";
        
        // Check if it's a buffer constraints issue
        if (dst_frame->buffer && buffer->handle()) {
            if (dst_frame->buffer->width != buffer->handle()->width ||
                dst_frame->buffer->height != buffer->handle()->height) {
                qCWarning(lcWlImageCapture) << "Buffer size mismatch detected, using BUFFER_CONSTRAINTS failure reason";
                qw_ext_image_copy_capture_frame_v1::from(dst_frame)->fail(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_BUFFER_CONSTRAINTS);
                return;
            }
        }
        
        // For other failures, use UNKNOWN reason
        qw_ext_image_copy_capture_frame_v1::from(dst_frame)->fail(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_UNKNOWN);
    }
}

wlr_ext_image_capture_source_v1_cursor *WExtImageCaptureSourceV1Impl::get_pointer_cursor([[maybe_unused]] wlr_seat *seat)
{
    qCDebug(lcWlImageCapture) << "WExtImageCaptureSourceV1Impl::get_pointer_cursor()";
    // TODO: Implement cursor retrieval logic
    // This needs to get cursor information from seat and create corresponding cursor structure
    // Currently return nullptr to indicate no cursor information
    return nullptr;
}

WAYLIB_SERVER_END_NAMESPACE
