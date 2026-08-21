// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct dde_multitask_desktop_state {
    int output_ready;
    int workspace_window_count;
    int multitask_created;
    int multitask_status;
    int active_reason;
    int mode_is_multitask;
    int mode_is_normal;
    int partial_factor_milli;
};

int protocol_test_run(const char *socket_name);

#ifdef __cplusplus
}
#endif
