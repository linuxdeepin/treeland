// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct decoration_server_state {
    int valid;
    double radius;

    double shadow_blur;
    double shadow_offset_x;
    double shadow_offset_y;
    int shadow_r;
    int shadow_g;
    int shadow_b;
    int shadow_a;
    int shadow_visible;

    double border_width;
    int border_r;
    int border_g;
    int border_b;
    int border_a;
    int border_visible;
};

void decoration_read_server_state(void *data);

#ifdef __cplusplus
}
#endif
