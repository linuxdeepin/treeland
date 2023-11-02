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
        , root(false)
    {

    }
    ~WOutputViewportPrivate() {

    }

    inline WOutputRenderWindow *outputWindow() const {
        auto ow = qobject_cast<WOutputRenderWindow*>(window);
        Q_ASSERT(ow);
        return ow;
    }

    inline static WOutputViewportPrivate *get(WOutputViewport *viewport) {
        return viewport->d_func();
    }

    void init();
    void initForOutput();
    void invalidateSceneGraph();

    qreal getImplicitWidth() const override;
    qreal getImplicitHeight() const override;

    void updateImplicitSize();

    W_DECLARE_PUBLIC(WOutputViewport)
    WOutput *output = nullptr;
    qreal devicePixelRatio = 1.0;
    WBufferRenderer *renderBuffer = nullptr;

    uint offscreen:1;
    uint root:1;
    WOutputViewport::LayerFlags layerFlags;
};

WAYLIB_SERVER_END_NAMESPACE
