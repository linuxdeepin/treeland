// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwglobal.h>
#include <qwobject.h>
extern "C" {
#include <wayland-server.h>
#include <wlr/types/wlr_drm.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(drm)
{
    QW_OBJECT
    Q_OBJECT

public:
    QW_FUNC_STATIC(drm, create, qw_drm *, wl_display *display, wlr_renderer *renderer)
};


class QW_CLASS_REINTERPRET_CAST(drm_buffer)
{
public:
    QW_FUNC_STATIC(drm_buffer, try_from_resource, qw_drm_buffer *, wl_resource *resource)
};

QW_END_NAMESPACE
