/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 */
#ifndef WAYLAND_POINTER_GESTURES_TEST_H
#define WAYLAND_POINTER_GESTURES_TEST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * E-level readback: the client creates zwp_pointer_gesture_swipe_v1 and
 * zwp_pointer_gesture_pinch_v1 resources on a real wl_pointer.  When the
 * client binds zwp_pointer_gestures_v1, the resource's user_data is the
 * real wlr_pointer_gestures_v1 handle.  The test locates this handle by
 * iterating the test client's bound resources, then reads the real
 * wlr_pointer_gestures_v1::swipes and ::pinches wl_list lengths, proving
 * the gesture resources were registered in the real compositor's gesture
 * manager rather than merely surviving without a protocol error.
 */
struct pointer_gestures_server_state {
	int valid; /* wlr_pointer_gestures_v1 handle found */
	int swipes; /* number of swipe gesture resources in production */
	int pinches; /* number of pinch gesture resources in production */
};

void pointer_gestures_read_server_state(void *data);

#ifdef __cplusplus
}
#endif

#endif /* WAYLAND_POINTER_GESTURES_TEST_H */
