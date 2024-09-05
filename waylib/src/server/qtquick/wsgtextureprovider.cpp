// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsgtextureprovider.h"
#include "woutputrenderwindow.h"
#include "wrenderhelper.h"
#include "private/wglobal_p.h"

#include <rhi/qrhi.h>
#include <private/qsgplaintexture_p.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

#ifdef QT_DEBUG
Q_LOGGING_CATEGORY(lcQtQuickTexture, "waylib.qtquick.texture", QtDebugMsg);
#else
Q_LOGGING_CATEGORY(lcQtQuickTexture, "waylib.qtquick.texture", QtInfoMsg);
#endif

class Q_DECL_HIDDEN WSGTextureProviderPrivate : public WObjectPrivate
{
public:
    WSGTextureProviderPrivate(WSGTextureProvider *qq, WOutputRenderWindow *window)
        : WObjectPrivate(qq)
        , window(window)
    {
        qtTexture.setOwnsTexture(false);
    }

    ~WSGTextureProviderPrivate() {
        cleanTexture();
    }

    void cleanTexture() {
        if (rhiTexture) {
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
            window->scheduleRenderJob(new TextureCleanupJob(rhiTexture),
                                      QQuickWindow::AfterSynchronizingStage);
            rhiTexture = nullptr;
        }

        if (ownsTexture && texture)
            delete texture;
        texture = nullptr;
    }

    void updateRhiTexture() {
        Q_ASSERT(texture);
        bool ok = WRenderHelper::makeTexture(window->rhi(), texture, &qtTexture);
        if (Q_UNLIKELY(!ok)) {
            qCWarning(lcQtQuickTexture) << "Failed to make texture:" << texture
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
    if (buffer && buffer == qwBuffer()) {
        Q_EMIT textureChanged();
        return;
    }

    W_D(WSGTextureProvider);
    d->cleanTexture();
    d->ownsTexture = true;
    d->buffer = buffer;

    if (buffer) {
        Q_ASSERT(d->window);
        d->texture = qw_texture::from_buffer(*d->window->renderer(), *buffer);
        if (Q_UNLIKELY(!d->texture)) {
            qCWarning(lcQtQuickTexture) << "Failed to update texture from buffer:" << buffer
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

WAYLIB_SERVER_END_NAMESPACE
