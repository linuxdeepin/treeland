// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct dde_picker_desktop_state {
    int output_ready;
    int wrapper_ready;
    int wrapper_in_workspace;
    int manager_found;
    int picker_resource_created;
    int pick_request_received;
    int root_object_count;
    int window_picker_instances;
    int picker_created;
    int mapped_window_selected;
};

int protocol_test_run(const char *socket_name);

#ifdef __cplusplus
}
#endif
