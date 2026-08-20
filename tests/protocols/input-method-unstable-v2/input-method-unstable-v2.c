// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "input-method-unstable-v2.h"
#include "input-method-unstable-v2-client-protocol.h"
#include "client-connection.h"
#include "xdg-toplevel-client.h"
#include "text-input-unstable-v2-client-protocol.h"
#include "virtual-keyboard-unstable-v1-client-protocol.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <xkbcommon/xkbcommon.h>

// input-method-v2 references zwp_text_input_v3.change_cause, whose
// input_method value is specified as zero. The v3 XML is not a test input.
#define INPUT_METHOD_CHANGE_CAUSE_INPUT_METHOD 0u

struct input_method_events {
    unsigned int activate;
    unsigned int deactivate;
    unsigned int surrounding_text;
    unsigned int text_change_cause;
    unsigned int content_type;
    unsigned int done;
    unsigned int unavailable;
    char surrounding[64];
    uint32_t cursor;
    uint32_t anchor;
    uint32_t cause;
    uint32_t hint;
    uint32_t purpose;
};

struct text_input_events {
    unsigned int enter;
    unsigned int leave;
    unsigned int commit_string;
    unsigned int delete_surrounding_text;
    unsigned int preedit_string;
    unsigned int preedit_cursor;
    unsigned int preedit_styling;
    char committed[64];
    char preedit[64];
    uint32_t delete_before;
    uint32_t delete_after;
    int32_t preedit_cursor_index;
    uint32_t preedit_style_length;
};

struct popup_events {
    unsigned int text_input_rectangle;
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
};

struct keyboard_grab_events {
    unsigned int keymap;
    unsigned int key;
    unsigned int modifiers;
    unsigned int repeat_info;
    uint32_t last_key;
    uint32_t last_key_state;
    uint32_t last_modifiers;
    int32_t repeat_rate;
    int32_t repeat_delay;
};

static int send_keymap(struct zwp_virtual_keyboard_v1 *keyboard)
{
    struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_keymap *keymap = context
        ? xkb_keymap_new_from_names(context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS) : NULL;
    char *keymap_text = keymap
        ? xkb_keymap_get_as_string(keymap, XKB_KEYMAP_FORMAT_TEXT_V1) : NULL;
    if (!keymap_text)
        goto failed;

    char name[64];
    snprintf(name, sizeof(name), "/treeland_input_method_keymap_%d", (int)getpid());
    const int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
    shm_unlink(name);
    if (fd < 0)
        goto failed;
    const size_t size = strlen(keymap_text) + 1;
    if (ftruncate(fd, (off_t)size) < 0 || write(fd, keymap_text, size) != (ssize_t)size) {
        close(fd);
        goto failed;
    }
    zwp_virtual_keyboard_v1_keymap(keyboard, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, (uint32_t)size);
    close(fd);
    free(keymap_text);
    xkb_keymap_unref(keymap);
    xkb_context_unref(context);
    return 1;

failed:
    if (keymap_text) free(keymap_text);
    if (keymap) xkb_keymap_unref(keymap);
    if (context) xkb_context_unref(context);
    return 0;
}

static void im_activate(void *data, struct zwp_input_method_v2 *im)
{
    (void)im;
    ++((struct input_method_events *)data)->activate;
}

static void im_deactivate(void *data, struct zwp_input_method_v2 *im)
{
    (void)im;
    ++((struct input_method_events *)data)->deactivate;
}

static void im_surrounding_text(void *data, struct zwp_input_method_v2 *im,
                                const char *text, uint32_t cursor, uint32_t anchor)
{
    (void)im;
    struct input_method_events *events = data;
    ++events->surrounding_text;
    snprintf(events->surrounding, sizeof(events->surrounding), "%s", text);
    events->cursor = cursor;
    events->anchor = anchor;
}

static void im_text_change_cause(void *data, struct zwp_input_method_v2 *im, uint32_t cause)
{
    (void)im;
    struct input_method_events *events = data;
    ++events->text_change_cause;
    events->cause = cause;
}

static void im_content_type(void *data, struct zwp_input_method_v2 *im,
                            uint32_t hint, uint32_t purpose)
{
    (void)im;
    struct input_method_events *events = data;
    ++events->content_type;
    events->hint = hint;
    events->purpose = purpose;
}

