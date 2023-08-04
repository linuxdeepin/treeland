#pragma once

#include <cassert>
#include <server-protocol.h>

namespace TreeLand {
class TreeLand;
}

#define TREELAND_SOCKET_MANAGER_V1_VERSION 1

struct treeland_socket_manager_v1 {
    struct wl_global *global;

    struct {
        struct wl_signal new_user_socket;
        struct wl_signal destroy;
    } events;

    void *data;

    struct wl_list contexts; // link to treeland_socket_context_v1.link

    wl_display *display;
    struct wl_listener display_destroy;

    TreeLand::TreeLand *treeland;
};

struct treeland_socket_context_v1_state {
    char *username;
    int32_t fd;
};

struct treeland_socket_manager_v1 *treeland_socket_manager_v1_create(struct wl_display *display, TreeLand::TreeLand *treeland);
