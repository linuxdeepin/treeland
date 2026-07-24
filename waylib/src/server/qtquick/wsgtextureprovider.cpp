// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsgtextureprovider.h"
#include "woutputrenderwindow.h"
#include "wrenderhelper.h"
#include "private/wglobal_p.h"
#include "utils/private/wvulkantrace_p.h"
#include "wayliblogging.h"

#include <qwtexture.h>
#include <qwbuffer.h>
#include <qwrenderer.h>

#include <rhi/qrhi.h>
#include <QQuickRenderControl>
#include <private/qsgplaintexture_p.h>

#include <utility>

extern "C" {
#include <wlr/types/wlr_buffer.h>
}

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN BufferRef
{
public:
    BufferRef() = default;
    ~BufferRef() { reset(); }

    BufferRef(const BufferRef &) = delete;
    BufferRef &operator=(const BufferRef &) = delete;

    BufferRef(BufferRef &&other) noexcept
    {
        m_buffer = std::exchange(other.m_buffer, nullptr);
    }

    BufferRef &operator=(BufferRef &&other) noexcept
    {
        if (this == &other)
            return *this;

        reset();
        m_buffer = std::exchange(other.m_buffer, nullptr);
        return *this;
    }

    void reset(qw_buffer *buffer = nullptr)
    {
        if (m_buffer == buffer)
            return;

        if (buffer) {
            buffer->lock();
            if (auto clientBuffer = qw_client_buffer::get(*buffer))
                clientBuffer->add_ignore_lock();
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
            clientBuffer->remove_ignore_lock();
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
        , vulkanRhi(window && window->rhi()
                    && window->rhi()->backend() == QRhi::Vulkan)
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

    void scheduleCleanup(QRhiTexture *oldRhiTexture,
                         qw_texture *oldTexture,
                         bool oldOwnsTexture,
                         BufferRef oldBuffer)
    {
        if (!oldRhiTexture && !oldTexture && !oldBuffer.get())
            return;

        const auto cleanupToken = WVulkanTrace::cleanupScheduled(q_func(), window,
                                                                 oldBuffer.get(), oldTexture);

        class TextureCleanupJob : public QRunnable
        {
        public:
            TextureCleanupJob(QRhiTexture *rhiTexture,
                              qw_texture *texture,
                              bool ownsTexture,
                              BufferRef buffer,
                              WVulkanTrace::CleanupToken cleanupToken,
                              bool deleteRhiTextureImmediately)
                : rhiTexture(rhiTexture)
                , texture(texture)
                , ownsTexture(ownsTexture)
                , buffer(std::move(buffer))
                , cleanupToken(cleanupToken)
                , deleteRhiTextureImmediately(deleteRhiTextureImmediately)
            {
            }

            ~TextureCleanupJob() override
            {
                cleanup();
            }

            void run() override
            {
                WVulkanTrace::cleanupRunning(cleanupToken);
                cleanup();
            }

        private:
            void cleanup()
            {
                if (rhiTexture) {
                    if (deleteRhiTextureImmediately)
                        delete rhiTexture;
                    else
                        rhiTexture->deleteLater();
                    rhiTexture = nullptr;
                }
                if (ownsTexture && texture) {
                    delete texture;
                    texture = nullptr;
                    ownsTexture = false;
                }
            }

            QRhiTexture *rhiTexture = nullptr;
            qw_texture *texture = nullptr;
            bool ownsTexture = false;
            BufferRef buffer;
            WVulkanTrace::CleanupToken cleanupToken;
            bool deleteRhiTextureImmediately = false;
        };

        if (window) {
            const bool isVulkan = isVulkanRhi();
            window->scheduleRenderJob(new TextureCleanupJob(oldRhiTexture,
                                                            oldTexture,
                                                            oldOwnsTexture,
                                                            std::move(oldBuffer),
                                                            cleanupToken,
                                                            isVulkan),
                                      isVulkan ? QQuickWindow::AfterSwapStage
                                               : QQuickWindow::AfterRenderingStage);
            return;
        }

        WVulkanTrace::cleanupRunning(cleanupToken);
        if (oldRhiTexture) {
            if (isVulkanRhi())
                delete oldRhiTexture;
            else
                oldRhiTexture->deleteLater();
        }
        if (oldOwnsTexture && oldTexture)
            delete oldTexture;
    }

    void cleanTexture()
    {
        auto oldRhiTexture = rhiTexture;
        auto oldTexture = texture;
        const bool oldOwnsTexture = ownsTexture;
        auto oldBuffer = std::move(buffer);

        if (isVulkanRhi())
            qtTexture.setTexture(nullptr);
        rhiTexture = nullptr;
        texture = nullptr;
        ownsTexture = false;
        failedBuffer = nullptr;
        failedTexture = nullptr;

        scheduleCleanup(oldRhiTexture,
                        oldTexture,
                        oldOwnsTexture,
                        std::move(oldBuffer));
    }

    void adoptTexture(qw_texture *newTexture,
                      bool newOwnsTexture,
                      BufferRef newBuffer)
    {
        auto oldRhiTexture = rhiTexture;
        auto oldTexture = texture;
        const bool oldOwnsTexture = ownsTexture;
        auto oldBuffer = std::move(buffer);

        texture = newTexture;
        ownsTexture = newOwnsTexture;
        buffer = std::move(newBuffer);
        rhiTexture = qtTexture.rhiTexture();
        failedBuffer = nullptr;
        failedTexture = nullptr;
        updateMipmapFiltering();

        scheduleCleanup(oldRhiTexture,
                        oldTexture,
                        oldOwnsTexture,
                        std::move(oldBuffer));
    }

    bool isVulkanRhi() const
    {
        return vulkanRhi;
    }

    void updateMipmapFiltering()
    {
        qtTexture.setMipmapFiltering(isVulkanRhi()
                                         ? QSGTexture::None
                                         : (smooth ? QSGTexture::Linear : QSGTexture::Nearest));
    }

    bool updateRhiTexture(qw_texture *newTexture, qw_buffer *newBuffer,
                          QSGPlainTexture *targetTexture = nullptr)
    {
        Q_ASSERT(newTexture);
        if (!targetTexture)
            targetTexture = &qtTexture;
        const bool forceShaderReadOnlyLayout = isVulkanRhi();
        const bool ok = WRenderHelper::makeTexture(window->rhi(),
                                                   newTexture,
                                                   targetTexture,
                                                   forceShaderReadOnlyLayout);
        if (Q_UNLIKELY(!ok)) {
            const QSize bufferSize = newBuffer
                                         ? QSize(newBuffer->handle()->width,
                                                 newBuffer->handle()->height)
                                         : QSize();
            const bool repeatedFailure = failedBuffer == newBuffer
                && failedTexture == newTexture;
            if (!repeatedFailure) {
                qCWarning(lcWlQtQuickTexture) << "Failed to make Qt texture from wlroots texture; keeping the previous texture"
                                              << "provider" << q_func()
                                              << "qwTexture" << newTexture
                                              << "wlrTexture" << newTexture->handle()
                                              << "qwBuffer" << newBuffer
                                              << "wlrBuffer" << (newBuffer ? newBuffer->handle() : nullptr)
                                              << "bufferSize" << bufferSize;
            } else {
                qCDebug(lcWlQtQuickTexture) << "Repeated Qt texture update failure; keeping the previous texture"
                                            << "provider" << q_func()
                                            << "qwTexture" << newTexture
                                            << "wlrTexture" << newTexture->handle()
                                            << "qwBuffer" << newBuffer
                                            << "wlrBuffer" << (newBuffer ? newBuffer->handle() : nullptr)
                                            << "bufferSize" << bufferSize;
            }
            failedBuffer = newBuffer;
            failedTexture = newTexture;
            return false;
        }

        failedBuffer = nullptr;
        failedTexture = nullptr;
        return true;
    }

    W_DECLARE_PUBLIC(WSGTextureProvider)

    QPointer<WOutputRenderWindow> window;
    const bool vulkanRhi = false;

    // wlroots resources
    qw_texture *texture = nullptr;
    bool ownsTexture = false;
    BufferRef buffer;

    // qt resources
    QSGPlainTexture qtTexture;
    QRhiTexture *rhiTexture = nullptr;
    bool smooth = true;
    qw_buffer *failedBuffer = nullptr;
    qw_texture *failedTexture = nullptr;
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
        if (!d->isVulkanRhi()) {
            // Preserve the established GLES2/Pixman same-buffer behavior.
            // The buffer object is unchanged, but its content may have changed.
            if (buffer)
                Q_EMIT textureChanged();
            return;
        }

        if (!buffer)
            return;

        if (d->rhiTexture) {
            auto *clientBuffer = qw_client_buffer::get(*buffer);
            const auto *clientTexture = clientBuffer
                ? qw_texture::from(clientBuffer->texture())
                : d->texture;
            if (clientTexture != d->texture) {
                // The owner stayed stable but wlroots replaced its texture.
                // Rebuild the Qt wrapper below instead of reusing stale native
                // image state.
            } else {
                WVulkanTrace::providerReuse(this, d->window, buffer, d->texture);
                Q_EMIT textureChanged();
                return;
            }
        }

        // A previous Vulkan wrapper creation may have failed while the owner
        // stayed unchanged. Fall through and retry instead of treating the
        // missing QRhi texture as reusable state.
    }

    if (!buffer) {
        d->cleanTexture();
        Q_EMIT textureChanged();
        return;
    }

    Q_ASSERT(d->window);

    if (d->isVulkanRhi()) {
        BufferRef candidateBuffer;
        candidateBuffer.reset(buffer);

        qw_texture *candidateTexture = nullptr;
        bool candidateOwnsTexture = false;
        if (auto clientBuffer = qw_client_buffer::get(*buffer)) {
            // wlroots owns and updates client textures. Qt only wraps the
            // resulting VkImage for read-only sampling.
            candidateTexture = qw_texture::from(clientBuffer->texture());
        } else {
            candidateTexture = qw_texture::from_buffer(*d->window->renderer(), *buffer);
            candidateOwnsTexture = true;
        }

        if (Q_UNLIKELY(!candidateTexture)) {
            qCWarning(lcWlQtQuickTexture) << "Failed to update texture from buffer:" << buffer
                                        << ", width height:" << buffer->handle()->width
                                        << buffer->handle()->height
                                        << ", n_locks:" << buffer->handle()->n_locks;
            return;
        }

        WVulkanTrace::providerBind(this, d->window, candidateBuffer.get(), candidateTexture);
        QSGPlainTexture candidateQtTexture;
        if (!d->updateRhiTexture(candidateTexture, candidateBuffer.get(), &candidateQtTexture)) {
            WVulkanTrace::providerDiscard(this, d->window, candidateTexture,
                                          "qt-wrap-failed");
            if (candidateOwnsTexture)
                delete candidateTexture;
            return;
        }

        // Immediately prepare new texture for sampling if we're in an active render frame.
        // This is critical for textures created during updatePaintNode() (e.g., cursor textures)
        // which would otherwise miss the prepareTextureSamplingForRenderPass() call that
        // happens before renderNextFrame().
        const bool prepareOk = d->window->prepareTextureForCurrentRenderPass(candidateTexture,
                                                                             "setbuffer-immediate");
        if (!prepareOk) {
            qCWarning(lcWlQtQuickTexture)
                << "Immediate texture preparation failed; keeping the previous Vulkan texture"
                << "provider" << this
                << "qwTexture" << candidateTexture
                << "wlrTexture" << candidateTexture->handle();
            WVulkanTrace::providerDiscard(this, d->window, candidateTexture,
                                          "sampling-prepare-failed");
            auto *candidateRhiTexture = candidateQtTexture.rhiTexture();
            candidateQtTexture.setOwnsTexture(false);
            d->scheduleCleanup(candidateRhiTexture,
                               candidateTexture,
                               candidateOwnsTexture,
                               std::move(candidateBuffer));
            return;
        }

        // During an active pass publish the candidate only after wlroots
        // sampling ownership has been acquired. Outside a pass it will be
        // acquired by the normal prepass before the next draw. Until this point
        // scene-graph nodes continue to see the old, valid texture.
        auto *candidateRhiTexture = candidateQtTexture.rhiTexture();
        Q_ASSERT(candidateRhiTexture);
        candidateQtTexture.setOwnsTexture(false);
        d->qtTexture.setTexture(candidateRhiTexture);
        d->qtTexture.setHasAlphaChannel(candidateQtTexture.hasAlphaChannel());
        d->qtTexture.setTextureSize(candidateQtTexture.textureSize());
        d->adoptTexture(candidateTexture,
                        candidateOwnsTexture,
                        std::move(candidateBuffer));

        Q_EMIT textureChanged();
        return;
    }

    // Keep the established eager replacement behavior for GLES2 and Pixman.
    d->cleanTexture();
    d->buffer.reset(buffer);
    if (auto clientBuffer = qw_client_buffer::get(*buffer)) {
        // Acquire texture from client buffer. wlroots already generate texture for us if this is a client buffer.
        // By the way, there is something wrong with getting texture from a client buffer using wlr_texture_from_buffer,
        // See: https://gitlab.freedesktop.org/wlroots/wlroots/-/issues/3897
        // Possible patch:  https://gitlab.freedesktop.org/wlroots/wlroots/-/merge_requests/4889
        d->texture = qw_texture::from(clientBuffer->texture());
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
        WVulkanTrace::providerBind(this, d->window, d->buffer.get(), d->texture);
        if (!d->updateRhiTexture(d->texture, d->buffer.get())) {
            d->cleanTexture();
        } else {
            d->rhiTexture = d->qtTexture.rhiTexture();
            d->updateMipmapFiltering();
        }
    }

    Q_EMIT textureChanged();
}

void WSGTextureProvider::setTexture(qw_texture *texture, qw_buffer *srcBuffer)
{
    W_D(WSGTextureProvider);

    if (d->isVulkanRhi() && texture) {
        const char *rejectReason = nullptr;
        auto *clientBuffer = srcBuffer ? qw_client_buffer::get(*srcBuffer) : nullptr;
        if (!srcBuffer)
            rejectReason = "missing-owner";
        else if (!clientBuffer)
            rejectReason = "non-client-owner";
        else if (qw_texture::from(clientBuffer->texture()) != texture)
            rejectReason = "texture-owner-mismatch";

        if (Q_UNLIKELY(rejectReason)) {
            WVulkanTrace::providerReject(this, d->window, srcBuffer, texture,
                                         rejectReason);
            qCWarning(lcWlQtQuickTexture)
                << "Rejected borrowed Vulkan texture without a matching client-buffer owner; keeping the previous texture"
                << "provider" << this
                << "qwTexture" << texture
                << "wlrTexture" << texture->handle()
                << "qwBuffer" << srcBuffer
                << "wlrBuffer" << (srcBuffer ? srcBuffer->handle() : nullptr)
                << "reason" << rejectReason;
            return;
        }

        // Route borrowed Vulkan textures through the owner-driven path so the
        // buffer and texture identity cannot diverge.
        setBuffer(srcBuffer);
        return;
    }

    d->cleanTexture();
    d->texture = texture;
    d->buffer.reset(srcBuffer);
    d->ownsTexture = false;
    if (texture) {
        WVulkanTrace::providerBind(this, d->window, d->buffer.get(), d->texture);
        if (!d->updateRhiTexture(d->texture, d->buffer.get())) {
            d->cleanTexture();
        } else {
            d->rhiTexture = d->qtTexture.rhiTexture();
            d->updateMipmapFiltering();
        }
    }

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
