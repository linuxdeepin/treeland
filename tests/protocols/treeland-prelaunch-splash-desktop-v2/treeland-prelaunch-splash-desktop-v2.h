// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct prelaunch_splash_desktop_state {
    int output_ready;
    int wrapper_created;
    int wrapper_in_workspace;
    int wrapper_is_splash;
    int wrapper_has_qml_item;
    int wrapper_destroyed;
    int wrapper_width;
    int wrapper_height;
    int icon_ready;
    int icon_width;
    int icon_height;
    char app_id[96];
    /* After the splash converted into the app's real window. */
    int converted_to_xdg;
    int splash_item_gone;
};

struct desktop_preview_icon_probe {
    int expect_icon;
    int preview_found;
    int icon_item_found;
    int icon_buffer_matches;
    int icon_visible;
    int icon_width;
    int icon_height;
    int title_row_height;
    int capture_ready;
    int pixel_red;
    int pixel_green;
    int pixel_blue;
    int pixel_alpha;
    int buffer_warnings;
};

int protocol_test_run(const char *socket_name);

#ifdef __cplusplus
}
#endif
