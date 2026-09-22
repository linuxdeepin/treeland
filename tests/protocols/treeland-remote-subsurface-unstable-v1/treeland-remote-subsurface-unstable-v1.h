// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct remote_subsurface_server_state {
    int valid;
    int parent_matches;
    int type;
    int place;
    double x;
    double y;
};

void remote_subsurface_read_server_state(void *data);

#ifdef __cplusplus
}
#endif