static void im_done(void *data, struct zwp_input_method_v2 *im)
{
    (void)im;
    ++((struct input_method_events *)data)->done;
}

static void im_unavailable(void *data, struct zwp_input_method_v2 *im)
{
    (void)im;
    ++((struct input_method_events *)data)->unavailable;
}

static const struct zwp_input_method_v2_listener im_listener = {
    .activate = im_activate,
    .deactivate = im_deactivate,
    .surrounding_text = im_surrounding_text,
    .text_change_cause = im_text_change_cause,
    .content_type = im_content_type,
    .done = im_done,
    .unavailable = im_unavailable,
};

static void text_input_enter(void *data, struct zwp_text_input_v2 *text_input,
                             uint32_t serial, struct wl_surface *surface)
{
    (void)text_input;
    (void)serial;
    (void)surface;
    ++((struct text_input_events *)data)->enter;
}

static void text_input_leave(void *data, struct zwp_text_input_v2 *text_input,
                             uint32_t serial, struct wl_surface *surface)
{
    (void)text_input;
    (void)serial;
    (void)surface;
    ++((struct text_input_events *)data)->leave;
}

static void text_input_input_panel_state(void *data, struct zwp_text_input_v2 *text_input,
                                         uint32_t state, int32_t x, int32_t y, int32_t width, int32_t height)
{ (void)data; (void)text_input; (void)state; (void)x; (void)y; (void)width; (void)height; }

static void text_input_preedit_string(void *data, struct zwp_text_input_v2 *text_input,
                                      const char *text, const char *commit)
{
    (void)text_input;
    (void)commit;
    struct text_input_events *events = data;
    ++events->preedit_string;
    snprintf(events->preedit, sizeof(events->preedit), "%s", text);
}

static void text_input_preedit_styling(void *data, struct zwp_text_input_v2 *text_input,
                                       uint32_t index, uint32_t length, uint32_t style)
{
    (void)text_input;
    (void)index;
    (void)style;
    struct text_input_events *events = data;
    ++events->preedit_styling;
    events->preedit_style_length = length;
}

static void text_input_preedit_cursor(void *data, struct zwp_text_input_v2 *text_input, int32_t index)
{
    (void)text_input;
    struct text_input_events *events = data;
    ++events->preedit_cursor;
    events->preedit_cursor_index = index;
}

static void text_input_commit_string(void *data, struct zwp_text_input_v2 *text_input, const char *text)
{
    (void)text_input;
    struct text_input_events *events = data;
    ++events->commit_string;
    snprintf(events->committed, sizeof(events->committed), "%s", text);
}

static void text_input_cursor_position(void *data, struct zwp_text_input_v2 *text_input,
                                       int32_t index, int32_t anchor)
{ (void)data; (void)text_input; (void)index; (void)anchor; }

static void text_input_delete_surrounding_text(void *data, struct zwp_text_input_v2 *text_input,
                                                uint32_t before, uint32_t after)
{
    (void)text_input;
    struct text_input_events *events = data;
    ++events->delete_surrounding_text;
    events->delete_before = before;
    events->delete_after = after;
}

static void text_input_modifiers_map(void *data, struct zwp_text_input_v2 *text_input, struct wl_array *map)
{ (void)data; (void)text_input; (void)map; }
static void text_input_keysym(void *data, struct zwp_text_input_v2 *text_input,
                              uint32_t time, uint32_t sym, uint32_t state, uint32_t modifiers)
{ (void)data; (void)text_input; (void)time; (void)sym; (void)state; (void)modifiers; }
static void text_input_language(void *data, struct zwp_text_input_v2 *text_input, const char *language)
{ (void)data; (void)text_input; (void)language; }
static void text_input_direction(void *data, struct zwp_text_input_v2 *text_input, uint32_t direction)
{ (void)data; (void)text_input; (void)direction; }
static void text_input_configure_surrounding(void *data, struct zwp_text_input_v2 *text_input,
                                             int32_t before, int32_t after)
{ (void)data; (void)text_input; (void)before; (void)after; }
static void text_input_method_changed(void *data, struct zwp_text_input_v2 *text_input,
                                      uint32_t serial, uint32_t flags)
{ (void)data; (void)text_input; (void)serial; (void)flags; }

