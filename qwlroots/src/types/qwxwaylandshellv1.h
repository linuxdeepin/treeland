// Copyright (C) 2023 JiDe Zhang <zhangjide@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <wlr/xwayland/shell.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(xwayland_shell_v1)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(new_surface, wlr_xwayland_surface_v1*)

public:
    QW_FUNC_STATIC(xwayland_shell_v1, create, qw_xwayland_shell_v1 *, wl_display *display, uint32_t version)

    QW_FUNC_MEMBER(xwayland_shell_v1, set_client, void, wl_client *client)
    QW_FUNC_MEMBER(xwayland_shell_v1, surface_from_serial, wlr_surface *, uint64_t serial)
};

QW_END_NAMESPACE
