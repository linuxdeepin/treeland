// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <wlr/types/wlr_text_input_v3.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(text_input_v3)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(enable, wlr_text_input_v3*)
    QW_SIGNAL(commit, wlr_text_input_v3*)
    QW_SIGNAL(disable, wlr_text_input_v3*)

public:
    QW_FUNC_MEMBER(text_input_v3, send_leave, void)
    QW_FUNC_MEMBER(text_input_v3, send_enter, void, wlr_surface *surface)
    QW_FUNC_MEMBER(text_input_v3, send_preedit_string, void, const char *text, int32_t cursor_begin, int32_t cursor_end)
    QW_FUNC_MEMBER(text_input_v3, send_commit_string, void, const char *text)
    QW_FUNC_MEMBER(text_input_v3, send_delete_surrounding_text, void, uint32_t before_length, uint32_t after_length)
    QW_FUNC_MEMBER(text_input_v3, send_done, void)
};

class QW_CLASS_OBJECT(text_input_manager_v3)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(text_input, wlr_text_input_v3*)

public:
    QW_FUNC_STATIC(text_input_manager_v3, create, qw_text_input_manager_v3 *, wl_display *wl_display)
};

QW_END_NAMESPACE
