// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "woutputviewport.h"
#include "woutputrenderwindow.h"
#include "wtexture.h"
#include "wbufferrenderer_p.h"

#include <qwoutput.h>
#include <qwtexture.h>

#include <QQuickTextureFactory>
#include <private/qquickitem_p.h>

extern "C" {
#define static
#include <wlr/types/wlr_output.h>
#undef static
}

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class OutputTextureProvider;
class WOutputViewportPrivate : public QQuickItemPrivate
{
public:
    WOutputViewportPrivate()
        : offscreen(false)
        , preserveColorContents(false)
        , live(true)
        , forceRender(false)
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
    void invalidateSceneGraph();

    qreal getImplicitWidth() const override;
    qreal getImplicitHeight() const override;

    void updateImplicitSize();
    void updateRenderBufferSource();
    void setExtraRenderSource(QQuickItem *source);

    W_DECLARE_PUBLIC(WOutputViewport)
    QQuickItem *input = nullptr;
    WOutput *output = nullptr;
    qreal devicePixelRatio = 1.0;
    WBufferRenderer *bufferRenderer = nullptr;
    QPointer<QQuickItem> extraRenderSource;

    uint offscreen:1;
    uint preserveColorContents:1;
    uint live:1;
    uint forceRender:1;
    WOutputViewport::LayerFlags layerFlags;
};

WAYLIB_SERVER_END_NAMESPACE
