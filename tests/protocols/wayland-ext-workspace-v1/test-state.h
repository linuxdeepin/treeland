// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once
#include <stdint.h>
struct workspace_test_state {
    char background_socket[256];
    uint32_t counts[2];
    uint32_t current[2];
    int independent;
};
#ifdef __cplusplus
extern "C" {
#endif
void workspace_test_read(void *data);
void workspace_test_remove_first(void *data);
void workspace_test_reorder(void *data);
#ifdef __cplusplus
}
#endif
