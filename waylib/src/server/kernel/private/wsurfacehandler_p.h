// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wsurfacehandler.h"
#include "wglobal_p.h"

#include <QPointF>
#include <QHash>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSurface;
class Q_DECL_HIDDEN WSurfaceHandlerPrivate : public WObjectPrivate
{
public:
    WSurfaceHandlerPrivate(WSurface *surface, WSurfaceHandler *qq)
        : WObjectPrivate(qq)
        , surface(surface)
    {}

    W_DECLARE_PUBLIC(WSurfaceHandler)
    WSurface *surface;
};

WAYLIB_SERVER_END_NAMESPACE
