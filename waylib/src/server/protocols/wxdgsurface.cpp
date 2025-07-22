// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxdgsurface.h"

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

WXdgSurface::WXdgSurface(WToplevelSurfacePrivate &d, QObject *parent)
    : WToplevelSurface(d, parent)
{

}

WXdgSurface::~WXdgSurface()
{

}

WAYLIB_SERVER_END_NAMESPACE
