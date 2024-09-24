// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wlr_abc.h"
#include <wayland-server.h>

static void handle_display_destroy(struct wl_listener *listener, [[maybe_unused]] void *data)
{
    struct wlr_abc *manager =
        wl_container_of(listener, manager, display_destroy);
    wl_signal_emit_mutable(&manager->events.destroy, manager);
    wl_list_remove(&manager->display_destroy.link);
    free(manager);
}

wlr_abc *wlr_abc_create(wl_display *display) {
    wlr_abc *a = (wlr_abc *)calloc(1, sizeof(wlr_abc));
    wl_signal_init(&a->events.destroy);
    wl_signal_init(&a->events.set_name);

    a->display_destroy.notify = handle_display_destroy;
    wl_display_add_destroy_listener(display, &a->display_destroy);
    return a;
}

int wlr_abc_sum(wlr_abc *abc) {
    return abc->a + abc->b + abc->c;
}

void wlr_abc_set_name(wlr_abc *abc, char *name) {
    abc->name = name;
    wl_signal_emit_mutable(&abc->events.set_name, name);
}
