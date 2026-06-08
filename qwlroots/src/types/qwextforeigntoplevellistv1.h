// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(ext_foreign_toplevel_list_v1)
{
    QW_OBJECT
    Q_OBJECT

public:
    QW_FUNC_STATIC(ext_foreign_toplevel_list_v1, create, qw_ext_foreign_toplevel_list_v1 *, wl_display *display, uint32_t version)
};

class QW_CLASS_OBJECT(ext_foreign_toplevel_handle_v1)
{
    QW_OBJECT
    Q_OBJECT

public:
    QW_FUNC_STATIC(ext_foreign_toplevel_handle_v1, create, qw_ext_foreign_toplevel_handle_v1 *, wlr_ext_foreign_toplevel_list_v1 *list, const wlr_ext_foreign_toplevel_handle_v1_state *state)
    QW_FUNC_STATIC(ext_foreign_toplevel_handle_v1, from_resource, qw_ext_foreign_toplevel_handle_v1 *, wl_resource *resource)

    QW_FUNC_MEMBER(ext_foreign_toplevel_handle_v1, update_state, void, const wlr_ext_foreign_toplevel_handle_v1_state *state)

protected:
    QW_FUNC_MEMBER(ext_foreign_toplevel_handle_v1, destroy, void)
};

QW_END_NAMESPACE
