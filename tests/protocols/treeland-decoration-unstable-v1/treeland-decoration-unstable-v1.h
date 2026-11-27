// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct decoration_server_state {
    int valid;
    double radius;
    double frame_radius;
    double presentation_radius;
    int shadow_visible;
    int border_visible;
    double shadow_radius;
    double border_width;
    // Maximized frame rect and the SSD title bar height currently taken out of it.
    int32_t maximized_width;
    int32_t maximized_height;
    int32_t titlebar_height;
};

struct decoration_effect_switches {
    int corner_radius;
    int shadow;
    int border;
};

void decoration_read_server_state(void *data);
void decoration_set_effect_switches(void *data);
void decoration_set_effect_level(void *data);

#ifdef __cplusplus
}
#endif
