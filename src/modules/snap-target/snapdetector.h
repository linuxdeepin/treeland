// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wtoplevelsurface.h>

#include <QList>
#include <QPointF>
#include <QPointer>
#include <QRectF>

WAYLIB_SERVER_BEGIN_NAMESPACE
class WOutputRenderWindow;
class WSurface;
WAYLIB_SERVER_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE

struct SnapTarget
{
    QRectF rect;
    QPointer<WToplevelSurface> shellSurface;

    bool operator==(const SnapTarget &other) const
    {
        return rect == other.rect && shellSurface == other.shellSurface;
    }
};

class SnapDetector
{
public:
    static QList<SnapTarget> collect(WOutputRenderWindow *renderWindow,
                                     const QList<WSurface *> &excludeSurfaces);
    static SnapTarget hitTest(const QPointF &cursorPos, const QList<SnapTarget> &snapshot);
};
