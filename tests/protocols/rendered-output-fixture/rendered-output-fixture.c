/* SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only */

#include "protocol-test-xdg-client.h"
#include "rendered-output-fixture.h"

#include <stdio.h>
#include <string.h>

extern void rendered_output_fixture_read_state(void *data);

int protocol_test_run(const char *socket_name)
{
    struct protocol_test_connection connection;
    struct protocol_test_xdg_toplevel toplevel = { 0 };
    struct rendered_output_fixture_state state = { 0 };
    if (!protocol_test_connect(&connection, socket_name))
        return 1;
    if (!protocol_test_xdg_toplevel_create_with_solid_buffer(
            &connection, &toplevel, 64, 64, 0xffff0000u)
        || !protocol_test_invoke_server(rendered_output_fixture_read_state, &state)
        || !state.output_ready
        || !state.wrapper_created
        || !state.wrapper_in_workspace
        || !state.render_window_ready
        || !state.surface_content_ready
        || !state.image_ready
        || state.image_width != 64
        || state.image_height != 64
        || state.sample_red != 255
        || state.sample_green != 0
        || state.sample_blue != 0
        || state.sample_alpha != 255) {
        fprintf(stderr,
                "rendered output fixture failed: output=%d wrapper=%d workspace=%d window=%d content=%d "
                "image=%d size=%dx%d rgba=(%d,%d,%d,%d)\n",
                state.output_ready,
                state.wrapper_created,
                state.wrapper_in_workspace,
                state.render_window_ready,
                state.surface_content_ready,
                state.image_ready,
                state.image_width,
                state.image_height,
                state.sample_red,
                state.sample_green,
                state.sample_blue,
                state.sample_alpha);
        protocol_test_xdg_toplevel_destroy(&toplevel);
        protocol_test_disconnect(&connection);
        return 1;
    }
    protocol_test_xdg_toplevel_destroy(&toplevel);
    protocol_test_disconnect(&connection);
    return 0;
}
