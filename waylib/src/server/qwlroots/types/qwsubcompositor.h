// Copyright (C) 2022 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <wlr/types/wlr_subcompositor.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(subcompositor)
{
    QW_OBJECT
    Q_OBJECT

public:
    QW_FUNC_STATIC(subcompositor, create, qw_subcompositor *, wl_display *display)
};

class QW_CLASS_OBJECT(subsurface)
{
    QW_OBJECT
    Q_OBJECT

public:
    QW_FUNC_STATIC(subsurface, try_from_wlr_surface, qw_subsurface *, wlr_surface *surface)
};

QW_END_NAMESPACE
