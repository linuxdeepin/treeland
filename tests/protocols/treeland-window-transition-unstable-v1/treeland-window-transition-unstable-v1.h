// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Shared POD snapshot between the C client and the C++ fixture.
 *
 * `token` is an *input*: the client fills it with the activation token string
 * passed to get_window_transition_rect before invoking the read on the
 * compositor thread, so the callback must not clear it. The remaining fields
 * are outputs written on the compositor thread.
 *
 * The `rect_*` fields describe the rectangle the production manager actually
 * handed to a SurfaceWindowTransition, read from that wrapper's real state
 * instead of from any wrapper that happens to be armed:
 *
 *   armed_count   wrappers whose transition is active() (non-empty rect)
 *   rect_present  an associated rect is reachable (globalRect() has a value)
 *   rect_mapped   that rect's wrapper surface is already mapped
 *   rect_w/rect_h that rect's size in pixels, 0x0 while it is disabled
 *   source_set    that transition carries a source surface
 */
struct window_transition_server_state {
    char token[128];     /* input: token string to query */
    int manager_present; /* output: production manager interface found */
    int pending;         /* output: hasPendingWindowTransitionRect(token) */
    int armed_count;     /* output: wrappers with an active transition rect */
    int rect_present;    /* output: an associated rect is reachable */
    int rect_mapped;     /* output: the associated rect's wrapper is mapped */
    int rect_w;          /* output: associated rect width in px */
    int rect_h;          /* output: associated rect height in px */
    int source_set;      /* output: the associated transition has a source surface */
};

void window_transition_read_server_state(void *data);

#ifdef __cplusplus
}
#endif
