// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "woutputviewport.h"
#include "woutputrenderwindow.h"
#include "wbufferrenderer_p.h"

#include <qwoutput.h>
#include <qwtexture.h>

#include <QQuickTextureFactory>
#include <private/qquickitem_p.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class OutputTextureProvider;
class Q_DECL_HIDDEN WOutputViewportPrivate : public QQuickItemPrivate
{
public:
    WOutputViewportPrivate()
        : attached(false)
        , offscreen(false)
        , preserveColorContents(false)
        , live(true)
        , forceRender(false)
        , ignoreViewport(false)
        , disableHardwareLayers(false)
        , ignoreSoftwareLayers(false)
    {

    }
    ~WOutputViewportPrivate() {

    }

    inline WOutputRenderWindow *outputWindow() const {
        return static_cast<WOutputRenderWindow*>(window);
    }

    inline static WOutputViewportPrivate *get(WOutputViewport *viewport) {
        return viewport->d_func();
    }

    inline bool renderable() const {
        return (forceRender || live) && q_func()->isVisible();
    }

    void init();
    void initForOutput();
    void update();

    // call in WOutputRenderWindow
    inline void notifyLayersChanged() {
        Q_EMIT q_func()->layersChanged();
    }
    inline void notifyHardwareLayersChanged() {
        if (disableHardwareLayers)
            return;
        Q_EMIT q_func()->hardwareLayersChanged();
    }

    qreal calculateImplicitWidth() const;
    qreal calculateImplicitHeight() const;

    void updateImplicitSize();
    void updateRenderBufferSource();
    void setExtraRenderSource(QQuickItem *source);

    W_DECLARE_PUBLIC(WOutputViewport)
    QList<WOutputViewport*> depends;

    QQuickItem *input = nullptr;
    WOutput *output = nullptr;
    QQuickTransform *viewportTransform = nullptr;

    qreal devicePixelRatio = 1.0;
    WBufferRenderer *bufferRenderer = nullptr;
    QPointer<QQuickItem> extraRenderSource;
    QRectF sourceRect;
    QRectF targetRect;

    uint attached:1;
    uint offscreen:1;
    uint preserveColorContents:1;
    uint live:1;
    uint forceRender:1;
    uint ignoreViewport:1;
    uint disableHardwareLayers:1;
    uint ignoreSoftwareLayers:1;
};

WAYLIB_SERVER_END_NAMESPACE
