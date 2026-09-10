/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 */
#ifndef WAYLAND_XDG_DIALOG_TEST_H
#define WAYLAND_XDG_DIALOG_TEST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Snapshot of the real mapped SurfaceWrapper's modal flag, read on the
 * compositor (Qt) thread.  Treeland wires WXdgDialogManagerV1::surfaceModalChanged
 * to SurfaceWrapper::setModal(), so an end-to-end (E-level) test verifies that a
 * client xdg_dialog_v1.set_modal request actually flips the production wrapper's
 * modal state, rather than only surviving without a protocol error.
 */
struct xdg_dialog_server_state {
	int valid; /* a mapped XdgToplevel SurfaceWrapper was captured */
	int modal; /* SurfaceWrapper::modal() */
};

/* Defined in setup.cpp; runs on the compositor thread via
 * invoke_on_server_thread() and fills *data with the wrapper's modal flag. */
void xdg_dialog_read_server_state(void *data);

#ifdef __cplusplus
}
#endif

#endif /* WAYLAND_XDG_DIALOG_TEST_H */
