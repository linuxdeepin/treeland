// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "qwayland-treeland-capture-unstable-v1.h"

#include <private/qwaylandclientextension_p.h>
#include <private/qwaylandshmbackingstore_p.h>

class TreelandCaptureFrame
    : public QObject
    , public QtWayland::treeland_capture_frame_v1
{
    Q_OBJECT
public:
    TreelandCaptureFrame(struct ::treeland_capture_frame_v1 *object)
        : QObject()
        , QtWayland::treeland_capture_frame_v1(object)
        , m_shmBuffer(nullptr)
        , m_pendingShmBuffer(nullptr)
    {
    }

    ~TreelandCaptureFrame() override
    {
        delete m_shmBuffer;
        delete m_pendingShmBuffer;
        destroy();
    }

    inline uint flags() const
    {
        return m_flags;
    }

Q_SIGNALS:
    void ready(QImage image);
    void failed();

protected:
    void treeland_capture_frame_v1_buffer(uint32_t format,
                                          uint32_t width,
                                          uint32_t height,
                                          uint32_t stride) override;
    void treeland_capture_frame_v1_flags(uint32_t flags) override;
    void treeland_capture_frame_v1_ready() override;
    void treeland_capture_frame_v1_failed() override;

private:
    QtWaylandClient::QWaylandShmBuffer *m_shmBuffer;
    QtWaylandClient::QWaylandShmBuffer *m_pendingShmBuffer;
    uint m_flags;
};

class TreelandCaptureContext
    : public QObject
    , public QtWayland::treeland_capture_context_v1
{
    Q_OBJECT
public:
    explicit TreelandCaptureContext(struct ::treeland_capture_context_v1 *object);

    ~TreelandCaptureContext() override
    {
        releaseCaptureFrame();
        destroy();
    }

    inline QRect captureRegion() const
    {
        return m_captureRegion;
    }

    inline QtWayland::treeland_capture_context_v1::source_type sourceType() const
    {
        return m_sourceType;
    }

    QPointer<TreelandCaptureFrame> frame();
    void selectSource(uint32_t sourceHint, bool freeze, bool withCursor, ::wl_surface *mask);
    void releaseCaptureFrame();

Q_SIGNALS:
    void sourceReady(QRect region, uint32_t sourceType);
    void sourceFailed(uint32_t reason);

protected:
    void treeland_capture_context_v1_source_ready(int32_t region_x,
                                                  int32_t region_y,
                                                  uint32_t region_width,
                                                  uint32_t region_height,
                                                  uint32_t source_type) override;
    void treeland_capture_context_v1_source_failed(uint32_t reason) override;

private:
    QRect m_captureRegion;
    TreelandCaptureFrame *m_captureFrame;
    QtWayland::treeland_capture_context_v1::source_type m_sourceType;
};

class TreelandCaptureManager
    : public QWaylandClientExtensionTemplate<TreelandCaptureManager>
    , public QtWayland::treeland_capture_manager_v1
{
    Q_OBJECT
public:
    TreelandCaptureManager(QObject *parent = nullptr)
        : QWaylandClientExtensionTemplate<TreelandCaptureManager>(1)
        , QtWayland::treeland_capture_manager_v1()
    {
        connect(this, &TreelandCaptureManager::activeChanged, this, [this] {
            if (!isActive()) {
                qDeleteAll(captureContexts);
                captureContexts.clear();
            }
        });
    }

    ~TreelandCaptureManager() override
    {
        destroy();
    }

    QPointer<TreelandCaptureContext> getContext();
    void releaseCaptureContext(QPointer<TreelandCaptureContext> context);

private:
    QList<TreelandCaptureContext *> captureContexts;
};
