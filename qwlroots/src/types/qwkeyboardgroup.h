// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <wlr/types/wlr_keyboard_group.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(keyboard_group)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(enter, wl_array*)
    QW_SIGNAL(leave, wl_array*)

public:
    QW_FUNC_STATIC(keyboard_group, create, qw_keyboard_group *, void)
    QW_FUNC_STATIC(keyboard_group, from_wlr_keyboard, qw_keyboard_group *, wlr_keyboard *keyboard)

    QW_FUNC_MEMBER(keyboard_group, add_keyboard, bool, wlr_keyboard *keyboard)
    QW_FUNC_MEMBER(keyboard_group, remove_keyboard, void, wlr_keyboard *keyboard)

protected:
    QW_FUNC_MEMBER(keyboard_group, destroy, void)
};

QW_END_NAMESPACE