static const struct zwp_text_input_v2_listener text_input_listener = {
    .enter = text_input_enter,
    .leave = text_input_leave,
    .input_panel_state = text_input_input_panel_state,
    .preedit_string = text_input_preedit_string,
    .preedit_styling = text_input_preedit_styling,
    .preedit_cursor = text_input_preedit_cursor,
    .commit_string = text_input_commit_string,
    .cursor_position = text_input_cursor_position,
    .delete_surrounding_text = text_input_delete_surrounding_text,
    .modifiers_map = text_input_modifiers_map,
    .keysym = text_input_keysym,
    .language = text_input_language,
    .text_direction = text_input_direction,
    .configure_surrounding_text = text_input_configure_surrounding,
    .input_method_changed = text_input_method_changed,
};

static void popup_text_input_rectangle(void *data, struct zwp_input_popup_surface_v2 *popup,
                                       int32_t x, int32_t y, int32_t width, int32_t height)
{
    (void)popup;
    struct popup_events *events = data;
    ++events->text_input_rectangle;
    events->x = x;
    events->y = y;
    events->width = width;
    events->height = height;
}

static const struct zwp_input_popup_surface_v2_listener popup_listener = {
    .text_input_rectangle = popup_text_input_rectangle,
};

static void keyboard_grab_keymap(void *data, struct zwp_input_method_keyboard_grab_v2 *grab,
                                 uint32_t format, int32_t fd, uint32_t size)
{
    (void)grab;
    (void)format;
    (void)size;
    ++((struct keyboard_grab_events *)data)->keymap;
    close(fd);
}

static void keyboard_grab_key(void *data, struct zwp_input_method_keyboard_grab_v2 *grab,
                              uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
    (void)grab;
    (void)serial;
    (void)time;
    struct keyboard_grab_events *events = data;
    ++events->key;
    events->last_key = key;
    events->last_key_state = state;
}

static void keyboard_grab_modifiers(void *data, struct zwp_input_method_keyboard_grab_v2 *grab,
                                    uint32_t serial, uint32_t depressed, uint32_t latched,
                                    uint32_t locked, uint32_t group)
{
    (void)grab;
    (void)serial;
    (void)latched;
    (void)locked;
    (void)group;
    struct keyboard_grab_events *events = data;
    ++events->modifiers;
    events->last_modifiers = depressed;
}

static void keyboard_grab_repeat_info(void *data, struct zwp_input_method_keyboard_grab_v2 *grab,
                                      int32_t rate, int32_t delay)
{
    (void)grab;
    struct keyboard_grab_events *events = data;
    ++events->repeat_info;
    events->repeat_rate = rate;
    events->repeat_delay = delay;
}

static const struct zwp_input_method_keyboard_grab_v2_listener keyboard_grab_listener = {
    .keymap = keyboard_grab_keymap,
    .key = keyboard_grab_key,
    .modifiers = keyboard_grab_modifiers,
    .repeat_info = keyboard_grab_repeat_info,
};

