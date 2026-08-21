// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct capture_desktop_selection_state {
    int output_ready;
    int wrapper_ready;
    int surface_content_ready;
    int content_in_paint_order;
    int content_visible;
    int content_width;
    int content_height;
    int selector_ready;
    int hovered_mapped_content;
    int source_selected;
    int source_is_surface;
    int source_width;
    int source_height;
    int render_requested;
};

int protocol_test_run(const char *socket_name);

#ifdef __cplusplus
}
#endif
