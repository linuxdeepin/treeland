// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct rendered_output_fixture_state {
    int output_ready;
    int wrapper_created;
    int wrapper_in_workspace;
    int render_window_ready;
    int surface_content_ready;
    int image_ready;
    int image_width;
    int image_height;
    int sample_red;
    int sample_green;
    int sample_blue;
    int sample_alpha;
};

int protocol_test_run(const char *socket_name);

#ifdef __cplusplus
}
#endif
