/* SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct input_mgr_state {
    int has_keyboard;
    int has_mouse;
    int has_touchpad;
};

#ifdef __cplusplus
}
#endif
