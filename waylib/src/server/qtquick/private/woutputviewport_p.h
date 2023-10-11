// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "woutputviewport.h"
#include "woutputrenderwindow.h"
#include "wtexture.h"

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
        , cacheBuffer(false)
    {

    }
    ~WOutputViewportPrivate() {

    }

    inline WOutputRenderWindow *outputWindow() const {
        auto ow = qobject_cast<WOutputRenderWindow*>(window);
        Q_ASSERT(ow);
        return ow;
    }

    void initForOutput();
    void invalidateSceneGraph();

    qreal getImplicitWidth() const override;
    qreal getImplicitHeight() const override;

    void updateImplicitSize();

    W_DECLARE_PUBLIC(WOutputViewport)
    WOutput *output = nullptr;
    qreal devicePixelRatio = 1.0;
    uint offscreen:1;
    uint root:1;
    uint cacheBuffer:1;

    std::unique_ptr<OutputTextureProvider> textureProvider;
};

WAYLIB_SERVER_END_NAMESPACE
