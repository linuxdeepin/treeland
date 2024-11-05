// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "capture.h"

#include <private/qguiapplication_p.h>
#include <private/qwaylanddisplay_p.h>
#include <private/qwaylandintegration_p.h>

#include <QPointer>

inline QtWaylandClient::QWaylandIntegration *waylandIntegration()
{
    return dynamic_cast<QtWaylandClient::QWaylandIntegration *>(
        QGuiApplicationPrivate::platformIntegration());
}

inline QPointer<QtWaylandClient::QWaylandDisplay> waylandDisplay()
{
    return waylandIntegration()->display();
}

void destruct_treeland_capture_manager(TreelandCaptureManager *manager)
{
    qDeleteAll(manager->captureContexts);
    manager->captureContexts.clear();
}

QPointer<TreelandCaptureContext> TreelandCaptureManager::getContext()
{
    auto context = get_context();
    auto captureContext = new TreelandCaptureContext(context);
    captureContexts.append(captureContext);
    return captureContext;
}

TreelandCaptureContext::TreelandCaptureContext(struct ::treeland_capture_context_v1 *object)
    : QObject()
    , QtWayland::treeland_capture_context_v1(object)
    , m_captureFrame(nullptr)
{
}

void TreelandCaptureContext::treeland_capture_context_v1_source_ready(int32_t region_x,
                                                                      int32_t region_y,
                                                                      uint32_t region_width,
                                                                      uint32_t region_height,
                                                                      uint32_t source_type)
{
    Q_EMIT sourceReady(QRect(region_x, region_y, region_width, region_height), source_type);
}

void TreelandCaptureContext::treeland_capture_context_v1_source_failed(uint32_t reason)
{
    Q_EMIT sourceFailed(reason);
}

QPointer<TreelandCaptureFrame> TreelandCaptureContext::frame()
{
    if (m_captureFrame)
        return m_captureFrame;
    auto capture_frame = capture();
    m_captureFrame = new TreelandCaptureFrame(capture_frame);
    return m_captureFrame;
}

void TreelandCaptureContext::selectSource(uint32_t sourceHint,
                                          bool freeze,
                                          bool withCursor,
                                          ::wl_surface *mask)
{
    select_source(sourceHint, freeze, withCursor, mask);
}

void TreelandCaptureContext::releaseCaptureFrame()
{
    if (m_captureFrame) {
        delete m_captureFrame;
        m_captureFrame = nullptr;
    }
}

void TreelandCaptureFrame::treeland_capture_frame_v1_buffer(uint32_t format,
                                                            uint32_t width,
                                                            uint32_t height,
                                                            uint32_t stride)
{
    if (stride != width * 4) {
        qDebug() << "Receive a buffer format which is not compatible with "
                    "QWaylandShmBuffer."
                 << "format:" << format << "width:" << width << "height:" << height
                 << "stride:" << stride;
        return;
    }
    if (m_pendingShmBuffer)
        return; // We only need one supported format
    m_pendingShmBuffer = new QtWaylandClient::QWaylandShmBuffer(
        waylandDisplay(),
        QSize(width, height),
        QtWaylandClient::QWaylandShm::formatFrom(static_cast<::wl_shm_format>(format)));
    copy(m_pendingShmBuffer->buffer());
}

void TreelandCaptureFrame::treeland_capture_frame_v1_flags(uint32_t flags)
{
    m_flags = flags;
}

void TreelandCaptureFrame::treeland_capture_frame_v1_ready()
{
    if (m_shmBuffer)
        delete m_shmBuffer;
    m_shmBuffer = m_pendingShmBuffer;
    m_pendingShmBuffer = nullptr;
    Q_EMIT ready(*m_shmBuffer->image());
}

void TreelandCaptureFrame::treeland_capture_frame_v1_failed()
{
    Q_EMIT failed();
}

void TreelandCaptureManager::releaseCaptureContext(QPointer<TreelandCaptureContext> context)
{
    for (const auto &entry : captureContexts) {
        if (entry == context.data()) {
            entry->deleteLater();
            captureContexts.removeOne(entry);
        }
    }
}
