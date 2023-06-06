// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <qwglobal.h>

#include <QObject>
#include <QQuickRenderTarget>

QT_BEGIN_NAMESPACE
class QOpenGLContext;
class QWindow;
QT_END_NAMESPACE

QW_BEGIN_NAMESPACE
class QWRenderer;
class QWBackend;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutput;
class WOutputHelperPrivate;
class WAYLIB_SERVER_EXPORT WOutputHelper : public QObject, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WOutputHelper)
    Q_PROPERTY(bool renderable READ renderable NOTIFY renderableChanged)
    Q_PROPERTY(bool contentIsDirty READ contentIsDirty NOTIFY contentIsDirtyChanged)

public:
    explicit WOutputHelper(WOutput *output, QObject *parent = nullptr);

    WOutput *output() const;
    QWindow *outputWindow() const;

    QQuickRenderTarget makeRenderTarget();
    static QW_NAMESPACE::QWRenderer *createRenderer(QW_NAMESPACE::QWBackend *backend);

    bool renderable() const;
    bool contentIsDirty() const;

    bool makeCurrent(QOpenGLContext *context);
    void doneCurrent(QOpenGLContext *context);

    void resetState();
    void update();

Q_SIGNALS:
    void requestRender();
    void renderableChanged();
    void contentIsDirtyChanged();
};

WAYLIB_SERVER_END_NAMESPACE