int protocol_test_run(const char *socket_name)
{
    struct client_connection app = {0};
    struct client_connection im_connection = {0};
    struct xdg_toplevel_client toplevel = {0};
    struct input_method_events first_events = {0};
    struct input_method_events second_events = {0};
    struct text_input_events text_events = {0};
    struct popup_events popup_events = {0};
    struct keyboard_grab_events grab_events = {0};
    struct wl_seat *app_seat = NULL;
    struct zwp_text_input_manager_v2 *text_manager = NULL;
    struct zwp_text_input_v2 *text_input = NULL;
    struct zwp_virtual_keyboard_manager_v1 *virtual_keyboard_manager = NULL;
    struct zwp_virtual_keyboard_v1 *virtual_keyboard = NULL;
    struct wl_seat *im_seat = NULL;
    struct wl_compositor *im_compositor = NULL;
    struct zwp_input_method_manager_v2 *manager = NULL;
    struct zwp_input_method_v2 *first = NULL;
    struct zwp_input_method_v2 *second = NULL;
    struct wl_surface *popup_surface = NULL;
    struct zwp_input_popup_surface_v2 *popup = NULL;
    struct zwp_input_method_keyboard_grab_v2 *grab = NULL;
    int stage = 0;

    if (!client_connect(&app, socket_name) || !client_connect(&im_connection, socket_name))
        goto failed;
    app_seat = client_bind(&app, "wl_seat", &wl_seat_interface, 1);
    text_manager = client_bind(&app, "zwp_text_input_manager_v2",
                                      &zwp_text_input_manager_v2_interface, 1);
    virtual_keyboard_manager = client_bind(&app, "zwp_virtual_keyboard_manager_v1",
                                                  &zwp_virtual_keyboard_manager_v1_interface, 1);
    if (!app_seat || !text_manager || !virtual_keyboard_manager
        || !xdg_toplevel_client_create(&app, &toplevel))
        goto failed;
    text_input = zwp_text_input_manager_v2_get_text_input(text_manager, app_seat);
    if (!text_input)
        goto failed;
    zwp_text_input_v2_add_listener(text_input, &text_input_listener, &text_events);
    zwp_text_input_v2_set_surrounding_text(text_input, "abc", 3, 3);
    zwp_text_input_v2_set_content_type(text_input,
                                       ZWP_TEXT_INPUT_V2_CONTENT_HINT_AUTO_COMPLETION,
                                       ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_EMAIL);
    zwp_text_input_v2_set_cursor_rectangle(text_input, 11, 12, 13, 14);
    zwp_text_input_v2_update_state(text_input, 0, ZWP_TEXT_INPUT_V2_UPDATE_STATE_FULL);
    if (wl_display_roundtrip(app.display) < 0)
        goto failed;

    im_seat = client_bind(&im_connection, "wl_seat", &wl_seat_interface, 1);
    im_compositor = client_bind(&im_connection, "wl_compositor", &wl_compositor_interface, 1);
    manager = client_bind(&im_connection, "zwp_input_method_manager_v2",
                                 &zwp_input_method_manager_v2_interface, 1);
    if (!im_seat || !im_compositor || !manager)
        goto failed;
    first = zwp_input_method_manager_v2_get_input_method(manager, im_seat);
    if (!first)
        goto failed;
    zwp_input_method_v2_add_listener(first, &im_listener, &first_events);

    // XML requires these edits to be accepted while inactive. The following
    // focus/activate must reset them rather than replaying an old state.
    zwp_input_method_v2_commit_string(first, "inactive");
    zwp_input_method_v2_set_preedit_string(first, "inactive-preedit", 1, 3);
    zwp_input_method_v2_delete_surrounding_text(first, 2, 4);
    zwp_input_method_v2_commit(first, 0);
    if (wl_display_roundtrip(im_connection.display) < 0 || first_events.activate
        || first_events.deactivate || first_events.surrounding_text
        || first_events.text_change_cause || first_events.content_type || first_events.done
        || first_events.unavailable)
        goto failed;
    zwp_text_input_v2_enable(text_input, toplevel.surface);
    if (wl_display_roundtrip(app.display) < 0)
        goto failed;
    stage = 1;

    if (wl_display_roundtrip(im_connection.display) < 0
        || first_events.activate != 1 || first_events.surrounding_text != 1
        || strcmp(first_events.surrounding, "abc") != 0 || first_events.cursor != 3
        || first_events.anchor != 3 || first_events.text_change_cause != 1
        || first_events.cause != INPUT_METHOD_CHANGE_CAUSE_INPUT_METHOD
        || first_events.content_type != 1
        || first_events.hint != ZWP_TEXT_INPUT_V2_CONTENT_HINT_AUTO_COMPLETION
        || first_events.purpose != ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_EMAIL
        || first_events.done != 1 || first_events.unavailable)
        goto failed;
    const uint32_t input_method_serial = first_events.done;
    stage = 2;

    popup_surface = wl_compositor_create_surface(im_compositor);
    popup = popup_surface ? zwp_input_method_v2_get_input_popup_surface(first, popup_surface) : NULL;
    if (!popup)
        goto failed;
    zwp_input_popup_surface_v2_add_listener(popup, &popup_listener, &popup_events);
    if (wl_display_roundtrip(im_connection.display) < 0 || popup_events.text_input_rectangle != 1
        || popup_events.x != 11 || popup_events.y != 12 || popup_events.width != 13 || popup_events.height != 14)
        goto failed;
    zwp_input_popup_surface_v2_destroy(popup);
    popup = NULL;
    wl_surface_destroy(popup_surface);
    popup_surface = NULL;
    stage = 3;

    zwp_input_method_v2_delete_surrounding_text(first, 1, 2);
    zwp_input_method_v2_commit_string(first, "committed");
    zwp_input_method_v2_set_preedit_string(first, "preedit", 1, 4);
    zwp_input_method_v2_commit(first, input_method_serial);
    if (wl_display_roundtrip(im_connection.display) < 0 || wl_display_roundtrip(app.display) < 0
        || text_events.enter != 1 || text_events.delete_surrounding_text != 1 || text_events.delete_before != 1
        || text_events.delete_after != 2 || text_events.commit_string != 1
        || strcmp(text_events.committed, "committed") != 0 || text_events.preedit_string != 1
        || strcmp(text_events.preedit, "preedit") != 0 || text_events.preedit_cursor != 1
        || text_events.preedit_cursor_index != 3 || text_events.preedit_styling != 1
        || text_events.preedit_style_length != strlen("preedit"))
        goto failed;
    stage = 4;

    virtual_keyboard = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(
        virtual_keyboard_manager, app_seat);
    if (!virtual_keyboard || !send_keymap(virtual_keyboard) || wl_display_roundtrip(app.display) < 0)
        goto failed;
    grab = zwp_input_method_v2_grab_keyboard(first);
    if (!grab)
        goto failed;
    zwp_input_method_keyboard_grab_v2_add_listener(grab, &keyboard_grab_listener, &grab_events);
    if (wl_display_roundtrip(im_connection.display) < 0 || grab_events.keymap != 1
        || grab_events.modifiers != 1 || grab_events.repeat_info != 1
        || grab_events.repeat_rate < 0 || grab_events.repeat_delay < 0)
        goto failed;
    zwp_virtual_keyboard_v1_key(virtual_keyboard, 1, 59, WL_KEYBOARD_KEY_STATE_PRESSED);
    zwp_virtual_keyboard_v1_key(virtual_keyboard, 2, 59, WL_KEYBOARD_KEY_STATE_RELEASED);
    zwp_virtual_keyboard_v1_modifiers(virtual_keyboard, 1, 0, 0, 0);
    if (wl_display_roundtrip(app.display) < 0 || wl_display_roundtrip(im_connection.display) < 0
        || grab_events.key != 2 || grab_events.last_key != 59
        || grab_events.last_key_state != WL_KEYBOARD_KEY_STATE_RELEASED
        || grab_events.modifiers != 2 || grab_events.last_modifiers != 1)
        goto failed;
    zwp_input_method_keyboard_grab_v2_release(grab);
    grab = NULL;

    second = zwp_input_method_manager_v2_get_input_method(manager, im_seat);
    if (!second)
        goto failed;
    zwp_input_method_v2_add_listener(second, &im_listener, &second_events);
    if (wl_display_roundtrip(im_connection.display) < 0 || second_events.unavailable != 1
        || second_events.activate || second_events.deactivate || second_events.surrounding_text
        || second_events.text_change_cause || second_events.content_type || second_events.done)
        goto failed;
    stage = 5;

    zwp_input_method_manager_v2_destroy(manager);
    manager = NULL;
    zwp_input_method_v2_commit_string(first, "survives-manager-destroy");
    zwp_input_method_v2_commit(first, input_method_serial);
    if (wl_display_roundtrip(im_connection.display) < 0 || wl_display_roundtrip(app.display) < 0)
        goto failed;
    stage = 6;

    zwp_input_method_v2_destroy(second);
    zwp_input_method_v2_destroy(first);
    zwp_text_input_v2_destroy(text_input);
    zwp_text_input_manager_v2_destroy(text_manager);
    zwp_virtual_keyboard_v1_destroy(virtual_keyboard);
    wl_seat_destroy(im_seat);
    wl_compositor_destroy(im_compositor);
    wl_seat_destroy(app_seat);
    xdg_toplevel_client_destroy(&toplevel);
    client_disconnect(&im_connection);
    client_disconnect(&app);
    return 0;

failed:
    fprintf(stderr, "input-method-v2 failure at stage %d: IM=(activate=%u surrounding=%u cause=%u content=%u done=%u unavailable=%u) text=(enter=%u commit=%u delete=%u preedit=%u)\n",
            stage, first_events.activate, first_events.surrounding_text,
            first_events.text_change_cause, first_events.content_type, first_events.done,
            first_events.unavailable, text_events.enter, text_events.commit_string,
            text_events.delete_surrounding_text, text_events.preedit_string);
    client_disconnect(&im_connection);
    client_disconnect(&app);
    return 1;
}
