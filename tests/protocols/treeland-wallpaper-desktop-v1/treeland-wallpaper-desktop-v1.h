/* SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct wallpaper_desktop_state {
    int shell_surface_registered;
    int shell_surface_mapped;
    int shell_surface_ready;
    int manager_reference_matched;
    int output_matched;
    int manager_configured;
    int switcher_source_matched;
    int content_surface_matched;
    int content_visible;
    int surface_width;
    int surface_height;
};

int protocol_test_run(const char *socket_name);

#ifdef __cplusplus
}
#endif
