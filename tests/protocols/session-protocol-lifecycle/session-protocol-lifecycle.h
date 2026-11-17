// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

struct lifecycle_state {
    char socket_name[512];
    int ready;
    int observed;
    int destroyed;
    int globals_retired;
    int interfaces_removed;
    int detached_once;
};

#ifdef __cplusplus
extern "C" {
#endif
void lifecycle_prepare(void *data);
void lifecycle_detach(void *data);
void lifecycle_remove(void *data);
#ifdef __cplusplus
}
#endif
