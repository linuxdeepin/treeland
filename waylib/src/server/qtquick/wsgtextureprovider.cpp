// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsgtextureprovider.h"
#include "woutputrenderwindow.h"
#include "wrenderhelper.h"
#include "private/wglobal_p.h"
#include "wayliblogging.h"

#include <wlr_all.h>

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
        }

        if (ownsTexture && texture)
            wlr_texture_destroy(texture);
        texture = nullptr;
    }

    void flushVulkanStageIfNeeded() {
#ifdef ENABLE_VULKAN_RENDER
        if (!window || !window->renderer())
            return;
        auto *renderer = window->renderer();
        if (!wlr_renderer_is_vk(renderer))
            return;
        // SHM uploads are recorded on wlroots' stage CB and only submitted by
        // a wlroots render pass. Flush before Qt samples the image.
        if (!waylib_vk_renderer_flush_stage(renderer)) {
            qCWarning(lcWlQtQuickTexture) << "Failed to flush Vulkan stage CB for SHM upload";
        }
#endif
    }

    void updateRhiTexture() {
        Q_ASSERT(texture);
        bool ok = WRenderHelper::makeTexture(window->rhi(), texture, &qtTexture);
        if (Q_UNLIKELY(!ok)) {
            qCWarning(lcWlQtQuickTexture) << "Failed to make texture:" << texture
                                        << ", width height:" << texture->width
                                        << texture->height;
            return;
        }

        flushVulkanStageIfNeeded();
    }

    W_DECLARE_PUBLIC(WSGTextureProvider)

    QPointer<WOutputRenderWindow> window;

    // wlroots resources
    wlr_texture *texture = nullptr;
    bool ownsTexture = false;
    wlr_buffer *buffer = nullptr;

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

void WSGTextureProvider::setBuffer(wlr_buffer *buffer)
{
    W_D(WSGTextureProvider);
    if (buffer == d->buffer) {
        // The buffer object is not changed, but maybe the buffer's content is changed.
        // So should emit textureChanged() signal too.
        if (buffer && d->texture) {
            d->flushVulkanStageIfNeeded();
            Q_EMIT textureChanged();
        }
        return;
    }

    d->cleanTexture();
    d->buffer = buffer;

    if (buffer) {
        Q_ASSERT(d->window);
        if (auto clientBuffer = wlr_client_buffer_get(buffer)) {
            // Acquire texture from client buffer. wlroots already generate texture for us if this is a client buffer.
            // By the way, there is something wrong with getting texture from a client buffer using wlr_texture_from_buffer,
            // See: https://gitlab.freedesktop.org/wlroots/wlroots/-/issues/3897
            // Possible patch:  https://gitlab.freedesktop.org/wlroots/wlroots/-/merge_requests/4889
            d->texture = clientBuffer->texture;
            d->ownsTexture = false;
        } else {
            d->texture = wlr_texture_from_buffer(d->window->renderer(), buffer);
            d->ownsTexture = true;
        }
        if (Q_UNLIKELY(!d->texture)) {
            qCWarning(lcWlQtQuickTexture) << "Failed to update texture from buffer:" << buffer
                                        << ", width height:" << buffer->width
                                        << buffer->height
                                        << ", n_locks:" << buffer->n_locks;
        } else {
            d->updateRhiTexture();
        }
    }

    Q_EMIT textureChanged();
}

void WSGTextureProvider::setTexture(wlr_texture *texture, wlr_buffer *srcBuffer)
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
    return d->texture ? const_cast<QSGPlainTexture*>(&d->qtTexture) : nullptr;
}

wlr_texture *WSGTextureProvider::qwTexture() const
{
    W_DC(WSGTextureProvider);
    return d->texture;
}

wlr_buffer *WSGTextureProvider::wlrBuffer() const
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
