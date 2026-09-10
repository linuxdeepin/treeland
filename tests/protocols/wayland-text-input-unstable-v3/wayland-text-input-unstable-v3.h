/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 */
#ifndef WAYLAND_TEXT_INPUT_V3_TEST_H
#define WAYLAND_TEXT_INPUT_V3_TEST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * E-level readback: the production WTextInputManagerV3 emits newTextInput
 * when the client creates a zwp_text_input_v3.  The captured WTextInputV3's
 * wlroots handle exposes current_enabled, which must be true after the client
 * calls enable + commit, proving the text-input request reached the real
 * compositor text-input pipeline.
 */
struct text_input_v3_server_state {
	int valid; /* a WTextInputV3 was captured */
	int enabled; /* handle()->current_enabled */
};

void text_input_v3_read_server_state(void *data);

#ifdef __cplusplus
}
#endif

#endif /* WAYLAND_TEXT_INPUT_V3_TEST_H */
