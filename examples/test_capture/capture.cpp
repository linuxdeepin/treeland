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

TreelandCaptureManager *TreelandCaptureManager::instance()
{
    static TreelandCaptureManager manager;
    return &manager;
}

TreelandCaptureManager *TreelandCaptureManager::create(QQmlEngine *, QJSEngine *)
{
    auto manager = instance();
    QQmlEngine::setObjectOwnership(manager, QQmlEngine::CppOwnership);
    return manager;
}

TreelandCaptureManager::TreelandCaptureManager()
    : QWaylandClientExtensionTemplate<TreelandCaptureManager>(1)
    , QtWayland::treeland_capture_manager_v1()
{
    qInfo() << "TreelandCaptureManager created.";
    connect(this, &TreelandCaptureManager::activeChanged, this, [this] {
        if (!isActive() && m_context) {
            delete m_context;
        }
    });
}

TreelandCaptureManager::~TreelandCaptureManager()
{
    if (m_context)
        delete m_context;
    destroy();
}

TreelandCaptureContext *TreelandCaptureManager::ensureContext()
{
    if (m_context)
        return m_context;
    auto context = get_context();
    m_context = new TreelandCaptureContext(context, this);
    connect(m_context, &TreelandCaptureContext::destroyed, this, [this] {
        m_context = nullptr;
        Q_EMIT contextChanged();
    });
    Q_EMIT contextChanged();
    return m_context;
}

void TreelandCaptureContext::treeland_capture_context_v1_source_ready(int32_t region_x,
                                                                      int32_t region_y,
                                                                      uint32_t region_width,
                                                                      uint32_t region_height,
                                                                      uint32_t source_type)
{
    m_captureRegion = QRect(region_x, region_y, region_width, region_height);
    Q_EMIT captureRegionChanged();
    Q_EMIT sourceReady(QRect(region_x, region_y, region_width, region_height), source_type);
}

void TreelandCaptureContext::treeland_capture_context_v1_source_failed(uint32_t reason)
{
    Q_EMIT sourceFailed(reason);
}

TreelandCaptureFrame *TreelandCaptureContext::ensureFrame()
{
    if (m_frame)
        return m_frame;
    auto object = QtWayland::treeland_capture_context_v1::capture();
    m_frame = new TreelandCaptureFrame(object, this);
    connect(m_frame, &TreelandCaptureFrame::destroyed, this, [this] {
        m_frame = nullptr;
        Q_EMIT frameChanged();
    });
    Q_EMIT frameChanged();
    return m_frame;
}

TreelandCaptureContext::TreelandCaptureContext(::treeland_capture_context_v1 *object,
                                               QObject *parent)
    : QObject(parent)
    , QtWayland::treeland_capture_context_v1(object)
{
}

TreelandCaptureContext::~TreelandCaptureContext()
{
    if (m_frame)
        delete m_frame;
    if (m_session)
        delete m_session;
    destroy();
}

void TreelandCaptureContext::selectSource(uint32_t sourceHint,
                                          bool freeze,
                                          bool withCursor,
                                          ::wl_surface *mask)
{
    select_source(sourceHint, freeze, withCursor, mask);
}

TreelandCaptureSession *TreelandCaptureContext::ensureSession()
{
    if (m_session)
        return m_session;
    auto object = create_session();
    m_session = new TreelandCaptureSession(object, this);
    connect(m_session, &TreelandCaptureSession::destroyed, this, [this] {
        m_session = nullptr;
        Q_EMIT sessionChanged();
    });
    Q_EMIT sessionChanged();
    return m_session;
}

TreelandCaptureFrame::TreelandCaptureFrame(::treeland_capture_frame_v1 *object, QObject *parent)
    : QObject(parent)
    , QtWayland::treeland_capture_frame_v1(object)
    , m_shmBuffer(nullptr)
    , m_pendingShmBuffer(nullptr)
{
}

TreelandCaptureFrame::~TreelandCaptureFrame()
{
    delete m_shmBuffer;
    delete m_pendingShmBuffer;
    destroy();
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

TreelandCaptureSession::TreelandCaptureSession(::treeland_capture_session_v1 *object,
                                               QObject *parent)
    : QObject(parent)
    , QtWayland::treeland_capture_session_v1(object)
{
}

TreelandCaptureSession::~TreelandCaptureSession() { }

void TreelandCaptureSession::start()
{
    QtWayland::treeland_capture_session_v1::start();
    m_started = true;
    Q_EMIT started();
}

void TreelandCaptureSession::treeland_capture_session_v1_frame(int32_t offset_x,
                                                               int32_t offset_y,
                                                               uint32_t width,
                                                               uint32_t height,
                                                               uint32_t buffer_flags,
                                                               uint32_t flags,
                                                               uint32_t format,
                                                               uint32_t mod_high,
                                                               uint32_t mod_low,
                                                               uint32_t num_objects)
{
    Q_EMIT invalid();
    m_objects.clear();
    m_objects.reserve(num_objects);
    m_offset = { offset_x, offset_y };
    m_bufferWidth = width;
    m_bufferHeight = height;
    m_bufferFlags = buffer_flags;
    m_bufferFormat = format;
    m_flags = static_cast<QtWayland::treeland_capture_session_v1::flags>(flags);
    m_modifierUnion.modLow = mod_low;
    m_modifierUnion.modHigh = mod_high;
}

void TreelandCaptureSession::treeland_capture_session_v1_object(uint32_t index,
                                                                int32_t fd,
                                                                uint32_t size,
                                                                uint32_t offset,
                                                                uint32_t stride,
                                                                uint32_t plane_index)
{
    m_objects.push_back({ .index = index,
                          .fd = fd,
                          .size = size,
                          .offset = offset,
                          .stride = stride,
                          .planeIndex = plane_index });
}

void TreelandCaptureSession::treeland_capture_session_v1_ready(uint32_t tv_sec_hi,
                                                               uint32_t tv_sec_lo,
                                                               uint32_t tv_nsec)
{
    m_tvSecHi = tv_sec_hi;
    m_tvSecLo = tv_sec_lo;
    m_tvUsec = tv_nsec;
    Q_EMIT ready();
}

void TreelandCaptureSession::treeland_capture_session_v1_cancel(uint32_t reason) { }

void TreelandCaptureManager::setRecord(bool newRecord)
{
    if (m_record == newRecord)
        return;
    m_record = newRecord;
    qInfo() << "Set record to" << m_record;
    emit recordChanged();
}

bool TreelandCaptureManager::recordStarted() const
{
    if (m_context && m_context->session()) {
        return m_context->session()->started();
    } else {
        return false;
    }
}
