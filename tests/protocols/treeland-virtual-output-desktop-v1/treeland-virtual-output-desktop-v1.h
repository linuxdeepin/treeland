// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct virtual_output_desktop_state {
    int first_present;
    int second_present;
    int root_output_count;
    int primary_is_first;
    int first_is_normal;
    int second_is_normal;
    int second_is_copy;
};

int protocol_test_run(const char *socket_name);

#ifdef __cplusplus
}
#endif
