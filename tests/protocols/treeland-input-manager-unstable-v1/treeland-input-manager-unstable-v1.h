// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct input_mgr_state {
    uint32_t pointer_changes;
    double pointer_scroll_factor;
    int pointer_handed_mode;
    double pointer_accel_speed;
    uint32_t pointer_accel_profile;
    uint32_t pointer_send_events_mode;
    int pointer_natural_scroll;
    int pointer_disable_while_typing;
    int pointer_tap_to_click;
    uint32_t keyboard_changes;
    int32_t keyboard_repeat_rate;
    int32_t keyboard_repeat_delay;
    int keyboard_num_lock;
};

#ifdef __cplusplus
}
#endif
