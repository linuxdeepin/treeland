// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <qwglobal.h>

#include <QObject>
#include <QQuickRenderTarget>
#include <QSGRendererInterface>

#include <cstdint>

QT_BEGIN_NAMESPACE
class QQuickRenderControl;
class QRhiTexture;
class QSGTexture;
class QSGPlainTexture;
class QRhi;
class QRhiCommandBuffer;
QT_END_NAMESPACE

QW_BEGIN_NAMESPACE
class qw_renderer;
class qw_allocator;
class qw_backend;
class qw_buffer;
class qw_texture;
QW_END_NAMESPACE

struct wlr_buffer;
struct wlr_surface;

WAYLIB_SERVER_BEGIN_NAMESPACE

class WRenderHelperPrivate;
class WAYLIB_SERVER_EXPORT WRenderHelper : public QObject, public WObject
{
    Q_OBJECT
    Q_PROPERTY(QSize size READ size WRITE setSize NOTIFY sizeChanged FINAL)
    W_DECLARE_PRIVATE(WRenderHelper)

public:
    explicit WRenderHelper(QW_NAMESPACE::qw_renderer *renderer, QObject *parent = nullptr);

    QSize size() const;
    void setSize(const QSize &size);

    static QSGRendererInterface::GraphicsApi getGraphicsApi(QQuickRenderControl *rc);
    static QSGRendererInterface::GraphicsApi getGraphicsApi();

    static QW_NAMESPACE::qw_buffer *toBuffer(QW_NAMESPACE::qw_renderer *renderer, QSGTexture *texture, QSGRendererInterface::GraphicsApi api);

    QQuickRenderTarget acquireRenderTarget(QQuickRenderControl *rc, QW_NAMESPACE::qw_buffer *buffer);
    QQuickRenderTarget renderTargetForBuffer(QW_NAMESPACE::qw_buffer *buffer,
                                             bool preserveColorContents) const;
    std::pair<QW_NAMESPACE::qw_buffer*, QQuickRenderTarget> lastRenderTarget() const;
    bool prepareVulkanRenderTargetForQt(QRhi *rhi, QRhiTexture *texture,
                                        QW_NAMESPACE::qw_buffer *buffer);
    bool releaseVulkanRenderTargetToScanout(QRhi *rhi, QRhiTexture *texture,
                                            QW_NAMESPACE::qw_buffer *buffer);
    static QW_NAMESPACE::qw_renderer *createRenderer(QW_NAMESPACE::qw_backend *backend);
    static QW_NAMESPACE::qw_renderer *createRenderer(QW_NAMESPACE::qw_backend *backend, QSGRendererInterface::GraphicsApi api);

    static void setupRendererBackend(QW_NAMESPACE::qw_backend *testBackend = nullptr);
    static QSGRendererInterface::GraphicsApi probe(QW_NAMESPACE::qw_backend *testBackend, const QList<QSGRendererInterface::GraphicsApi> &apiList);

    struct NativeTextureCleanup {
        enum class Type {
            None,
            OpenGLTexture,
            VulkanTexture,
            VulkanRenderTarget,
        };

        Type type = Type::None;
        quint64 texture = 0;
        void *eglImage = nullptr;
        void *eglDisplay = nullptr;
        void *nativeData = nullptr;
    };

    static void releaseNativeTexture(NativeTextureCleanup *cleanup);

    struct ImportedVulkanTexture {
        QRhiTexture *texture = nullptr;
        NativeTextureCleanup nativeCleanup;
        QSize size;
        uint32_t drmFormat = 0;
        uint64_t drmModifier = 0;
        bool hasAlpha = false;

        bool isValid() const { return texture && !size.isEmpty(); }
    };

    static bool importVulkanTextureFromBuffer(QRhi *rhi, QW_NAMESPACE::qw_buffer *buffer,
                                              wlr_surface *surface,
                                              ImportedVulkanTexture *importedTexture);
    static bool acquireImportedVulkanTextureFromBuffer(QRhi *rhi,
                                                       QW_NAMESPACE::qw_buffer *buffer,
                                                       wlr_surface *surface,
                                                       NativeTextureCleanup *nativeCleanup);
    static void releaseImportedVulkanTexture(ImportedVulkanTexture *importedTexture);

    static bool makeTexture(QRhi *rhi, QW_NAMESPACE::qw_texture *handle,
                            QSGPlainTexture *texture, QW_NAMESPACE::qw_buffer *buffer = nullptr,
                            NativeTextureCleanup *nativeCleanup = nullptr,
                            bool allowBufferDirectImport = true,
                            wlr_surface *surface = nullptr,
                            QRhiCommandBuffer *commandBuffer = nullptr);
    static bool makeOpenGLTextureFromBuffer(QRhi *rhi, QW_NAMESPACE::qw_buffer *buffer,
                                            QSGPlainTexture *texture,
                                            NativeTextureCleanup *nativeCleanup);
    static bool makeVulkanTextureFromBuffer(QRhi *rhi, QW_NAMESPACE::qw_buffer *buffer,
                                            QSGPlainTexture *texture,
                                            NativeTextureCleanup *nativeCleanup,
                                            wlr_surface *surface = nullptr);

    // Legacy explicit-sync experiment kept for compatibility with older local
    // tests. The active Qt RHI Vulkan output path uses
    // prepareVulkanRenderTargetForQt() and releaseVulkanRenderTargetToScanout()
    // so layout and external queue-family ownership stay tracked per buffer.
    static void transitionVkImageToGeneral(QRhi *rhi, QRhiTexture *texture,
                                           QW_NAMESPACE::qw_buffer *buffer);

    struct TextureEntry {
        wlr_buffer *buffer;
        QW_NAMESPACE::qw_texture *texture;
        QRhiTexture *rhiTexture;
        NativeTextureCleanup nativeCleanup;
    };
    static TextureEntry newTexture(QW_NAMESPACE::qw_allocator *allocator,
                                   QW_NAMESPACE::qw_renderer *renderer,
                                   uint32_t drmFormat, uint64_t drmModifier,
                                   QRhi *rhi, const QSize &size,
                                   int rhiFormat, int rhiFlags);
    static TextureEntry newTextureLike(QW_NAMESPACE::qw_allocator *allocator,
                                       QW_NAMESPACE::qw_renderer *renderer,
                                       QRhiTexture *texture, QRhi *rhi, int rhiFlags);
    static QW_NAMESPACE::qw_buffer *lookupBuffer(const QRhiRenderTarget *rt);
    static QW_NAMESPACE::qw_buffer *lookupBuffer(const QRhiTexture *texture);

Q_SIGNALS:
    void sizeChanged();

private:
    W_PRIVATE_SLOT(void onBufferDestroy())
};

WAYLIB_SERVER_END_NAMESPACE
