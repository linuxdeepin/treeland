// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

QT_BEGIN_NAMESPACE
class QPoint;
class QPointF;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSeat;
class WSurface;
class WSurfaceHandlerPrivate;
class WAYLIB_SERVER_EXPORT WSurfaceHandler : public WObject
{
    W_DECLARE_PRIVATE(WSurfaceHandler)
public:
    WSurfaceHandler(WSurface *surface);

    virtual QPointF position() const = 0;
    virtual QPointF mapFromGlobal(const QPointF &localPos);
    virtual QPointF mapToGlobal(const QPointF &pos);

protected:
    WSurfaceHandler(WSurfaceHandlerPrivate &dd);
};

WAYLIB_SERVER_END_NAMESPACE
