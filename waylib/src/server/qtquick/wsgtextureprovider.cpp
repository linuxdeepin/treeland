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

extern "C" {
#include <wlr/types/wlr_buffer.h>
}

WAYLIB_SERVER_BEGIN_NAMESPACE

static const char *vulkanSamplingPolicyName(WSGTextureProvider::VulkanSamplingPolicy policy)
{
    switch (policy) {
    case WSGTextureProvider::VulkanSamplingPolicy::Invalid:
        return "invalid";
    case WSGTextureProvider::VulkanSamplingPolicy::Raw:
        return "raw";
    case WSGTextureProvider::VulkanSamplingPolicy::ShmUpload:
        return "shm-upload";
    }

    return "unknown";
}

static WSGTextureProvider::VulkanSamplingPolicy classifyVulkanSamplingPolicy(qw_buffer *buffer)
{
    if (!buffer)
        return WSGTextureProvider::VulkanSamplingPolicy::Raw;

    wlr_dmabuf_attributes dmabuf = {};
    if (wlr_buffer_get_dmabuf(buffer->handle(), &dmabuf))
        return WSGTextureProvider::VulkanSamplingPolicy::Raw;

    void *data = nullptr;
    uint32_t format = 0;
    size_t stride = 0;
    if (wlr_buffer_begin_data_ptr_access(buffer->handle(),
                                         WLR_BUFFER_DATA_PTR_ACCESS_READ,
                                         &data,
                                         &format,
                                         &stride)) {
        wlr_buffer_end_data_ptr_access(buffer->handle());
        return WSGTextureProvider::VulkanSamplingPolicy::ShmUpload;
    }

    return WSGTextureProvider::VulkanSamplingPolicy::Raw;
}

class Q_DECL_HIDDEN BufferRef
{
public:
    BufferRef() = default;
    ~BufferRef() { reset(); }

    BufferRef(const BufferRef &) = delete;
    BufferRef &operator=(const BufferRef &) = delete;

    BufferRef(BufferRef &&other) noexcept
    {
        std::swap(m_buffer, other.m_buffer);
    }

    BufferRef &operator=(BufferRef &&other) noexcept
    {
        if (this == &other)
            return *this;

        reset();
        std::swap(m_buffer, other.m_buffer);
        return *this;
    }

    void reset(qw_buffer *buffer = nullptr)
    {
        if (m_buffer == buffer)
            return;

        if (buffer) {
            buffer->lock();
            if (auto clientBuffer = qw_client_buffer::get(*buffer))
                clientBuffer->handle()->n_ignore_locks++;
        }

        release();
        m_buffer = buffer;
    }

    qw_buffer *get() const { return m_buffer; }

private:
    void release()
    {
        if (!m_buffer)
            return;

        if (auto clientBuffer = qw_client_buffer::get(*m_buffer))
            clientBuffer->handle()->n_ignore_locks--;
        m_buffer->unlock();
        m_buffer = nullptr;
    }

