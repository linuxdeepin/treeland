// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "private/wglobal_p.h"
#include "wsubsurface.h"

#include <QPointer>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WSubsurfacePrivate : public WObjectPrivate
{
public:
    WSubsurfacePrivate(WSubsurface *q,
                       WSubsurface::Type type,
                       WSurface *parentSurface,
                       WSurface *surface);

    W_DECLARE_PUBLIC(WSubsurface)

    WSubsurface::Type type;
    WSubsurface::Place place = WSubsurface::Place::Above;
    QPointF position;
    bool mapped = false;
    QPointer<WSurface> parentSurface;
    QPointer<WSurface> surface;
};

WAYLIB_SERVER_END_NAMESPACE
