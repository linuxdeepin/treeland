// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include "wtoplevelsurface.h"
#include "wglobal_p.h"

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WToplevelSurfacePrivate : public WWrapObjectPrivate
{
public:
    inline WToplevelSurfacePrivate(WToplevelSurface *q)
        : WWrapObjectPrivate(q) {}
};

WAYLIB_SERVER_END_NAMESPACE
