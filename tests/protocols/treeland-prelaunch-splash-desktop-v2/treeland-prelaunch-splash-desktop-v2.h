// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct prelaunch_splash_desktop_state {
    int output_ready;
    int wrapper_created;
    int wrapper_in_workspace;
    int wrapper_is_splash;
    int wrapper_has_qml_item;
    int wrapper_destroyed;
    int wrapper_width;
    int wrapper_height;
    char app_id[96];
};

int protocol_test_run(const char *socket_name);

#ifdef __cplusplus
}
#endif
