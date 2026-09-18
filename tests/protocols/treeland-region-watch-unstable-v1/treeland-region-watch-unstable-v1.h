// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#ifndef REGION_WATCH_TEST_H
#define REGION_WATCH_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

// Server-side state of the captured watcher, shared by setup.cpp (producer)
// and the C client (consumer via invoke_on_server_thread).
struct region_watch_server_state {
    int has_watch;
    int has_region;
    int x;
    int y;
    int width;
    int height;
    char second_output_name[64];
};

#ifdef __cplusplus
}
#endif
#endif
