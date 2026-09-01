/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 */
#ifndef WAYLAND_EXT_DATA_CONTROL_TEST_H
#define WAYLAND_EXT_DATA_CONTROL_TEST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * E-level readback: the production wlr_seat's selection_source field must be
 * non-NULL after the client sets a selection via the ext_data_control protocol.
 * This proves the data control selection request reached the real compositor
 * seat pipeline rather than merely surviving without a protocol error.
 */
struct ext_data_control_server_state {
	int valid; /* seat was found */
	int has_source; /* selection_source != NULL */
};

void ext_data_control_read_server_state(void *data);

#ifdef __cplusplus
}
#endif

#endif /* WAYLAND_EXT_DATA_CONTROL_TEST_H */
