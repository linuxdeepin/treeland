// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <wlr/types/wlr_keyboard_shortcuts_inhibit_v1.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(keyboard_shortcuts_inhibitor_v1)
{
    QW_OBJECT
    Q_OBJECT

public:
    QW_FUNC_MEMBER(keyboard_shortcuts_inhibitor_v1, activate, void)
    QW_FUNC_MEMBER(keyboard_shortcuts_inhibitor_v1, deactivate, void)
};

class QW_CLASS_OBJECT(keyboard_shortcuts_inhibit_manager_v1)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(new_inhibitor, qw_keyboard_shortcuts_inhibitor_v1*)

public:
    QW_FUNC_STATIC(keyboard_shortcuts_inhibit_v1, create, qw_keyboard_shortcuts_inhibit_manager_v1 *, wl_display *display)
};

QW_END_NAMESPACE

