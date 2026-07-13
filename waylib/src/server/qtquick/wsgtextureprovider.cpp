// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsgtextureprovider.h"
#include "woutputrenderwindow.h"
#include "wrenderhelper.h"
#include "private/wglobal_p.h"
#include "wayliblogging.h"

#include <qwtexture.h>
#include <qwbuffer.h>
#include <qwrenderer.h>

#include <rhi/qrhi.h>
#include <private/qsgplaintexture_p.h>
#include <memory>

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
        qtTexture.setMipmapFiltering(smooth ? QSGTexture::Linear
                                            : QSGTexture::Nearest);
    }

    ~WSGTextureProviderPrivate() {
        cleanTexture();
    }

    void cleanTexture() {
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

        if (rhiTexture) {
            Q_ASSERT(window);
            // Delay clean the qt rhi textures.
            window->scheduleRenderJob(new TextureCleanupJob(rhiTexture),
                                      QQuickWindow::AfterSynchronizingStage);
            rhiTexture = nullptr;
        }

        if (shmTexture) {
            if (QRhiTexture *tex = shmTexture->rhiTexture()) {
                Q_ASSERT(window);
                window->scheduleRenderJob(new TextureCleanupJob(tex),
                                          QQuickWindow::AfterSynchronizingStage);
            }
            // Prevent QSGPlainTexture from deleting the texture in its
            // destructor; the deferred job above owns the cleanup.
            shmTexture->setOwnsTexture(false);
            shmTexture.reset();
            hasShmImage = false;
        }

        if (ownsTexture && texture)
            delete texture;
        texture = nullptr;
    }

    void updateRhiTexture() {
        Q_ASSERT(texture);
        bool ok = WRenderHelper::makeTexture(window->rhi(), texture, &qtTexture);
        if (Q_UNLIKELY(!ok)) {
            qCWarning(lcWlQtQuickTexture) << "Failed to make texture:" << texture
                                        << ", width height:" << texture->handle()->width
                                        << texture->handle()->height;
            return;
        }

        rhiTexture = qtTexture.rhiTexture();
    }

    W_DECLARE_PUBLIC(WSGTextureProvider)

    QPointer<WOutputRenderWindow> window;

    // wlroots resources
    qw_texture *texture = nullptr;
    bool ownsTexture = false;
    qw_buffer *buffer = nullptr;

    // qt resources
    QSGPlainTexture qtTexture;
    QRhiTexture *rhiTexture = nullptr;
    bool smooth = true;

    // For shm buffers in vulkan mode: a Qt-owned texture uploaded from shm
    // pixels, instead of borrowing wlroots' device-local VkImage.
    std::unique_ptr<QSGPlainTexture> shmTexture;
    bool hasShmImage = false;

    QSGPlainTexture *createPlainTexture() {
        QSGPlainTexture *t = new QSGPlainTexture;
        t->setOwnsTexture(true);
        t->setFiltering(smooth ? QSGTexture::Linear : QSGTexture::Nearest);
        t->setMipmapFiltering(smooth ? QSGTexture::Linear : QSGTexture::Nearest);
        return t;
    }
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

    if (buffer == qwBuffer()) {
        // The buffer object is not changed, but maybe the buffer's content is changed.
        // So should emit textureChanged() signal too.
        if (buffer) {
#ifdef ENABLE_VULKAN_RENDER
            // For the shm upload path, wlroots applies damage to its own texture
            // (which we don't use). Re-read the shm pixels so the Qt-owned
            // texture reflects the latest content.
            //
            // NOTE: This re-reads and re-uploads the entire buffer, not just
            // the damaged region. Partial dirty-region optimization (via
            // QRhiTextureUpdater or a custom QSGTexture) is deferred to a
            // future iteration.
            //
            // makeTextureFromShm calls setImage() internally; QSGPlainTexture
            // compares the new and old images via QImage::operator==, which
            // does a memcmp on pixel data (not a pointer comparison), so
            // unchanged content skips the GPU upload but is not zero-cost.
            if (d->hasShmImage)
                WRenderHelper::makeTextureFromShm(buffer, d->shmTexture.get());
#endif
            Q_EMIT textureChanged();
        }
        return;
    }

#ifdef ENABLE_VULKAN_RENDER
    // In vulkan mode, shm client buffers must be uploaded by Qt itself instead of
    // borrowing wlroots' device-local VkImage. That VkImage shares the VkDevice
    // with Qt but runs on a separate command stream without cross-stream sync,
    // so Qt may sample it before the upload is complete. dmabuf buffers are
    // unaffected (dma-buf sync provides cross-stream synchronization).
    if (buffer && WRenderHelper::getGraphicsApi() == QSGRendererInterface::Vulkan) {
        wlr_shm_attributes shmAttrs;
        if (buffer->get_shm(&shmAttrs)) {
            std::unique_ptr<QSGPlainTexture> shmTex(d->createPlainTexture());
            if (WRenderHelper::makeTextureFromShm(buffer, shmTex.get())) {
                d->cleanTexture();
                d->buffer = buffer;
                d->shmTexture = std::move(shmTex);
                d->hasShmImage = true;
                Q_EMIT textureChanged();
                return;
            }
            // Shm upload failed (e.g. unsupported pixel format): fall back
            // to the wlroots texture path below.
        }
    }
#endif

    d->cleanTexture();
    d->buffer = buffer;

    if (buffer) {
        Q_ASSERT(d->window);
        if (auto clientBuffer = qw_client_buffer::get(*buffer)) {
            // Acquire texture from client buffer. wlroots already generate texture for us if this is a client buffer.
            // By the way, there is something wrong with getting texture from a client buffer using wlr_texture_from_buffer,
            // See: https://gitlab.freedesktop.org/wlroots/wlroots/-/issues/3897
            // Possible patch:  https://gitlab.freedesktop.org/wlroots/wlroots/-/merge_requests/4889
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
    if (d->hasShmImage)
        return d->shmTexture.get();
    return d->texture ? const_cast<QSGPlainTexture*>(&d->qtTexture) : nullptr;
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
    d->qtTexture.setMipmapFiltering(newSmooth ? QSGTexture::Linear
                                              : QSGTexture::Nearest);
    if (d->shmTexture) {
        d->shmTexture->setFiltering(newSmooth ? QSGTexture::Linear
                                              : QSGTexture::Nearest);
        d->shmTexture->setMipmapFiltering(newSmooth ? QSGTexture::Linear
                                                    : QSGTexture::Nearest);
    }

    Q_EMIT smoothChanged();
}

WAYLIB_SERVER_END_NAMESPACE
