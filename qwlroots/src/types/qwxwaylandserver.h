// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <wlr/xwayland/server.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(xwayland_server)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(start)
    QW_SIGNAL(ready)

public:
    QW_FUNC_STATIC(xwayland_server, create, qw_xwayland_server *, wl_display *display, wlr_xwayland_server_options *options)
};

QW_END_NAMESPACE
