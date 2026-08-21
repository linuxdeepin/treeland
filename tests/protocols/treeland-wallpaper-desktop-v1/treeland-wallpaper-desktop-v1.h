// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct wallpaper_desktop_state {
    int shell_surface_registered;
    int manager_reference_matched;
    int output_matched;
    int manager_configured;
};

int protocol_test_run(const char *socket_name);

#ifdef __cplusplus
}
#endif
