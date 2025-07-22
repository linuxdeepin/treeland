// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wtoplevelsurface.h"
#include "private/wtoplevelsurface_p.h"

WAYLIB_SERVER_BEGIN_NAMESPACE

WToplevelSurface::WToplevelSurface(WToplevelSurfacePrivate &d, QObject *parent)
    : WWrapObject(d, parent)
{

}

WAYLIB_SERVER_END_NAMESPACE
