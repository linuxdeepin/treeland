/* SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only */
#pragma once

#include "protocol-test-client.h"
#include "protocol-test-xdg-client.h"

struct dde_desktop_state {
    int output_ready;
    int wrapper_created;
    int wrapper_in_workspace;
    int is_dde_shell_surface;
    int role_overlay;
    int position_x;
    int position_y;
    unsigned int auto_placement;
    int skip_switcher;
    int skip_dock_preview;
    int skip_multitask_view;
    int accept_keyboard_focus;
};
