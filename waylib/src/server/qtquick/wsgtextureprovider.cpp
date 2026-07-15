// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsgtextureprovider.h"
#include "woutputrenderwindow.h"
#include "wrenderhelper.h"
#include "wtools.h"
#include "private/wglobal_p.h"
#include "wayliblogging.h"

#include <qwtexture.h>
#include <qwbuffer.h>
#include <qwrenderer.h>

#include <rhi/qrhi.h>
#include <private/qsgplaintexture_p.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WSGTextureProviderPrivate : public WObjectPrivate
{
public:
    WSGTextureProviderPrivate(WSGTextureProvider *qq, WOutputRenderWindow *window)
        : WObjectPrivate(qq)
        , window(window)
    {
        qtTexture.setOwnsTexture(false);
        qtTexture.setFiltering(smooth ? QSGTexture::Linear
                                      : QSGTexture::Nearest);
    }

    ~WSGTextureProviderPrivate() {
        cleanTexture();
    }

    void cleanTexture() {
        auto *oldRhiTexture = qtTexture.rhiTexture();
        if (oldRhiTexture) {
            // QSGPlainTexture does not own the QRhiTexture in this provider.
            // Clear its pointer before scheduling the deferred destruction.
            qtTexture.setOwnsTexture(false);
            qtTexture.setTexture(nullptr);

            Q_ASSERT(window);
            class TextureCleanupJob : public QRunnable
            {
            public:
                TextureCleanupJob(QRhiTexture *texture)
                    : texture(texture) { }
                void run() override {
                    texture->deleteLater();
                }
                QRhiTexture *texture;
            };

            // Delay clean the qt rhi textures.
            window->scheduleRenderJob(new TextureCleanupJob(oldRhiTexture),
                                      QQuickWindow::AfterSynchronizingStage);
        } else if (imageBacked) {
            qtTexture.setImage({});
        }
        imageBacked = false;

        if (ownsTexture && texture)
            delete texture;
        texture = nullptr;
    }

    bool updateImageTexture(qw_buffer *newBuffer) {
        void *data = nullptr;
        uint32_t drmFormat = 0;
        size_t stride = 0;
        if (!newBuffer->begin_data_ptr_access(WLR_BUFFER_DATA_PTR_ACCESS_READ,
                                              &data, &drmFormat, &stride))
            return false;

        const auto imageFormat = WTools::convertToDrmSupportedFormat(
            WTools::toImageFormat(drmFormat));
        if (imageFormat == QImage::Format_Invalid) {
            newBuffer->end_data_ptr_access();
            return false;
        }
        const auto image = QImage(static_cast<const uchar *>(data),
                                  static_cast<int>(newBuffer->handle()->width),
                                  static_cast<int>(newBuffer->handle()->height),
                                  static_cast<int>(stride), imageFormat)
                               .copy();
        newBuffer->end_data_ptr_access();
        if (image.isNull())
            return false;

        qtTexture.setImage(image);
        imageBacked = true;
        return true;
    }

    void updateRhiTexture() {
        Q_ASSERT(texture);
        bool ok = WRenderHelper::makeTexture(window->rhi(), texture, &qtTexture);
        if (Q_UNLIKELY(!ok)) {
            qCWarning(lcWlQtQuickTexture) << "Failed to make texture:" << texture
                                        << ", width height:" << texture->handle()->width
                                        << ", height:" << texture->handle()->height;
        }
    }
    W_DECLARE_PUBLIC(WSGTextureProvider)

    QPointer<WOutputRenderWindow> window;

    // wlroots resources
    qw_texture *texture = nullptr;
    bool ownsTexture = false;
    qw_buffer *buffer = nullptr;
    bool imageBacked = false;
    // qt resources
    QSGPlainTexture qtTexture;
    bool smooth = true;
};

WSGTextureProvider::WSGTextureProvider(WOutputRenderWindow *window)
    : WObject(*new WSGTextureProviderPrivate(this, window))
{

}

WOutputRenderWindow *WSGTextureProvider::window() const
{
    W_D(const WSGTextureProvider);
    return d->window;
}

void WSGTextureProvider::setBuffer(qw_buffer *buffer)
{
    W_D(WSGTextureProvider);
    if (buffer == d->buffer) {
        // The buffer object is not changed, but its contents may have changed.
        if (buffer && d->imageBacked)
            d->updateImageTexture(buffer);
        if (buffer)
            Q_EMIT textureChanged();
        return;
    }

    d->cleanTexture();
    d->buffer = buffer;

    if (buffer) {
        Q_ASSERT(d->window);
        if (WRenderHelper::getGraphicsApi() == QSGRendererInterface::Vulkan
            && d->updateImageTexture(buffer)) {
            // SHM uploads are owned by Qt RHI. This avoids wlroots' deferred
            // Vulkan stage command buffer, which is submitted only by a wlroots
            // render pass and is not part of Qt's command stream.
        } else {
            if (auto clientBuffer = qw_client_buffer::get(*buffer)) {
                // Acquire texture from client buffer. wlroots already generate texture for us if this is a client buffer.
                // By the way, there is something wrong with getting texture from a client buffer using wlr_texture_from_buffer,
                // See: https://gitlab.freedesktop.org/wlroots/wlroots/-/issues/3897
                // Possible patch: https://gitlab.freedesktop.org/wlroots/wlroots/-/merge_requests/4889
                d->texture = qw_texture::from(clientBuffer->handle()->texture);
                d->ownsTexture = false;
            } else {
                d->texture = qw_texture::from_buffer(*d->window->renderer(), *buffer);
                d->ownsTexture = true;
            }
            if (Q_UNLIKELY(!d->texture)) {
                qCWarning(lcWlQtQuickTexture) << "Failed to update texture from buffer:" << buffer
                                            << ", width height:" << buffer->handle()->width
                                            << buffer->handle()->height
                                            << ", n_locks:" << buffer->handle()->n_locks;
            } else {
                d->updateRhiTexture();
            }
        }
    }

    Q_EMIT textureChanged();
}

void WSGTextureProvider::setTexture(qw_texture *texture, qw_buffer *srcBuffer)
{
    W_D(WSGTextureProvider);
    d->cleanTexture();
    d->texture = texture;
    d->buffer = srcBuffer;
    d->ownsTexture = false;
    if (texture)
        d->updateRhiTexture();

    Q_EMIT textureChanged();
}

void WSGTextureProvider::invalidate()
{
    W_D(WSGTextureProvider);
    d->cleanTexture();
    d->window = nullptr;

    Q_EMIT textureChanged();
}

QSGTexture *WSGTextureProvider::texture() const
{
    W_DC(WSGTextureProvider);
    return (d->imageBacked || d->texture)
        ? const_cast<QSGPlainTexture*>(&d->qtTexture)
        : nullptr;
}

qw_texture *WSGTextureProvider::qwTexture() const
{
    W_DC(WSGTextureProvider);
    return d->texture;
}

qw_buffer *WSGTextureProvider::qwBuffer() const
{
    W_DC(WSGTextureProvider);
    return d->buffer;
}

bool WSGTextureProvider::smooth() const
{
    W_DC(WSGTextureProvider);
    return d->smooth;
}

void WSGTextureProvider::setSmooth(bool newSmooth)
{
    W_D(WSGTextureProvider);
    if (d->smooth == newSmooth)
        return;
    d->smooth = newSmooth;
    d->qtTexture.setFiltering(newSmooth ? QSGTexture::Linear
                                        : QSGTexture::Nearest);

    Q_EMIT smoothChanged();
}

WAYLIB_SERVER_END_NAMESPACE
