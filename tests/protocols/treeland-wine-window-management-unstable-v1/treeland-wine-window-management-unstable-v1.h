/*
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 */
#ifndef WINE_WM_TEST_H
#define WINE_WM_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

#include "protocol-test-client.h"
#include "protocol-test-xdg-client.h"

/* Server-side state read by the C client via protocol_test_invoke_server(). */
struct wine_wm_state {
    int wrapper_created;
    int x;
    int y;
    int always_on_top;        // explicit flag from wrapper->alwaysOnTop()
    int effective_always_on_top;  // E-level: effective flag (includes parent inheritance)
    int z;                   // E-level: QQuickItem z value (1 = always-on-top layer)
    int parent_item_count;   // E-level: for verifying surface is in parent container
};

extern void wine_wm_read_state(void *data);

#ifdef __cplusplus
}
#endif
#endif
