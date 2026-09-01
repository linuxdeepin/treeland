/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 */
#ifndef WAYLAND_POINTER_CONSTRAINTS_TEST_H
#define WAYLAND_POINTER_CONSTRAINTS_TEST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * E-level readback: the production WPointerConstraintsV1 resolves the active
 * constraint for the client's surface+seat via constraintForSurface().  The
 * constraint type must be Locked (1), proving the lock_pointer request reached
 * the real compositor constraint pipeline.
 *
 * wlr_pointer_constraint_v1_type: 0=Locked, 1=Confined
 * (enum wlr_pointer_constraint_v1_type maps from zwp enum)
 */
struct pointer_constraints_server_state {
	int valid; /* a constraint was found */
	int constraint_type; /* 0=Locked, 1=Confined */
};

void pointer_constraints_read_server_state(void *data);

#ifdef __cplusplus
}
#endif

#endif /* WAYLAND_POINTER_CONSTRAINTS_TEST_H */
