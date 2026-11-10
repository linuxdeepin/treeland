// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "radiuseffectitem.h"

#include <private/qquickitem_p.h>

namespace Treeland {

class Q_DECL_HIDDEN RadiusEffectItemPrivate : public QQuickItemPrivate
{
    Q_DECLARE_PUBLIC(RadiusEffectItem)

public:
    void updateImplicitAntialiasing()
    {
        setImplicitAntialiasing(radius > 0.0 || topLeftRadius > 0.0
                                || topRightRadius > 0.0 || bottomLeftRadius > 0.0
                                || bottomRightRadius > 0.0);
    }

    QQuickItem *sourceItem = nullptr;
    bool hideSource = false;
    qreal radius = 0.0;
    qreal topLeftRadius = -1.0;
    qreal topRightRadius = -1.0;
    qreal bottomLeftRadius = -1.0;
    qreal bottomRightRadius = -1.0;
    QPointF sourceOffset;
    qreal sourceWidthScale = 1.0;
    qreal sourceHeightScale = 1.0;
    QRectF renderRect;
};

} // namespace Treeland
