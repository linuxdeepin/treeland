/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 */
#ifndef WAYLAND_XDG_DECORATION_TEST_H
#define WAYLAND_XDG_DECORATION_TEST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * E-level readback: the production WXdgDecorationManager reports the
 * effective decoration mode for the mapped toplevel surface.  The client
 * requests CLIENT_SIDE via set_mode; after commit the production object's
 * modeBySurface() must reflect the client's preference, proving the
 * decoration request reached the real compositor decoration pipeline.
 */
struct xdg_decoration_server_state {
	int valid; /* a mapped SurfaceWrapper was captured */
	int mode; /* WXdgDecorationManager::DecorationMode: 0=Undefined,1=None,2=Client,3=Server */
};

void xdg_decoration_read_server_state(void *data);

#ifdef __cplusplus
}
#endif

#endif /* WAYLAND_XDG_DECORATION_TEST_H */
