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
QT_END_NAMESPACE

QW_BEGIN_NAMESPACE
class QWRenderer;
class QWBackend;
class QWBuffer;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WRenderHelperPrivate;
class WAYLIB_SERVER_EXPORT WRenderHelper : public QObject, public WObject
{
    Q_OBJECT
    Q_PROPERTY(QSize size READ size WRITE setSize NOTIFY sizeChanged FINAL)
    W_DECLARE_PRIVATE(WRenderHelper)

public:
    explicit WRenderHelper(QW_NAMESPACE::QWRenderer *renderer, QObject *parent = nullptr);

    QSize size() const;
    void setSize(const QSize &size);

    static QSGRendererInterface::GraphicsApi getGraphicsApi(QQuickRenderControl *rc);
    static QSGRendererInterface::GraphicsApi getGraphicsApi();

    static QW_NAMESPACE::QWBuffer *toBuffer(QW_NAMESPACE::QWRenderer *renderer, QSGTexture *texture);

    QQuickRenderTarget acquireRenderTarget(QQuickRenderControl *rc, QW_NAMESPACE::QWBuffer *buffer);
    std::pair<QW_NAMESPACE::QWBuffer*, QQuickRenderTarget> lastRenderTarget() const;
    static QW_NAMESPACE::QWRenderer *createRenderer(QW_NAMESPACE::QWBackend *backend);

Q_SIGNALS:
    void sizeChanged();

private:
    W_PRIVATE_SLOT(void onBufferDestroy(QWBuffer*))
};

WAYLIB_SERVER_END_NAMESPACE
