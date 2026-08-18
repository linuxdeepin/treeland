// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxdgsurface.h"

WAYLIB_SERVER_BEGIN_NAMESPACE

WXdgSurface::WXdgSurface(WToplevelSurfacePrivate &d)
    : WToplevelSurface(d)
{

}

WXdgSurface::~WXdgSurface()
{

}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_wxdgsurface.cpp"
