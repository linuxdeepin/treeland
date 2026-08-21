// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <QList>
#include <QPointF>
#include <QRectF>

WAYLIB_SERVER_BEGIN_NAMESPACE
class WOutputRenderWindow;
class WSurface;
WAYLIB_SERVER_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE

class SnapDetector
{
public:
    static QList<QRectF> collect(WOutputRenderWindow *renderWindow, WSurface *excludeSurface);
    static QRectF hitTest(const QPointF &cursorPos, const QList<QRectF> &snapshot);
};
