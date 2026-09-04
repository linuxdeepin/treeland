/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 */
#ifndef WAYLAND_TEXT_INPUT_V1_TEST_H
#define WAYLAND_TEXT_INPUT_V1_TEST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * E-level readback: the production WTextInputManagerV1 emits newTextInput
 * when the client creates a zwp_text_input_v1.  The captured WTextInputV1's
 * activate() signal must fire when the client calls activate, proving the
 * text-input activation request reached the real compositor text-input
 * pipeline.
 */
struct text_input_v1_server_state {
	int valid; /* a WTextInputV1 was captured */
	int activated; /* activate() signal was emitted */
};

void text_input_v1_read_server_state(void *data);

#ifdef __cplusplus
}
#endif

#endif /* WAYLAND_TEXT_INPUT_V1_TEST_H */
