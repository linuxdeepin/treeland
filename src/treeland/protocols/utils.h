// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wayland-server-core.h>
#include <wayland-server.h>

static void resource_handle_destroy(struct wl_client *, struct wl_resource *resource)
{
    wl_list_remove(&resource->link);
    wl_resource_destroy(resource);
}

template<typename T>
static void context_destroy(T *context)
{
    if (context == nullptr) {
        return;
    }

    wl_list_remove(&context->link);
    free(context);
}
