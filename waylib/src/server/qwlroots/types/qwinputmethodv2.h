// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#define delete delete_c
#include <wlr/types/wlr_input_method_v2.h>
#undef delete
#include <wlr/util/box.h>
}
QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(input_method_manager_v2)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(input_method, wlr_input_method_v2*)

public:
    QW_FUNC_STATIC(input_method_manager_v2, create, qw_input_method_manager_v2 *, wl_display *display)
};

class QW_CLASS_OBJECT(input_method_keyboard_grab_v2)
{
    QW_OBJECT
    Q_OBJECT

public:
    QW_FUNC_MEMBER(input_method_keyboard_grab_v2, send_key, void, uint32_t time, uint32_t key, uint32_t state)
    QW_FUNC_MEMBER(input_method_keyboard_grab_v2, send_modifiers, void, wlr_keyboard_modifiers *modifiers)
    QW_FUNC_MEMBER(input_method_keyboard_grab_v2, set_keyboard, void, wlr_keyboard *keyboard)

protected:
    QW_FUNC_MEMBER(input_method_keyboard_grab_v2, destroy, void)
};

class QW_CLASS_OBJECT(input_popup_surface_v2)
{
    QW_OBJECT
    Q_OBJECT

public:
    QW_FUNC_MEMBER(input_popup_surface_v2, send_text_input_rectangle, void, wlr_box *sbox)
    QW_FUNC_STATIC(input_popup_surface_v2, try_from_wlr_surface, qw_input_popup_surface_v2 *, wlr_surface *surface)
};

class QW_CLASS_OBJECT(input_method_v2)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(commit, wlr_input_method_v2*)
    QW_SIGNAL(new_popup_surface, wlr_input_popup_surface_v2*)
    QW_SIGNAL(grab_keyboard, wlr_input_method_keyboard_grab_v2*)

public:
    QW_FUNC_MEMBER(input_method_v2, send_activate, void)
    QW_FUNC_MEMBER(input_method_v2, send_content_type, void, uint32_t hint, uint32_t purpose)
    QW_FUNC_MEMBER(input_method_v2, send_deactivate, void)
    QW_FUNC_MEMBER(input_method_v2, send_done, void)
    QW_FUNC_MEMBER(input_method_v2, send_surrounding_text, void, const char *text, uint32_t cursor, uint32_t anchor)
    QW_FUNC_MEMBER(input_method_v2, send_text_change_cause, void, uint32_t cause)
    QW_FUNC_MEMBER(input_method_v2, send_unavailable, void)
};

QW_END_NAMESPACE
