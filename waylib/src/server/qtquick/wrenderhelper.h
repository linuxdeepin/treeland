// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <qwglobal.h>

#include <QObject>
#include <QQuickRenderTarget>
#include <QSGRendererInterface>

#include <memory>

QT_BEGIN_NAMESPACE
class QQuickRenderControl;
class QSGTexture;
class QSGPlainTexture;
class QRhi;
class QRhiRenderTarget;
class QRhiTexture;
QT_END_NAMESPACE

QW_BEGIN_NAMESPACE
class qw_renderer;
class qw_allocator;
class qw_backend;
class qw_buffer;
class qw_texture;
QW_END_NAMESPACE

struct wlr_buffer;
struct wlr_texture;

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
    QQuickRenderTarget preserveRenderTarget(QW_NAMESPACE::qw_buffer *buffer) const;
    bool acquireRenderBuffer(QQuickRenderControl *rc, QW_NAMESPACE::qw_buffer *buffer, const char *purpose);
    bool releaseRenderBuffer(QQuickRenderControl *rc, QW_NAMESPACE::qw_buffer *buffer, QRhiTexture *renderTargetTexture, const char *purpose);
    void cleanupRetiredRenderResources(bool force = false);
    std::pair<QW_NAMESPACE::qw_buffer*, QQuickRenderTarget> lastRenderTarget() const;
    static QW_NAMESPACE::qw_renderer *createRenderer(QW_NAMESPACE::qw_backend *backend);
    static QW_NAMESPACE::qw_renderer *createRenderer(QW_NAMESPACE::qw_backend *backend, QSGRendererInterface::GraphicsApi api);

    static void setupRendererBackend(QW_NAMESPACE::qw_backend *testBackend = nullptr);
    static QSGRendererInterface::GraphicsApi probe(QW_NAMESPACE::qw_backend *testBackend, const QList<QSGRendererInterface::GraphicsApi> &apiList);

    static bool makeTexture(QRhi *rhi, QW_NAMESPACE::qw_texture *handle, QSGPlainTexture *texture,
                            bool forceVulkanShaderReadOnlyLayout = false);
    static bool prepareTextureForSampling(QQuickRenderControl *rc,
                                          QW_NAMESPACE::qw_renderer *renderer,
                                          QW_NAMESPACE::qw_texture *texture,
                                          const char *purpose);
    static bool finishTextureSampling(QQuickRenderControl *rc,
                                      QW_NAMESPACE::qw_renderer *renderer,
                                      QW_NAMESPACE::qw_texture *texture,
                                      const char *purpose);

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
