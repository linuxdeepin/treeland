// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wtoplevelsurface.h"
#include "private/wwaylandresource_p.h"
#include "private/wtoplevelsurface_p.h"

WAYLIB_SERVER_BEGIN_NAMESPACE

WToplevelSurface::WToplevelSurface(WToplevelSurfacePrivate &d)
    : QObject(nullptr)
    , WWaylandResource(d, nullptr)
{

}

WAYLIB_SERVER_END_NAMESPACE

// moc_wtoplevelsurface.cpp is intentionally kept as a separate translation
// unit (compiled via mocs_compilation.cpp) instead of being #included here:
// wtoplevelsurface.h only forward-declares WSeat, which is used in signals, so
// the moc needs a complete WSeat definition supplied by moc_wseat.cpp (also kept
// separate) being compiled earlier in mocs_compilation.cpp. See WM-292.
