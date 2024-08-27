// Copyright (C) 2024 lbwtw <xiaoyaobing@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "tquickradiuseffect.h"

#include <private/qquickitem_p.h>

#include <QSGTextureProvider>

class Q_DECL_HIDDEN TQuickRadiusEffectPrivate : public QQuickItemPrivate
{
    Q_DECLARE_PUBLIC(TQuickRadiusEffect)

public:
    struct ExtraData {
        ExtraData()
            : topLeftRadius(-1.)
            , topRightRadius(-1.)
            , bottomLeftRadius(-1.)
            , bottomRightRadius(-1.)
        {
        }

        qreal topLeftRadius;
        qreal topRightRadius;
        qreal bottomLeftRadius;
        qreal bottomRightRadius;
    };

    TQuickRadiusEffectPrivate()
        : sourceItem(nullptr)
        , hideSource(false)
        , radius(0)
    {
    }

    ~TQuickRadiusEffectPrivate()
    {
    }

    void maybeSetImplicitAntialiasing();

    QQuickItem *sourceItem;
    uint hideSource : 1;
    QLazilyAllocated<ExtraData> extraRadius;
    qreal radius;
};
