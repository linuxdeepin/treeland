// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <qwglobal.h>

#include <QObject>
#include <QQuickRenderTarget>
#include <QSGRendererInterface>

QT_BEGIN_NAMESPACE
class QQuickRenderControl;
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

    // Opaque value type holding a weak reference to the internal buffer data
    // managed by WRenderHelper. When the underlying buffer data is destroyed
    // (e.g. the buffer is removed), all RenderTarget instances referencing it
    // automatically become null — similar to how QPointer tracks QObject.
    class WAYLIB_SERVER_EXPORT RenderTarget {
    public:
        RenderTarget();
        RenderTarget(const RenderTarget &);
        RenderTarget &operator=(const RenderTarget &);
        ~RenderTarget();

        bool isNull() const;
        QQuickRenderTarget rt() const;
        QW_NAMESPACE::qw_buffer *buffer() const;
        bool colorPreserved() const;

    private:
        friend class WRenderHelper;
        class Private;
        Private *d = nullptr;
    };

    RenderTarget acquireRenderTarget(QQuickRenderControl *rc, QW_NAMESPACE::qw_buffer *buffer,
                                     WGlobal::ColorContentsMode mode = WGlobal::ColorContentsMode::DontCare);
    // For Vulkan render targets: insert a layout barrier before Qt RHI's
    // render pass so the image matches the render pass's initialLayout.
    // No-op for non-Vulkan backends or buffers without a Vulkan dmabuf image.
    void prepareVulkanRenderTarget(QRhiCommandBuffer *cb, const RenderTarget &rt);
    void finishVulkanRenderTarget(QRhiCommandBuffer *cb, const RenderTarget &rt);
    RenderTarget lastRenderTarget() const;
    static QW_NAMESPACE::qw_renderer *createRenderer(QW_NAMESPACE::qw_backend *backend);
    static QW_NAMESPACE::qw_renderer *createRenderer(QW_NAMESPACE::qw_backend *backend, QSGRendererInterface::GraphicsApi api);

    static void setupRendererBackend(QW_NAMESPACE::qw_backend *testBackend = nullptr);
    static QSGRendererInterface::GraphicsApi probe(QW_NAMESPACE::qw_backend *testBackend, const QList<QSGRendererInterface::GraphicsApi> &apiList);

    static bool makeTexture(QRhi *rhi, QW_NAMESPACE::qw_texture *handle, QSGPlainTexture *texture);

    struct TextureEntry {
        wlr_buffer *buffer;
        QW_NAMESPACE::qw_texture *texture;
        QRhiTexture *rhiTexture;
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
