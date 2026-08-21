// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct app_id_resolver_desktop_state {
    int output_ready;
    int splash_created;
    int wrapper_in_workspace;
    int wrapper_converted_to_xdg;
    int wrapper_app_id_matches;
    int workspace_surface_count;
};

int protocol_test_run(const char *socket_name);

#ifdef __cplusplus
}
#endif
