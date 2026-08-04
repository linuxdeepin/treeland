// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <QObject>
#include <QQuickRenderTarget>
#include <QSGRendererInterface>

QT_BEGIN_NAMESPACE
class QQuickRenderControl;
class QSGTexture;
class QSGPlainTexture;
class QRhi;
QT_END_NAMESPACE

struct wlr_buffer;
struct wlr_texture;
struct wlr_renderer;
struct wlr_allocator;
struct wlr_backend;

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

    QQuickRenderTarget acquireRenderTarget(QQuickRenderControl *rc, wlr_buffer *buffer);
    std::pair<wlr_buffer*, QQuickRenderTarget> lastRenderTarget() const;
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

Q_SIGNALS:
    void sizeChanged();

private:
    W_PRIVATE_SLOT(void onBufferDestroy())
};

WAYLIB_SERVER_END_NAMESPACE
