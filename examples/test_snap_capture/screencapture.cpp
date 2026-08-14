// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "screencapture.h"

#include <QGuiApplication>
#include <QPainter>
#include <qnativeinterface.h>

#include <wayland-client.h>

#include <climits>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

QImage::Format ScreenCapture::qimageFormatForShm(uint32_t format)
{
    switch (format) {
    case WL_SHM_FORMAT_ARGB8888:
    case WL_SHM_FORMAT_BGRA8888:
        return QImage::Format_ARGB32;
    case WL_SHM_FORMAT_XRGB8888:
    case WL_SHM_FORMAT_BGRX8888:
        return QImage::Format_RGB32;
    case WL_SHM_FORMAT_XBGR8888:
        return QImage::Format_RGBX8888;
    default:
        return QImage::Format_Invalid;
    }
}

ScreenCapture::~ScreenCapture()
{
    for (Output *output : std::as_const(m_outputs)) {
        cleanupOutput(output);
        if (output->xdgOutput.isInitialized())
            output->xdgOutput.destroy();
        if (output->wlOutput.isInitialized())
            output->wlOutput.release();
        delete output;
    }
}

void ScreenCapture::cleanupOutput(Output *output)
{
    if (output->frame.isInitialized())
        output->frame.destroy();
    if (output->buffer.isInitialized())
        output->buffer.destroy();
    if (output->shmPool.isInitialized())
        output->shmPool.destroy();
    if (output->shmData) {
        munmap(output->shmData, output->bufferH * output->bufferW * 4);
        output->shmData = nullptr;
    }
    if (output->shmFd >= 0) {
        close(output->shmFd);
        output->shmFd = -1;
    }
    if (output->session.isInitialized())
        output->session.destroy();
}

void ScreenCapture::Registry::registry_global(uint32_t name, const QString &interface,
                                              uint32_t version)
{
    auto *self = owner;
    if (interface == QLatin1String("wl_shm")) {
        self->m_shm.init(self->m_registry.object(), name, version);
    } else if (interface == QLatin1String("zxdg_output_manager_v1")) {
        self->m_xdgOutputManager.init(self->m_registry.object(), name, version);
    } else if (interface == QLatin1String("ext_output_image_capture_source_manager_v1")) {
        self->m_extSourceManager.init(self->m_registry.object(), name, version);
    } else if (interface == QLatin1String("ext_image_copy_capture_manager_v1")) {
        self->m_extCopyManager.init(self->m_registry.object(), name, version);
    } else if (interface == QLatin1String("wl_output")) {
        auto *output = new Output;
        output->wlOutput.init(self->m_registry.object(), name, version);
        self->m_outputs.append(output);
    }
}

void ScreenCapture::Registry::registry_global_remove(uint32_t name)
{
    Q_UNUSED(name);
}

void ScreenCapture::XdgOutput::zxdg_output_v1_logical_position(int32_t x, int32_t y)
{
    output->logicalX = x;
    output->logicalY = y;
}

void ScreenCapture::XdgOutput::zxdg_output_v1_logical_size(int32_t width, int32_t height)
{
    output->logicalW = width;
    output->logicalH = height;
}

void ScreenCapture::Session::ext_image_copy_capture_session_v1_buffer_size(uint32_t width,
                                                                          uint32_t height)
{
    output->bufferW = width;
    output->bufferH = height;
}

void ScreenCapture::Session::ext_image_copy_capture_session_v1_shm_format(uint32_t format)
{
    if (output->imageFormat != QImage::Format_Invalid)
        return;
    output->imageFormat = qimageFormatForShm(format);
    if (output->imageFormat != QImage::Format_Invalid)
        output->shmFormat = format;
}

void ScreenCapture::Session::ext_image_copy_capture_session_v1_done()
{
    output->constraintsDone = true;
}

void ScreenCapture::Frame::ext_image_copy_capture_frame_v1_ready()
{
    auto *output = this->output;
    if (!output->shmData || output->bufferW <= 0 || output->bufferH <= 0
        || output->imageFormat == QImage::Format_Invalid) {
        output->captureFailed = true;
    } else {
        QImage image(static_cast<const uchar *>(output->shmData), output->bufferW, output->bufferH,
                     output->bufferW * 4, output->imageFormat);
        output->image = image.copy();
        output->captureReady = true;
    }
    destroy();
}

void ScreenCapture::Frame::ext_image_copy_capture_frame_v1_failed(uint32_t reason)
{
    Q_UNUSED(reason);
    output->captureFailed = true;
    destroy();
}

