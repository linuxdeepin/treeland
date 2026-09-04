/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 */
#ifndef WAYLAND_EXT_FOREIGN_TOPLEVEL_LIST_TEST_H
#define WAYLAND_EXT_FOREIGN_TOPLEVEL_LIST_TEST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * E-level readback: the production WExtForeignToplevelListV1's wlroots handle
 * has a toplevels linked list.  After the client maps a toplevel (which
 * triggers WExtForeignToplevelListV1::addSurface), the list must contain at
 * least one entry, proving the toplevel was registered in the real compositor
 * foreign-toplevel-list pipeline.
 */
struct foreign_toplevel_list_server_state {
	int valid; /* manager was found */
	int count; /* number of toplevels in the list */
};

void foreign_toplevel_list_read_server_state(void *data);

#ifdef __cplusplus
}
#endif

#endif /* WAYLAND_EXT_FOREIGN_TOPLEVEL_LIST_TEST_H */