    qw_buffer *m_buffer = nullptr;
};

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

    void cleanTexture()
    {
        auto oldRhiTexture = rhiTexture;
        auto oldTexture = texture;
        const bool oldOwnsTexture = ownsTexture;
        auto oldBuffer = std::move(buffer);

        rhiTexture = nullptr;
        texture = nullptr;
        ownsTexture = false;
        vulkanSamplingPolicy = WSGTextureProvider::VulkanSamplingPolicy::Invalid;

        if (!oldRhiTexture && !oldTexture && !oldBuffer.get())
            return;

        class TextureCleanupJob : public QRunnable
        {
        public:
            TextureCleanupJob(QRhiTexture *rhiTexture,
                              qw_texture *texture,
                              bool ownsTexture,
                              BufferRef buffer)
                : rhiTexture(rhiTexture)
                , texture(texture)
                , ownsTexture(ownsTexture)
                , buffer(std::move(buffer))
            {
            }

            void run() override
            {
                if (rhiTexture)
                    rhiTexture->deleteLater();
                if (ownsTexture && texture)
                    delete texture;
            }

            QRhiTexture *rhiTexture = nullptr;
            qw_texture *texture = nullptr;
            bool ownsTexture = false;
            BufferRef buffer;
        };

        if (window) {
            window->scheduleRenderJob(new TextureCleanupJob(oldRhiTexture,
                                                            oldTexture,
                                                            oldOwnsTexture,
                                                            std::move(oldBuffer)),
                                      QQuickWindow::AfterRenderingStage);
            return;
        }

        if (oldRhiTexture)
            oldRhiTexture->deleteLater();
        if (oldOwnsTexture && oldTexture)
            delete oldTexture;
    }

    bool isVulkanRhi() const
    {
        return window && window->rhi() && window->rhi()->backend() == QRhi::Vulkan;
    }

    void updateMipmapFiltering()
    {
        qtTexture.setMipmapFiltering(isVulkanRhi()
                                         ? QSGTexture::None
                                         : (smooth ? QSGTexture::Linear : QSGTexture::Nearest));
    }

    bool updateRhiTexture() {
        Q_ASSERT(texture);
        const bool forceShaderReadOnlyLayout = isVulkanRhi()
                                               && vulkanSamplingPolicy != WSGTextureProvider::VulkanSamplingPolicy::Invalid;
        bool ok = WRenderHelper::makeTexture(window->rhi(), texture, &qtTexture, forceShaderReadOnlyLayout);
        if (Q_UNLIKELY(!ok)) {
            auto bufferHandle = buffer.get();
            qCWarning(lcWlQtQuickTexture) << "Failed to make Qt texture from wlroots texture"
                                          << "provider" << q_func()
                                          << "qwTexture" << texture
                                          << "wlrTexture" << texture->handle()
                                          << "qwBuffer" << bufferHandle
                                          << "wlrBuffer" << (bufferHandle ? bufferHandle->handle() : nullptr)
                                          << "policy" << vulkanSamplingPolicyName(vulkanSamplingPolicy)
                                          << "bufferSize" << (bufferHandle ? QSize(bufferHandle->handle()->width,
                                                                                   bufferHandle->handle()->height)
                                                                          : QSize());
            return false;
        }

        rhiTexture = qtTexture.rhiTexture();
        updateMipmapFiltering();
        qCDebug(lcWlQtQuickTexture) << "Updated Qt texture provider sampling policy"
                                    << "provider" << q_func()
                                    << "qwTexture" << texture
                                    << "wlrTexture" << texture->handle()
                                    << "qwBuffer" << buffer.get()
                                    << "wlrBuffer" << (buffer.get() ? buffer.get()->handle() : nullptr)
                                    << "policy" << vulkanSamplingPolicyName(vulkanSamplingPolicy)
                                    << "forceShaderReadOnlyLayout" << forceShaderReadOnlyLayout;
        return true;
    }

    W_DECLARE_PUBLIC(WSGTextureProvider)

    QPointer<WOutputRenderWindow> window;

    // wlroots resources
    qw_texture *texture = nullptr;
    bool ownsTexture = false;
    BufferRef buffer;
    WSGTextureProvider::VulkanSamplingPolicy vulkanSamplingPolicy = WSGTextureProvider::VulkanSamplingPolicy::Invalid;

    // qt resources
    QSGPlainTexture qtTexture;
    QRhiTexture *rhiTexture = nullptr;
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
    if (buffer == qwBuffer()) {
        // The buffer object is not changed, but maybe the buffer's content is changed.
        // So should emit textureChanged() signal too.
        if (buffer)
            Q_EMIT textureChanged();
        return;
    }

    W_D(WSGTextureProvider);
    d->cleanTexture();
    d->buffer.reset(buffer);
    d->vulkanSamplingPolicy = classifyVulkanSamplingPolicy(buffer);

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
            if (!d->updateRhiTexture())
                d->cleanTexture();
        }
    }

    Q_EMIT textureChanged();
}

void WSGTextureProvider::setTexture(qw_texture *texture, qw_buffer *srcBuffer)
{
    W_D(WSGTextureProvider);
    d->cleanTexture();
    d->texture = texture;
    d->buffer.reset(srcBuffer);
    d->vulkanSamplingPolicy = classifyVulkanSamplingPolicy(srcBuffer);
    d->ownsTexture = false;
    if (texture && !d->updateRhiTexture())
        d->cleanTexture();

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

bool WSGTextureProvider::hasTexture() const
{
    W_DC(WSGTextureProvider);
    return d->texture && d->rhiTexture;
}

qw_texture *WSGTextureProvider::qwTexture() const
{
    W_DC(WSGTextureProvider);
    return d->texture;
}

qw_buffer *WSGTextureProvider::qwBuffer() const
{
    W_DC(WSGTextureProvider);
    return d->buffer.get();
}

WSGTextureProvider::VulkanSamplingPolicy WSGTextureProvider::vulkanSamplingPolicy() const
{
    W_DC(WSGTextureProvider);
    return d->vulkanSamplingPolicy;
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
    d->updateMipmapFiltering();

    Q_EMIT smoothChanged();
}

WAYLIB_SERVER_END_NAMESPACE
