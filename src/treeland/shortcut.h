#pragma once

#include <cassert>
#include <shortcut-server-protocol.h>

class Helper;
namespace TreeLand {
class TreeLand;
}

#define TREELAND_SHORTCUT_MANAGER_V1_VERSION 1

struct treeland_shortcut_manager_v1 {
    struct wl_global *global;

    struct {
        struct wl_signal shortcut;
        struct wl_signal destroy;
    } events;

    void *data;

    struct wl_list contexts; // link to treeland_shortcut_context_v1.link

    wl_display *display;
    struct wl_listener display_destroy;

    TreeLand::TreeLand *treeland;
    Helper *helper;
};

struct treeland_shortcut_context_v1_state {
    int32_t key_code;
    int32_t modify;
};

struct treeland_shortcut_manager_v1 *treeland_shortcut_manager_v1_create(struct wl_display *display, TreeLand::TreeLand *treeland, Helper *helper);
