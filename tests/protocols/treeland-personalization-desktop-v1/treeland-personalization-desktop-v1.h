/* SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only */
#pragma once

#include "protocol-test-client.h"
#include "protocol-test-xdg-client.h"

struct personalization_desktop_state {
    int output_ready;
    int wrapper_created;
    int wrapper_in_workspace;
    int personalization_attached;
    int background_type;
    int corner_radius;
    int blur;
    int no_titlebar;
    int wrapper_no_titlebar;
    int shadow_radius;
    int shadow_offset_x;
    int shadow_offset_y;
    int shadow_red;
    int shadow_green;
    int shadow_blue;
    int shadow_alpha;
    int border_width;
    int border_red;
    int border_green;
    int border_blue;
    int border_alpha;
};
