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
QT_END_NAMESPACE

QW_BEGIN_NAMESPACE
class qw_renderer;
class qw_backend;
class qw_buffer;
class qw_texture;
QW_END_NAMESPACE

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
    std::pair<QW_NAMESPACE::qw_buffer*, QQuickRenderTarget> lastRenderTarget() const;
    static QW_NAMESPACE::qw_renderer *createRenderer(QW_NAMESPACE::qw_backend *backend);
    static QW_NAMESPACE::qw_renderer *createRenderer(QW_NAMESPACE::qw_backend *backend, QSGRendererInterface::GraphicsApi api);

    static void setupRendererBackend(QW_NAMESPACE::qw_backend *testBackend = nullptr);
    static QSGRendererInterface::GraphicsApi probe(QW_NAMESPACE::qw_backend *testBackend, const QList<QSGRendererInterface::GraphicsApi> &apiList);

    static bool makeTexture(QRhi *rhi, QW_NAMESPACE::qw_texture *handle, QSGPlainTexture *texture);

Q_SIGNALS:
    void sizeChanged();

private:
    W_PRIVATE_SLOT(void onBufferDestroy())
};

WAYLIB_SERVER_END_NAMESPACE
