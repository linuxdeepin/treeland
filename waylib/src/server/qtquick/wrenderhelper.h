// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wlr_fwd.h>
#include <wglobal.h>

#include <QObject>
#include <QQuickRenderTarget>
#include <QSGRendererInterface>
#include <QString>

QT_BEGIN_NAMESPACE
class QQuickRenderControl;
class QSGTexture;
class QSGPlainTexture;
class QRhi;
class QRhiTexture;
class QRhiCommandBuffer;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WRenderHelperPrivate;
class WAYLIB_SERVER_EXPORT WRenderHelper : public QObject, public WObject
{
    Q_OBJECT
    Q_PROPERTY(QSize size READ size WRITE setSize NOTIFY sizeChanged FINAL)
    W_DECLARE_PRIVATE(WRenderHelper)

public:
    explicit WRenderHelper(wlr_renderer *renderer, QObject *parent = nullptr);

    QSize size() const;
    void setSize(const QSize &size);

    static QSGRendererInterface::GraphicsApi getGraphicsApi(QQuickRenderControl *rc);
    static QSGRendererInterface::GraphicsApi getGraphicsApi();

    static wlr_buffer *toBuffer(wlr_renderer *renderer, QSGTexture *texture, QSGRendererInterface::GraphicsApi api);

    // Opaque handle to an internal BufferData. Becomes null when the buffer
    // is destroyed (similar to QPointer).
    class WAYLIB_SERVER_EXPORT RenderTarget {
    public:
        RenderTarget();
        RenderTarget(const RenderTarget &);
        RenderTarget &operator=(const RenderTarget &);
        ~RenderTarget();

        bool isNull() const;
        QQuickRenderTarget rt() const;
        wlr_buffer *buffer() const;
        bool colorPreserved() const;

    private:
        friend class WRenderHelper;
        class Private;
        Private *d = nullptr;
    };

    RenderTarget acquireRenderTarget(QQuickRenderControl *rc, wlr_buffer *buffer,
                                     WGlobal::ColorContentsMode mode = WGlobal::ColorContentsMode::DontCare);
    RenderTarget lastRenderTarget() const;

#ifdef ENABLE_VULKAN_RENDER
    // Record wlroots render_buffer FOREIGN acquire/release around Qt RHI pass.
    void prepareVulkanRenderTarget(QRhiCommandBuffer *cb, const RenderTarget &rt);
    void finishVulkanRenderTarget(QRhiCommandBuffer *cb, const RenderTarget &rt);
#endif
    static wlr_renderer *createRenderer(wlr_backend *backend);
    static wlr_renderer *createRenderer(wlr_backend *backend, QSGRendererInterface::GraphicsApi api);

    static void setupRendererBackend(wlr_backend *testBackend = nullptr);
    static QSGRendererInterface::GraphicsApi probe(wlr_backend *testBackend, const QList<QSGRendererInterface::GraphicsApi> &apiList);

    static bool makeTexture(QRhi *rhi, wlr_texture *handle, QSGPlainTexture *texture);

    struct TextureEntry {
        wlr_buffer *buffer;
        wlr_texture *texture;
        QRhiTexture *rhiTexture;
    };
    static TextureEntry newTexture(wlr_allocator *allocator,
                                   wlr_renderer *renderer,
                                   uint32_t drmFormat, uint64_t drmModifier,
                                   QRhi *rhi, const QSize &size,
                                   int rhiFormat, int rhiFlags);
    static TextureEntry newTextureLike(wlr_allocator *allocator,
                                       wlr_renderer *renderer,
                                       QRhiTexture *texture, QRhi *rhi, int rhiFlags);
    static wlr_buffer *lookupBuffer(const QRhiRenderTarget *rt);
    static wlr_buffer *lookupBuffer(const QRhiTexture *texture);

    // Runtime control for WAYLIB_DEBUG_DAMAGE: none / rerender / highlight / log.
    static QString damageDebugMode();
    static bool setDamageDebugMode(const QString &mode);

Q_SIGNALS:
    void sizeChanged();
};

WAYLIB_SERVER_END_NAMESPACE
