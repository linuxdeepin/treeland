/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 */
#ifndef WAYLAND_PRIMARY_SELECTION_TEST_H
#define WAYLAND_PRIMARY_SELECTION_TEST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * E-level readback: the production wlr_seat's primary_selection_source field
 * must be non-NULL after the client sets a primary selection source.  This
 * proves the primary selection request reached the real compositor seat
 * pipeline rather than merely surviving without a protocol error.
 */
struct primary_selection_server_state {
	int valid; /* seat was found */
	int has_source; /* primary_selection_source != NULL */
};

void primary_selection_read_server_state(void *data);

#ifdef __cplusplus
}
#endif

#endif /* WAYLAND_PRIMARY_SELECTION_TEST_H */
