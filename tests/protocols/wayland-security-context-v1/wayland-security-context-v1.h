/*
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 */
#ifndef WAYLAND_SECURITY_CONTEXT_TEST_H
#define WAYLAND_SECURITY_CONTEXT_TEST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * E-level readback: the production wlr_security_context_manager_v1 emits a
 * commit signal when the client commits a security context.  The captured
 * app_id must match the client's requested value, proving the security context
 * commit reached the real compositor security-context pipeline.
 */
struct security_context_server_state {
	int valid; /* a commit event was captured */
	int app_id_match; /* captured app_id matches "test-app" */
};

void security_context_read_server_state(void *data);

#ifdef __cplusplus
}
#endif

#endif /* WAYLAND_SECURITY_CONTEXT_TEST_H */
