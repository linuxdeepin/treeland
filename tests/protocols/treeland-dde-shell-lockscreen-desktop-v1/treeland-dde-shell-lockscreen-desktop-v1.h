// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct dde_lockscreen_desktop_state {
    int output_ready;
    int lockscreen_available;
    int lockscreen_locked;
    int mode_is_normal;
    int mode_is_lockscreen;
};

int protocol_test_run(const char *socket_name);

#ifdef __cplusplus
}
#endif
