/* SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "protocol-test-client.h"
#include "protocol-test-xdg-client.h"

struct window_management_desktop_state {
    int wrapper_created;
    int wrapper_in_workspace;
    int wrapper_in_paint_order;
    int wrapper_visible;
    int wrapper_minimized;
    unsigned int desktop_state;
};

struct window_management_desktop_visibility_wait {
    int visible;
    int reached;
};

int protocol_test_run(const char *socket_name);

#ifdef __cplusplus
}
#endif
