/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 */
#ifndef WAYLAND_RELATIVE_POINTER_TEST_H
#define WAYLAND_RELATIVE_POINTER_TEST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * E-level readback: the production WRelativePointerManagerV1's wlroots handle
 * has a relative_pointers linked list.  After the client creates a
 * zwp_relative_pointer_v1, the list must contain at least one entry, proving
 * the relative pointer object was created in the real compositor.
 */
struct relative_pointer_server_state {
	int valid; /* manager was found */
	int count; /* number of relative_pointers in the list */
};

void relative_pointer_read_server_state(void *data);

#ifdef __cplusplus
}
#endif

#endif /* WAYLAND_RELATIVE_POINTER_TEST_H */
