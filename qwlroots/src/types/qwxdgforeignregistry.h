// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <wlr/types/wlr_xdg_foreign_registry.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(xdg_foreign_registry)
{
    QW_OBJECT
    Q_OBJECT

public:
    QW_FUNC_STATIC(xdg_foreign_registry, create, qw_xdg_foreign_registry *, wl_display *display)

    QW_FUNC_MEMBER(xdg_foreign_registry, find_by_handle, wlr_xdg_foreign_exported *, const char *handle)
};

class QW_CLASS_OBJECT(xdg_foreign_exported)
{
    QW_OBJECT
    Q_OBJECT

public:
    QW_FUNC_MEMBER(xdg_foreign_exported, init, bool, wlr_xdg_foreign_registry *registry)
    QW_FUNC_MEMBER(xdg_foreign_exported, finish, void)
};

QW_END_NAMESPACE