QImage ScreenCapture::captureFullCanvas()
{
    m_outputs.clear();

    auto *app = qGuiApp;
    auto *waylandApp = app ? app->nativeInterface<QNativeInterface::QWaylandApplication>() : nullptr;
    if (!waylandApp)
        return { };
    m_display = waylandApp->display();
    if (!m_display)
        return { };

    m_registry.owner = this;
    m_registry.init(wl_display_get_registry(m_display));
    wl_display_roundtrip(m_display);

    if (!m_shm.isInitialized() || !m_extSourceManager.isInitialized()
        || !m_extCopyManager.isInitialized() || m_outputs.isEmpty()) {
        return { };
    }

    if (m_xdgOutputManager.isInitialized()) {
        for (Output *output : std::as_const(m_outputs)) {
            output->xdgOutput.output = output;
            output->xdgOutput.init(m_xdgOutputManager.get_xdg_output(output->wlOutput.object()));
        }
        wl_display_roundtrip(m_display);
    }

    for (Output *output : std::as_const(m_outputs)) {
        output->session.output = output;
        auto *source = m_extSourceManager.create_source(output->wlOutput.object());
        auto *session = m_extCopyManager.create_session(source, 0);
        ext_image_capture_source_v1_destroy(source);
        output->session.init(session);
    }
    wl_display_roundtrip(m_display);

    static int serial = 0;
    int framesSent = 0;
    for (Output *output : std::as_const(m_outputs)) {
        if (!output->constraintsDone
            || output->imageFormat == QImage::Format_Invalid)
            continue;

        const int stride = output->bufferW * 4;
        const int size = stride * output->bufferH;

        QByteArray name = QByteArrayLiteral("/treeland-capture-")
            + QByteArray::number(static_cast<qint64>(getpid()))
            + QByteArrayLiteral("-") + QByteArray::number(serial++);
        int fd = shm_open(name.constData(), O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        if (fd < 0)
            continue;
        shm_unlink(name.constData());
        if (ftruncate(fd, size) < 0) {
            close(fd);
            continue;
        }
        auto *data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (data == MAP_FAILED) {
            close(fd);
            continue;
        }

        output->shmFd = fd;
        output->shmData = data;
        output->shmPool.init(m_shm.create_pool(fd, size));
        output->buffer.init(output->shmPool.create_buffer(
            0, output->bufferW, output->bufferH, stride, output->shmFormat));

        output->frame.output = output;
        auto *frame = output->session.create_frame();
        output->frame.init(frame);
        output->frame.attach_buffer(output->buffer.object());
        output->frame.damage_buffer(0, 0, INT32_MAX, INT32_MAX);
        output->frame.capture();
        ++framesSent;
    }

    if (framesSent > 0) {
        bool pending = true;
        while (pending) {
            if (wl_display_dispatch(m_display) < 0)
                break;
            pending = false;
            for (Output *output : std::as_const(m_outputs)) {
                if (!output->captureReady && !output->captureFailed) {
                    pending = true;
                    break;
                }
            }
        }
    }

    int minX = 0, minY = 0, maxX = 0, maxY = 0;
    bool haveImage = false;
    for (Output *output : std::as_const(m_outputs)) {
        if (!output->captureReady || output->image.isNull())
            continue;
        const int w = output->logicalW > 0 ? output->logicalW : output->bufferW;
        const int h = output->logicalH > 0 ? output->logicalH : output->bufferH;
        if (!haveImage) {
            minX = output->logicalX;
            minY = output->logicalY;
            maxX = output->logicalX + w;
            maxY = output->logicalY + h;
            haveImage = true;
            continue;
        }
        minX = qMin(minX, output->logicalX);
        minY = qMin(minY, output->logicalY);
        maxX = qMax(maxX, output->logicalX + w);
        maxY = qMax(maxY, output->logicalY + h);
    }
    if (!haveImage)
        return { };

    double scale = 1.0;
    for (Output *output : std::as_const(m_outputs)) {
        if (!output->captureReady || output->image.isNull())
            continue;
        if (output->logicalW > 0 && output->bufferW > 0)
            scale = qMax(scale, double(output->bufferW) / output->logicalW);
    }

    QImage canvas(qMax(1, qRound((maxX - minX) * scale)),
                  qMax(1, qRound((maxY - minY) * scale)),
                  QImage::Format_ARGB32);
    canvas.fill(Qt::transparent);
    QPainter painter(&canvas);
    for (Output *output : std::as_const(m_outputs)) {
        if (!output->captureReady || output->image.isNull())
            continue;
        const QRectF target((output->logicalX - minX) * scale,
                            (output->logicalY - minY) * scale,
                            (output->logicalW > 0 ? output->logicalW : output->bufferW) * scale,
                            (output->logicalH > 0 ? output->logicalH : output->bufferH) * scale);
        painter.drawImage(target, output->image);
    }
    painter.end();

    return canvas;
}

QRect ScreenCapture::desktopRect()
{
    auto *app = qGuiApp;
    auto *waylandApp = app ? app->nativeInterface<QNativeInterface::QWaylandApplication>() : nullptr;
    if (!waylandApp)
        return {};
    auto *display = waylandApp->display();
    if (!display)
        return {};

    m_registry.owner = this;
    m_registry.init(wl_display_get_registry(display));
    wl_display_roundtrip(display);

    if (m_outputs.isEmpty())
        return {};

    if (m_xdgOutputManager.isInitialized()) {
        for (Output *output : std::as_const(m_outputs)) {
            output->xdgOutput.output = output;
            output->xdgOutput.init(m_xdgOutputManager.get_xdg_output(output->wlOutput.object()));
        }
        wl_display_roundtrip(display);
    }

    int minX = 0, minY = 0, maxX = 0, maxY = 0;
    bool have = false;
    for (Output *output : std::as_const(m_outputs)) {
        if (output->logicalW <= 0 || output->logicalH <= 0)
            continue;
        if (!have) {
            minX = output->logicalX;
            minY = output->logicalY;
            maxX = output->logicalX + output->logicalW;
            maxY = output->logicalY + output->logicalH;
            have = true;
            continue;
        }
        minX = qMin(minX, output->logicalX);
        minY = qMin(minY, output->logicalY);
        maxX = qMax(maxX, output->logicalX + output->logicalW);
        maxY = qMax(maxY, output->logicalY + output->logicalH);
    }

    return have ? QRect(minX, minY, maxX - minX, maxY - minY) : QRect{};
}
