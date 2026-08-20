// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct foreign_toplevel_server_state {
    int output_ready;
    int wrapper_created;
    int wrapper_in_workspace;
    int minimized;
    int maximized;
    int fullscreen;
    int activated;
    int focused;
    int maximize_request_count;
    int last_maximize_request;
    int maximized_after_last_request;
    int animation_running_after_last_request;
    int animation_running;
};

#ifdef __cplusplus
}
#endif
