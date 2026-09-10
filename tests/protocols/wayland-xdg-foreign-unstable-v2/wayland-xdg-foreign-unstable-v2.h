/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 */
#ifndef WAYLAND_XDG_FOREIGN_TEST_H
#define WAYLAND_XDG_FOREIGN_TEST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * E-level readback: after the client calls set_parent_of on an imported
 * surface, the production WXdgToplevelSurface for the child surface must
 * report a non-null parentXdgSurface().  This proves the real
 * wlr_xdg_toplevel_set_parent path was exercised through the xdg-foreign-v2
 * protocol, establishing the parent-child relationship on the real
 * compositor's toplevel objects.
 */
struct xdg_foreign_v2_server_state {
	int valid; /* both parent and child SurfaceWrappers captured */
	int has_parent; /* child toplevel has parent set in production */
};

void xdg_foreign_v2_read_server_state(void *data);

#ifdef __cplusplus
}
#endif

#endif /* WAYLAND_XDG_FOREIGN_TEST_H */
