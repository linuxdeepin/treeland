// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef SCREENCAPTURE_H
#define SCREENCAPTURE_H

#include <QImage>
#include <QRect>
#include <QVector>

#include "qwayland-ext-image-capture-source-v1.h"
#include "qwayland-ext-image-copy-capture-v1.h"

#include <QtWaylandClient/private/qwayland-wayland.h>
#include <QtWaylandClient/private/qwayland-xdg-output-unstable-v1.h>

class ScreenCapture
{
public:
    ScreenCapture() = default;
    ~ScreenCapture();

    // Captures the whole canvas (all outputs, composed by logical position)
    // via ext-image-copy-capture. Returns an empty image on failure.
    QImage captureFullCanvas();

    // Enumerates outputs via xdg-output and returns the logical desktop
    // rect (union of all output logical geometries). No capture is performed.
    QRect desktopRect();

private:
    struct Output;

    class Registry : public QtWayland::wl_registry
    {
    public:
        ScreenCapture *owner = nullptr;

    protected:
        void registry_global(uint32_t name, const QString &interface, uint32_t version) override;
        void registry_global_remove(uint32_t name) override;
    };

    class XdgOutput : public QtWayland::zxdg_output_v1
    {
    public:
        Output *output = nullptr;

    protected:
        void zxdg_output_v1_logical_position(int32_t x, int32_t y) override;
        void zxdg_output_v1_logical_size(int32_t width, int32_t height) override;
    };

    class Session : public QtWayland::ext_image_copy_capture_session_v1
    {
    public:
        Output *output = nullptr;

    protected:
        void ext_image_copy_capture_session_v1_buffer_size(uint32_t width, uint32_t height) override;
        void ext_image_copy_capture_session_v1_shm_format(uint32_t format) override;
        void ext_image_copy_capture_session_v1_done() override;
    };

    class Frame : public QtWayland::ext_image_copy_capture_frame_v1
    {
    public:
        Output *output = nullptr;

    protected:
        void ext_image_copy_capture_frame_v1_ready() override;
        void ext_image_copy_capture_frame_v1_failed(uint32_t reason) override;
    };

    struct Output
    {
        QtWayland::wl_output wlOutput;
        XdgOutput xdgOutput;
        int logicalX = 0;
        int logicalY = 0;
        int logicalW = 0;
        int logicalH = 0;

        Session session;
        Frame frame;
        QtWayland::wl_shm_pool shmPool;
        QtWayland::wl_buffer buffer;

        int bufferW = 0;
        int bufferH = 0;
        uint32_t shmFormat = 0;
        QImage::Format imageFormat = QImage::Format_Invalid;
        bool constraintsDone = false;

        int shmFd = -1;
        void *shmData = nullptr;

        QImage image;
        bool captureReady = false;
        bool captureFailed = false;
    };

    static QImage::Format qimageFormatForShm(uint32_t format);
    void cleanupOutput(Output *output);

    Registry m_registry;
    QtWayland::wl_shm m_shm;
    QtWayland::zxdg_output_manager_v1 m_xdgOutputManager;
    QtWayland::ext_output_image_capture_source_manager_v1 m_extSourceManager;
    QtWayland::ext_image_copy_capture_manager_v1 m_extCopyManager;
    QVector<Output *> m_outputs;
    wl_display *m_display = nullptr;
};

#endif // SCREENCAPTURE_H
