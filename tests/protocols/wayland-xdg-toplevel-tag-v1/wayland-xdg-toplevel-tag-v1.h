/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 */
#ifndef WAYLAND_XDG_TOPLEVEL_TAG_TEST_H
#define WAYLAND_XDG_TOPLEVEL_TAG_TEST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Snapshot of the real mapped WXdgToplevelSurface's tag, read on the
 * compositor (Qt) thread.  The xdg_toplevel_tag_manager_v1.set_toplevel_tag
 * request is handled by WXdgToplevelTagManagerV1, which stores the tag on the
 * production WXdgToplevelSurface via setTag(); an end-to-end (E-level) test
 * reads that live object back and verifies the tag matches the client request,
 * rather than only asserting the request is accepted.
 */
struct xdg_toplevel_tag_server_state {
	int valid; /* a mapped XdgToplevel SurfaceWrapper was captured */
	char tag[128]; /* WXdgToplevelSurface::tag() (UTF-8) */
};

/* Defined in setup.cpp; runs on the compositor thread via
 * invoke_on_server_thread() and fills *data with the wrapper's tag. */
void xdg_toplevel_tag_read_server_state(void *data);

#ifdef __cplusplus
}
#endif

#endif /* WAYLAND_XDG_TOPLEVEL_TAG_TEST_H */
