// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wayland-server.h>

struct wlr_abc {
    int a, b, c;
    char *name;

    struct {
        struct wl_signal destroy;
        struct wl_signal set_name; // char *
    } events;

    struct wl_listener display_destroy;
};

wlr_abc *wlr_abc_create(wl_display *display);

int wlr_abc_sum(wlr_abc *abc);

void wlr_abc_set_name(wlr_abc *abc, char *name);
