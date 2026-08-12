/* SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only */

#include "treeland-input-manager-unstable-v1.h"
#include "protocol-test-client.h"
#include "treeland-input-manager-unstable-v1-client-protocol.h"

#include <stdio.h>

extern void input_manager_read_state(void *data);

struct capability_state {
    uint32_t types;
    int available_count;
};

static void capability_available(void *data,
                                 struct treeland_input_manager_v1 *manager,
                                 uint32_t types,
                                 struct wl_seat *seat)
{
    (void)manager;
    (void)seat;
    struct capability_state *state = data;
    state->types |= types;
    ++state->available_count;
}

static void capability_unavailable(void *data,
                                   struct treeland_input_manager_v1 *manager,
                                   uint32_t types,
                                   struct wl_seat *seat)
{
    (void)data;
    (void)manager;
    (void)types;
    (void)seat;
}

static const struct treeland_input_manager_v1_listener manager_listener = {
    .capability_available = capability_available,
    .capability_unavailable = capability_unavailable,
};

static int read_state(struct input_mgr_state *state)
{
    return protocol_test_invoke_server(input_manager_read_state, state);
}

int protocol_test_run(const char *socket_name)
{
    struct protocol_test_connection connection;
    if (!protocol_test_connect(&connection, socket_name))
        return 1;

    struct wl_seat *seat = protocol_test_bind(&connection, "wl_seat", &wl_seat_interface, 1);
    struct treeland_input_manager_v1 *manager = protocol_test_bind(
        &connection, "treeland_input_manager_v1", &treeland_input_manager_v1_interface, 1);
    if (!seat || !manager)
        goto failed;

    struct capability_state capabilities = {0};
    treeland_input_manager_v1_add_listener(manager, &manager_listener, &capabilities);
    if (wl_display_roundtrip(connection.display) < 0
        || capabilities.available_count != 1
        || capabilities.types != (TREELAND_INPUT_MANAGER_V1_DEVICE_TYPE_MOUSE
                                  | TREELAND_INPUT_MANAGER_V1_DEVICE_TYPE_TOUCHPAD
                                  | TREELAND_INPUT_MANAGER_V1_DEVICE_TYPE_KEYBOARD)) {
        fprintf(stderr, "input-manager: expected deterministic mouse/touchpad/keyboard capability\n");
        goto failed;
    }

    struct treeland_mouse_settings_v1 *mouse =
        treeland_input_manager_v1_get_mouse_settings(manager, seat);
    struct treeland_touchpad_settings_v1 *touchpad =
        treeland_input_manager_v1_get_touchpad_settings(manager, seat);
    struct treeland_keyboard_settings_v1 *keyboard =
        treeland_input_manager_v1_get_keyboard_settings(manager, seat);
    if (!mouse || !touchpad || !keyboard || wl_display_roundtrip(connection.display) < 0)
        goto failed;

    struct treeland_pointer_device_configuration_v1 *mouse_config =
        treeland_mouse_settings_v1_get_pointer_configuration(mouse, 0);
    struct treeland_pointer_device_configuration_v1 *touchpad_config =
        treeland_touchpad_settings_v1_get_pointer_configuration(touchpad, 0);
    if (!mouse_config || !touchpad_config || wl_display_roundtrip(connection.display) < 0)
        goto failed;

    /* Exercise the mouse factory independently, then configure every pointer
     * request on the touchpad factory as one atomic apply batch. */
    treeland_pointer_device_configuration_v1_set_scroll_factor(mouse_config, wl_fixed_from_double(1.25));
    treeland_pointer_device_configuration_v1_apply(mouse_config);

    treeland_pointer_device_configuration_v1_set_scroll_factor(touchpad_config, wl_fixed_from_double(1.5));
    treeland_pointer_device_configuration_v1_set_handed_mode(
        touchpad_config, TREELAND_POINTER_DEVICE_CONFIGURATION_V1_HANDED_MODE_LEFT);
    treeland_pointer_device_configuration_v1_set_accel_speed(touchpad_config, wl_fixed_from_double(0.5));
    treeland_pointer_device_configuration_v1_set_acceleration_profile(
        touchpad_config, TREELAND_POINTER_DEVICE_CONFIGURATION_V1_ACCELERATION_PROFILE_FLAT);
    treeland_pointer_device_configuration_v1_set_send_events_mode(
        touchpad_config, TREELAND_POINTER_DEVICE_CONFIGURATION_V1_SEND_EVENTS_MODE_DISABLED);
    treeland_pointer_device_configuration_v1_set_natural_scroll(
        touchpad_config, TREELAND_INPUT_MANAGER_V1_STATE_ENABLED);
    treeland_pointer_device_configuration_v1_set_disable_while_typing(
        touchpad_config, TREELAND_INPUT_MANAGER_V1_STATE_ENABLED);
    treeland_pointer_device_configuration_v1_set_tap_to_click(
        touchpad_config, TREELAND_INPUT_MANAGER_V1_STATE_ENABLED);
    treeland_pointer_device_configuration_v1_apply(touchpad_config);

    treeland_keyboard_settings_v1_set_repeat(keyboard, 32, 450);
    treeland_keyboard_settings_v1_set_num_lock(keyboard, TREELAND_KEYBOARD_SETTINGS_V1_TOGGLE_STATE_ON);
    treeland_keyboard_settings_v1_apply(keyboard);

    if (wl_display_roundtrip(connection.display) < 0)
        goto failed;

    struct input_mgr_state state = {0};
    if (!read_state(&state)
        || state.pointer_changes != 0xff
        || state.pointer_scroll_factor != 1.5
        || state.pointer_handed_mode != TREELAND_POINTER_DEVICE_CONFIGURATION_V1_HANDED_MODE_LEFT
        || state.pointer_accel_speed != 0.5
        || state.pointer_accel_profile != TREELAND_POINTER_DEVICE_CONFIGURATION_V1_ACCELERATION_PROFILE_FLAT
        || state.pointer_send_events_mode != TREELAND_POINTER_DEVICE_CONFIGURATION_V1_SEND_EVENTS_MODE_DISABLED
        || !state.pointer_natural_scroll || !state.pointer_disable_while_typing || !state.pointer_tap_to_click
        || state.keyboard_changes != 0x3
        || state.keyboard_repeat_rate != 32 || state.keyboard_repeat_delay != 450
        || !state.keyboard_num_lock) {
        fprintf(stderr, "input-manager: settings were not committed to production protocol objects\n");
        goto failed;
    }

    treeland_pointer_device_configuration_v1_destroy(mouse_config);
    treeland_pointer_device_configuration_v1_destroy(touchpad_config);
    treeland_mouse_settings_v1_destroy(mouse);
    treeland_touchpad_settings_v1_destroy(touchpad);
    treeland_keyboard_settings_v1_destroy(keyboard);
    treeland_input_manager_v1_destroy(manager);
    wl_seat_destroy(seat);
    protocol_test_disconnect(&connection);
    return 0;

failed:
    protocol_test_disconnect(&connection);
    return 1;
}
