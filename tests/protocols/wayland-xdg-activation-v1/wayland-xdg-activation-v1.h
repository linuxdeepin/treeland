/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 */
#ifndef WAYLAND_XDG_ACTIVATION_TEST_H
#define WAYLAND_XDG_ACTIVATION_TEST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * E-level readback: the production ActivationManagerInterfaceV1 emits
 * activateRequested when the client sends xdg_activation_v1.activate with a
 * valid token.  The captured TokenDisposition must be non-Invalid, proving the
 * activation request reached the real compositor activation pipeline.
 *
 * TokenDisposition: 0=Invalid, 1=Attention, 2=Active
 */
struct xdg_activation_server_state {
	int valid; /* activateRequested signal was captured */
	int disposition; /* TokenDisposition enum value */
};

void xdg_activation_read_server_state(void *data);

#ifdef __cplusplus
}
#endif

#endif /* WAYLAND_XDG_ACTIVATION_TEST_H */
