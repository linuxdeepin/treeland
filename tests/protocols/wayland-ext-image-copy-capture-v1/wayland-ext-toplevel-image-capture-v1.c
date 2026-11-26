// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#define _POSIX_C_SOURCE 200809L

#include "client-connection.h"
#include "server-bridge-api.h"
#include "xdg-toplevel-client.h"
#include "wayland-ext-toplevel-image-capture-v1.h"
#include "ext-image-capture-source-v1-client-protocol.h"
#include "ext-image-copy-capture-v1-client-protocol.h"
#include "ext-foreign-toplevel-list-v1-client-protocol.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "wlr-output-power-management-unstable-v1-client-protocol.h"
#include "treeland-ddm-v1-client-protocol.h"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define CHECK(condition, ...) do { \
    if (!(condition)) { TEST_ERROR(__VA_ARGS__); return 1; } \
} while (0)

enum { MAIN_WIDTH = 240, MAIN_HEIGHT = 180 };
static const uint32_t main_color = 0xff204080;
static const uint32_t red = 0xffff0000;
static const uint32_t green = 0xff00ff00;
static const uint32_t yellow = 0xffffff00;
static const uint32_t orange = 0xffff8000;
static const uint32_t teal = 0xff00c0c0;

struct shm_buffer {
    struct wl_buffer *buffer;
    uint32_t *pixels;
    size_t size;
    int width, height;
};

static int buffer_create(struct wl_shm *shm, struct shm_buffer *buffer,
                         int width, int height, uint32_t format, uint32_t color)
{
    static unsigned serial;
    char name[80];
    snprintf(name, sizeof(name), "/treeland_capture_%ld_%u", (long)getpid(), serial++);
    int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd < 0)
        return 0;
    shm_unlink(name);
    buffer->size = (size_t)width * height * 4;
    if (ftruncate(fd, (off_t)buffer->size) < 0) {
        close(fd);
        return 0;
    }
    buffer->pixels = mmap(NULL, buffer->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (buffer->pixels == MAP_FAILED) {
        close(fd);
        return 0;
    }
    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, (int)buffer->size);
    buffer->buffer = wl_shm_pool_create_buffer(pool, 0, width, height, width * 4, format);
    wl_shm_pool_destroy(pool);
    close(fd);
    buffer->width = width;
    buffer->height = height;
    for (size_t i = 0; i < (size_t)width * height; ++i)
        buffer->pixels[i] = color;
    return buffer->buffer != NULL;
}

static void buffer_destroy(struct shm_buffer *buffer)
{
    wl_buffer_destroy(buffer->buffer);
    munmap(buffer->pixels, buffer->size);
    *buffer = (struct shm_buffer){0};
}

static void attach(struct wl_surface *surface, struct shm_buffer *buffer)
{
    wl_surface_attach(surface, buffer->buffer, 0, 0);
    wl_surface_damage(surface, 0, 0, buffer->width, buffer->height);
    wl_surface_commit(surface);
}

struct foreign_state {
    struct ext_foreign_toplevel_handle_v1 *target;
    int target_done;
};

static void foreign_closed(void *data, struct ext_foreign_toplevel_handle_v1 *handle)
{
    struct foreign_state *state = data;
    if (state->target == handle)
        state->target = NULL;
    ext_foreign_toplevel_handle_v1_destroy(handle);
}

static void foreign_done(void *data, struct ext_foreign_toplevel_handle_v1 *handle)
{
    struct foreign_state *state = data;
    if (state->target == handle)
        state->target_done = 1;
}

static void foreign_title(void *data, struct ext_foreign_toplevel_handle_v1 *handle, const char *title)
{
    (void)data; (void)handle; (void)title;
}

static void foreign_app_id(void *data, struct ext_foreign_toplevel_handle_v1 *handle, const char *app_id)
{
    if (strcmp(app_id, "capture-target") == 0)
        ((struct foreign_state *)data)->target = handle;
}

static void foreign_identifier(void *data, struct ext_foreign_toplevel_handle_v1 *handle, const char *identifier)
{
    (void)data; (void)handle; (void)identifier;
}

static const struct ext_foreign_toplevel_handle_v1_listener foreign_listener = {
    .closed = foreign_closed,
    .done = foreign_done,
    .title = foreign_title,
    .app_id = foreign_app_id,
    .identifier = foreign_identifier,
};

static void list_toplevel(void *data, struct ext_foreign_toplevel_list_v1 *list,
                          struct ext_foreign_toplevel_handle_v1 *handle)
{
    (void)list;
    ext_foreign_toplevel_handle_v1_add_listener(handle, &foreign_listener, data);
}

static void list_finished(void *data, struct ext_foreign_toplevel_list_v1 *list)
{
    (void)data; (void)list;
}

static const struct ext_foreign_toplevel_list_v1_listener list_listener = {
    .toplevel = list_toplevel,
    .finished = list_finished,
};

static void decoration_configure(void *data, struct zxdg_toplevel_decoration_v1 *decoration, uint32_t mode)
{
    (void)decoration;
    *(uint32_t *)data = mode;
}

static const struct zxdg_toplevel_decoration_v1_listener decoration_listener = {
    .configure = decoration_configure,
};

struct capture_session {
    struct ext_image_copy_capture_session_v1 *session;
    uint32_t width, height, format;
    int done, stopped, has_shm;
};

static void session_size(void *data, struct ext_image_copy_capture_session_v1 *session,
                         uint32_t width, uint32_t height)
{
    (void)session;
    struct capture_session *state = data;
    state->width = width;
    state->height = height;
}

static void session_shm(void *data, struct ext_image_copy_capture_session_v1 *session, uint32_t format)
{
    (void)session;
    struct capture_session *state = data;
    if (!state->has_shm || format == WL_SHM_FORMAT_ARGB8888)
        state->format = format;
    state->has_shm = 1;
}

static void session_device(void *data, struct ext_image_copy_capture_session_v1 *session, struct wl_array *device)
{
    (void)data; (void)session; (void)device;
}

static void session_dmabuf(void *data, struct ext_image_copy_capture_session_v1 *session,
                          uint32_t format, struct wl_array *modifiers)
{
    (void)data; (void)session; (void)format; (void)modifiers;
}

static void session_done(void *data, struct ext_image_copy_capture_session_v1 *session)
{
    (void)session;
    ++((struct capture_session *)data)->done;
}

static void session_stopped(void *data, struct ext_image_copy_capture_session_v1 *session)
{
    (void)session;
    ((struct capture_session *)data)->stopped = 1;
}

static const struct ext_image_copy_capture_session_v1_listener session_listener = {
    .buffer_size = session_size,
    .shm_format = session_shm,
    .dmabuf_device = session_device,
    .dmabuf_format = session_dmabuf,
    .done = session_done,
    .stopped = session_stopped,
};

static int session_create(struct client_connection *connection, struct capture_session *state,
                          struct ext_image_copy_capture_manager_v1 *manager,
                          struct ext_image_capture_source_v1 *source)
{
    *state = (struct capture_session){0};
    state->session = ext_image_copy_capture_manager_v1_create_session(manager, source, 0);
    ext_image_copy_capture_session_v1_add_listener(state->session, &session_listener, state);
    CHECK(wl_display_roundtrip(connection->display) >= 0 && state->done && !state->stopped
          && state->has_shm && state->format == WL_SHM_FORMAT_ARGB8888,
          "capture: missing initial ARGB constraints\n");
    return 0;
}

struct frame_state {
    int ready, failed, has_transform, has_time, has_damage;
    uint32_t reason, transform, nsec;
};

static void frame_transform(void *data, struct ext_image_copy_capture_frame_v1 *frame, uint32_t transform)
{
    (void)frame;
    struct frame_state *state = data;
    state->transform = transform;
    state->has_transform = 1;
}

static void frame_damage(void *data, struct ext_image_copy_capture_frame_v1 *frame,
                         int32_t x, int32_t y, int32_t width, int32_t height)
{
    (void)frame;
    if (x >= 0 && y >= 0 && width > 0 && height > 0)
        ((struct frame_state *)data)->has_damage = 1;
}

static void frame_time(void *data, struct ext_image_copy_capture_frame_v1 *frame,
                       uint32_t hi, uint32_t lo, uint32_t nsec)
{
    (void)frame; (void)hi; (void)lo;
    struct frame_state *state = data;
    state->nsec = nsec;
    state->has_time = 1;
}

static void frame_ready(void *data, struct ext_image_copy_capture_frame_v1 *frame)
{
    (void)frame;
    ++((struct frame_state *)data)->ready;
}

static void frame_failed(void *data, struct ext_image_copy_capture_frame_v1 *frame, uint32_t reason)
{
    (void)frame;
    struct frame_state *state = data;
    ++state->failed;
    state->reason = reason;
}

static const struct ext_image_copy_capture_frame_v1_listener frame_listener = {
    .transform = frame_transform,
    .damage = frame_damage,
    .presentation_time = frame_time,
    .ready = frame_ready,
    .failed = frame_failed,
};

static struct ext_image_copy_capture_frame_v1 *begin_capture(struct capture_session *session,
                                                            struct shm_buffer *buffer,
                                                            struct frame_state *state)
{
    *state = (struct frame_state){0};
    struct ext_image_copy_capture_frame_v1 *frame =
        ext_image_copy_capture_session_v1_create_frame(session->session);
    ext_image_copy_capture_frame_v1_add_listener(frame, &frame_listener, state);
    ext_image_copy_capture_frame_v1_attach_buffer(frame, buffer->buffer);
    ext_image_copy_capture_frame_v1_damage_buffer(frame, 0, 0, buffer->width, buffer->height);
    ext_image_copy_capture_frame_v1_capture(frame);
    return frame;
}

static int finish_capture(struct client_connection *connection, struct capture_session *session,
                          struct ext_image_copy_capture_frame_v1 *frame,
                          struct frame_state *state, int expect_failure)
{
    while (!state->ready && !state->failed && !session->stopped) {
        CHECK(wl_display_dispatch(connection->display) >= 0, "capture: dispatch failed\n");
    }
    ext_image_copy_capture_frame_v1_destroy(frame);
    if (expect_failure) {
        CHECK(state->failed && state->reason == EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_BUFFER_CONSTRAINTS,
              "capture: expected buffer_constraints failure, got ready=%d reason=%u\n",
              state->ready, state->reason);
    } else {
        CHECK(state->ready && state->has_transform && state->transform == WL_OUTPUT_TRANSFORM_NORMAL
              && state->has_time && state->nsec < 1000000000 && state->has_damage,
              "capture: missing ready/metadata, failed=%d reason=%u\n", state->failed, state->reason);
    }
    return 0;
}

static int capture(struct client_connection *connection, struct capture_session *session,
                    struct shm_buffer *buffer, int expect_failure)
{
    struct frame_state state;
    struct ext_image_copy_capture_frame_v1 *frame = begin_capture(session, buffer, &state);
    return finish_capture(connection, session, frame, &state, expect_failure);
}

static int check_waiting(struct client_connection *connection, struct capture_session *session,
                         struct frame_state *frame, struct toplevel_capture_state *server, int paints)
{
    CHECK(wl_display_roundtrip(connection->display) >= 0
          && invoke_on_server_thread(toplevel_capture_sync_output, server)
          && wl_display_roundtrip(connection->display) >= 0
          && invoke_on_server_thread(toplevel_capture_check_screen, server),
          "capture: waiting-frame synchronization failed\n");
    CHECK(!frame->ready && !frame->failed && !session->stopped && server->capture_paints == paints,
          "capture: quiet frame completed or rendered (ready=%d failed=%d paints=%d -> %d)\n",
          frame->ready, frame->failed, paints, server->capture_paints);
    return 0;
}

struct power_state {
    uint32_t mode;
    int failed;
};

static void power_mode(void *data, struct zwlr_output_power_v1 *power, uint32_t mode)
{
    (void)power;
    ((struct power_state *)data)->mode = mode;
}

static void power_failed(void *data, struct zwlr_output_power_v1 *power)
{
    (void)power;
    ((struct power_state *)data)->failed = 1;
}

static const struct zwlr_output_power_v1_listener power_listener = {
    .mode = power_mode,
    .failed = power_failed,
};

static void ddm_vt(void *data, struct treeland_ddm_v1 *ddm, int32_t vtnr)
{
    (void)data; (void)ddm; (void)vtnr;
}

static const struct treeland_ddm_v1_listener ddm_listener = {
    .switch_to_vt = ddm_vt,
    .acquire_vt = ddm_vt,
};

static void callback_done(void *data, struct wl_callback *callback, uint32_t serial)
{
    (void)serial;
    ++*(int *)data;
    wl_callback_destroy(callback);
}

static const struct wl_callback_listener callback_listener = {
    .done = callback_done,
};

static int pause_rendering(struct client_connection *connection, struct treeland_ddm_v1 *ddm)
{
    int done = 0;
    struct wl_callback *callback = treeland_ddm_v1_disable_render(ddm);
    wl_callback_add_listener(callback, &callback_listener, &done);
    CHECK(wl_display_roundtrip(connection->display) >= 0 && done == 1,
          "capture: DDM did not acknowledge rendering pause\n");
    return 0;
}

static int check_render_pause(struct client_connection *connection, struct treeland_ddm_v1 *ddm,
                              struct capture_session *session, struct shm_buffer *buffer,
                              struct toplevel_capture_output_state *outputs, int capture_before_pause)
{
    struct frame_state state = {0};
    struct ext_image_copy_capture_frame_v1 *frame = NULL;
    if (capture_before_pause)
        frame = begin_capture(session, buffer, &state);
    CHECK(pause_rendering(connection, ddm) == 0
          && invoke_on_server_thread(toplevel_capture_read_output_state, outputs),
          "capture: could not pause rendering\n");
    const int attempts = outputs->render_attempts;
    if (!capture_before_pause)
        frame = begin_capture(session, buffer, &state);
    CHECK(wl_display_roundtrip(connection->display) >= 0
          && invoke_on_server_thread(toplevel_capture_read_output_state, outputs),
          "capture: could not observe the paused request\n");
    CHECK(!state.ready && !state.failed && !session->stopped
          && !outputs->render_enabled && outputs->render_attempts == attempts
          && outputs->viewport_unchanged,
          "capture: rendering pause completed or rendered a pending frame\n");
    treeland_ddm_v1_enable_render(ddm);
    CHECK(wl_display_roundtrip(connection->display) >= 0
          && invoke_on_server_thread(toplevel_capture_read_output_state, outputs)
          && wl_display_roundtrip(connection->display) >= 0,
          "capture: could not observe rendering resume\n");
    if (!outputs->capture_output_enabled) {
        CHECK(state.ready == 1 && !state.failed && outputs->render_attempts > attempts,
              "capture: resume did not wake the disabled-output request (ready=%d failed=%d)\n",
              state.ready, state.failed);
    }
    CHECK(finish_capture(connection, session, frame, &state, 0) == 0,
          "capture: pending frame did not complete after resume\n");
    CHECK(invoke_on_server_thread(toplevel_capture_read_output_state, outputs)
          && outputs->render_enabled && outputs->render_attempts > attempts && outputs->viewport_unchanged
          && outputs->offscreen_viewports == 1,
          "capture: rendering resume rebuilt the capture pipeline\n");
    return 0;
}

static int pixel_is(const struct shm_buffer *buffer, int x, int y, uint32_t color, const char *name)
{
    CHECK(x >= 0 && y >= 0 && x < buffer->width && y < buffer->height,
          "capture: %s point outside %dx%d\n", name, buffer->width, buffer->height);
    const uint32_t actual = buffer->pixels[(size_t)y * buffer->width + x];
    CHECK(actual == color, "capture: %s at %d,%d: expected %08x, got %08x\n", name, x, y, color, actual);
    return 0;
}

static int check_pixels(const struct shm_buffer *buffer,
                        const struct toplevel_capture_state *state, uint32_t nested_color)
{
    // The left-extending subsurface determines x=-20. Only a visible outer
    // SSD stroke adds y=-1; maximize removes it, drag-to-restore adds it back.
    const int titlebar = state->titlebar_height;
    const int top = state->border_visible ? 1 : 0;
    CHECK(pixel_is(buffer, 20 + 100, top + titlebar + 90, main_color, "occluded main surface") == 0,
          "capture: captured the desktop region instead of the window\n");
    CHECK(pixel_is(buffer, 20, top + titlebar + 40, red, "negative-position subsurface") == 0,
          "capture: missing above subsurface\n");
    CHECK(pixel_is(buffer, 20 + 40, top + titlebar - 5, nested_color, "nested subsurface") == 0,
          "capture: missing recursive subsurface\n");
    CHECK(pixel_is(buffer, 20 + 30, top + titlebar + 55, yellow, "above stacking order") == 0,
          "capture: wrong subsurface stacking\n");
    const uint32_t below_color = state->width > 247 && state->height - titlebar > 190
        ? main_color : orange;
    CHECK(pixel_is(buffer, 20 + 247, top + titlebar + 190, below_color, "below subsurface") == 0,
          "capture: clipped the below subsurface\n");
    CHECK(pixel_is(buffer, 10, top + titlebar + 190, 0, "transparent gap without shadow") == 0,
          "capture: captured shadow/background in the window buffer\n");
    CHECK(pixel_is(buffer, 20 + state->width - 20, top + state->height - 20,
                   main_color, "bottom-right content") == 0,
          "capture: truncated the current window size\n");
    const uint32_t title = buffer->pixels[(size_t)(top + 10) * buffer->width + 20 + 130];
    CHECK((title >> 24) == 255 && title != main_color, "capture: SSD titlebar missing\n");
    if (state->border_visible) {
        const uint32_t border = buffer->pixels[(size_t)(top + titlebar + 80) * buffer->width + 20 + state->width];
        CHECK((border >> 24) > 0 && (border >> 24) < 255, "capture: outer SSD border missing (%08x)\n", border);
    }
    return 0;
}

static int check_screen(struct toplevel_capture_state *state)
{
    CHECK(invoke_on_server_thread(toplevel_capture_check_screen, state), "capture: screen read failed\n");
    CHECK(state->geometry_unchanged && state->screen_unchanged && state->render_state_unchanged
          && state->capture_paints > 0 && state->offscreen_viewports == state->isolated_inputs
          && state->shared_delegates == state->offscreen_viewports,
          "capture: real window changed (geometry=%d pixels=%d QSG=%d paints=%d isolated=%d/%d cached=%d)\n",
          state->geometry_unchanged, state->screen_unchanged, state->render_state_unchanged,
          state->capture_paints, state->isolated_inputs, state->offscreen_viewports,
          state->shared_delegates);
    // Check first without repainting, then separately prove the next screen
    // render also remains correct. A repaint must never repair the evidence.
    CHECK(invoke_on_server_thread(toplevel_capture_repaint_screen, state) && state->screen_unchanged,
          "capture: next real output frame changed\n");
    return 0;
}

static int check_render_failure(struct client_connection *connection, struct capture_session *session,
                                struct capture_session *other, struct shm_buffer *buffer,
                                struct toplevel_capture_state *server)
{
    int held_buffers = 0;
    CHECK(invoke_on_server_thread(toplevel_capture_hold_buffers, &held_buffers) && held_buffers > 0,
          "capture: could not exhaust the real capture swapchain\n");
    struct capture_session *sessions[] = {session, other};
    struct frame_state states[2];
    struct ext_image_copy_capture_frame_v1 *frames[2];
    const size_t count = other ? 2 : 1;
    for (size_t i = 0; i < count; ++i)
        frames[i] = begin_capture(sessions[i], buffer, &states[i]);
    CHECK(wl_display_roundtrip(connection->display) >= 0,
          "capture: render-failure requests were not processed\n");
    CHECK(invoke_on_server_thread(toplevel_capture_render_offscreen, server)
          && wl_display_roundtrip(connection->display) >= 0,
          "capture: render-failure attempt did not finish\n");
    for (size_t i = 0; i < count; ++i) {
        CHECK(states[i].failed == 1 && !states[i].ready && !sessions[i]->stopped
              && !states[i].has_transform && !states[i].has_time && !states[i].has_damage
              && states[i].reason == EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_UNKNOWN,
              "capture: render failure left session %zu pending (failed=%d ready=%d stopped=%d reason=%u)\n",
              i, states[i].failed, states[i].ready, sessions[i]->stopped, states[i].reason);
        ext_image_copy_capture_frame_v1_destroy(frames[i]);
    }
    int viewport_survived = 0;
    CHECK(invoke_on_server_thread(toplevel_capture_release_buffers, &viewport_survived)
          && viewport_survived && server->offscreen_viewports == 1,
          "capture: render failure destroyed the shared capture viewport\n");
    for (size_t i = 0; i < count; ++i) {
        CHECK(capture(connection, sessions[i], buffer, 0) == 0,
              "capture: session %zu could not recover from render failure\n", i);
    }
    return 0;
}

static int commit_configured_buffer(struct client_connection *connection, struct xdg_toplevel_client *target,
                                    struct shm_buffer *buffer)
{
    xdg_surface_ack_configure(target->xdg_surface, target->configure_serial);
    target->acknowledged_configure_serial = target->configure_serial;
    xdg_surface_set_window_geometry(target->xdg_surface, 0, 0, buffer->width, buffer->height);
    attach(target->surface, buffer);
    CHECK(wl_display_roundtrip(connection->display) >= 0, "capture: configured buffer commit failed\n");
    return 0;
}

static int resize_destination(struct wl_shm *shm, const struct capture_session *session, struct shm_buffer *buffer)
{
    buffer_destroy(buffer);
    CHECK(buffer_create(shm, buffer, session->width, session->height, session->format, 0xabcdef01),
          "capture: destination allocation failed\n");
    return 0;
}

static void save_capture(const struct shm_buffer *buffer)
{
    const char *path = getenv("TREELAND_CAPTURE_TEST_IMAGE");
    if (!path)
        return;
    FILE *file = fopen(path, "wb");
    if (!file)
        return;
    fprintf(file, "P7\nWIDTH %d\nHEIGHT %d\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n",
            buffer->width, buffer->height);
    for (size_t i = 0; i < (size_t)buffer->width * buffer->height; ++i) {
        const uint32_t pixel = buffer->pixels[i];
        const unsigned char rgba[] = { pixel >> 16, pixel >> 8, pixel, pixel >> 24 };
        fwrite(rgba, 1, sizeof(rgba), file);
    }
    fclose(file);
}

static int same_pixels(const struct shm_buffer *buffer, const uint32_t *reference)
{
    for (size_t i = 0; i < buffer->size / sizeof(uint32_t); ++i) {
        if (buffer->pixels[i] != reference[i]) {
            TEST_ERROR("capture: image differs at %zu,%zu: %08x -> %08x\n",
                       i % buffer->width, i / buffer->width, reference[i], buffer->pixels[i]);
            save_capture(buffer);
            return 0;
        }
    }
    return 1;
}

int protocol_test_run(const char *socket_name)
{
    struct client_connection connection;
    CHECK(client_connect(&connection, socket_name), "capture: connect failed\n");
    struct ext_image_copy_capture_manager_v1 *manager = client_bind(&connection,
        ext_image_copy_capture_manager_v1_interface.name, &ext_image_copy_capture_manager_v1_interface, 1);
    struct ext_foreign_toplevel_image_capture_source_manager_v1 *source_manager = client_bind(&connection,
        ext_foreign_toplevel_image_capture_source_manager_v1_interface.name,
        &ext_foreign_toplevel_image_capture_source_manager_v1_interface, 1);
    struct ext_foreign_toplevel_list_v1 *list = client_bind(&connection,
        ext_foreign_toplevel_list_v1_interface.name, &ext_foreign_toplevel_list_v1_interface, 1);
    struct zxdg_decoration_manager_v1 *decoration_manager = client_bind(&connection,
        zxdg_decoration_manager_v1_interface.name, &zxdg_decoration_manager_v1_interface, 1);
    struct wl_subcompositor *subcompositor = client_bind(&connection, "wl_subcompositor", &wl_subcompositor_interface, 1);
    CHECK(manager && source_manager && list && decoration_manager && subcompositor, "capture: missing globals\n");
    struct foreign_state foreign = {0};
    ext_foreign_toplevel_list_v1_add_listener(list, &list_listener, &foreign);

    // Case: a real SSD toplevel with above, below and nested wl_subsurfaces.
    struct xdg_toplevel_client target, occluder;
    CHECK(xdg_toplevel_client_create_pending(&connection, &target), "capture: create target failed\n");
    xdg_toplevel_set_app_id(target.toplevel, "capture-target");
    xdg_toplevel_set_title(target.toplevel, "Capture target");
    uint32_t decoration_mode = 0;
    struct zxdg_toplevel_decoration_v1 *decoration =
        zxdg_decoration_manager_v1_get_toplevel_decoration(decoration_manager, target.toplevel);
    zxdg_toplevel_decoration_v1_add_listener(decoration, &decoration_listener, &decoration_mode);
    zxdg_toplevel_decoration_v1_set_mode(decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
    CHECK(wl_display_roundtrip(connection.display) >= 0 && target.configured, "capture: no xdg configure\n");
    CHECK(xdg_toplevel_client_ack_latest_configure(&connection, &target), "capture: ack configure failed\n");
    struct shm_buffer main_buffer = {0}, sub_buffers[4] = {{0}};
    CHECK(buffer_create(target.shm, &main_buffer, MAIN_WIDTH, MAIN_HEIGHT, WL_SHM_FORMAT_ARGB8888, main_color),
          "capture: main buffer failed\n");
    struct wl_surface *sub_surfaces[4];
    struct wl_subsurface *subs[4];
    const int sub_widths[] = {70, 50, 30, 50};
    const int sub_heights[] = {60, 25, 30, 40};
    const int sub_x[] = {-20, 45, 20, 205};
    const int sub_y[] = {30, -45, 45, 165};
    const uint32_t colors[] = {red, green, yellow, orange};
    for (int i = 0; i < 4; ++i) {
        sub_surfaces[i] = wl_compositor_create_surface(target.compositor);
        subs[i] = wl_subcompositor_get_subsurface(subcompositor, sub_surfaces[i],
                                                 i == 1 ? sub_surfaces[0] : target.surface);
        wl_subsurface_set_desync(subs[i]);
        wl_subsurface_set_position(subs[i], sub_x[i], sub_y[i]);
        if (i == 3)
            wl_subsurface_place_below(subs[i], target.surface);
        CHECK(buffer_create(target.shm, &sub_buffers[i], sub_widths[i], sub_heights[i],
                            WL_SHM_FORMAT_ARGB8888, colors[i]), "capture: subsurface buffer failed\n");
        attach(sub_surfaces[i], &sub_buffers[i]);
    }
    wl_surface_commit(sub_surfaces[0]); // Commit the nested subsurface position.
    xdg_surface_set_window_geometry(target.xdg_surface, 0, 0, MAIN_WIDTH, MAIN_HEIGHT);
    attach(target.surface, &main_buffer);
    CHECK(wl_display_roundtrip(connection.display) >= 0, "capture: map target failed\n");
    CHECK(xdg_toplevel_client_create_with_solid_buffer(&connection, &occluder, 120, 100, 0xffff00ff),
          "capture: create occluder failed\n");
    xdg_toplevel_set_app_id(occluder.toplevel, "capture-occluder");
    CHECK(wl_display_roundtrip(connection.display) >= 0 && foreign.target && foreign.target_done,
          "capture: no production foreign-toplevel handle\n");
    struct toplevel_capture_state server;
    CHECK(invoke_on_server_thread(toplevel_capture_prepare, &server), "capture: prepare fixture failed\n");
    CHECK(server.valid && server.titlebar_height > 0 && server.occluder_visible,
          "capture: target/SSD/occlusion fixture invalid (valid=%d title=%d occlusion=%d)\n",
          server.valid, server.titlebar_height, server.occluder_visible);

    // Control: the partly-offscreen scene must be stable without any capture
    // source or session, before testing whether capture can alter it.
    CHECK(invoke_on_server_thread(toplevel_capture_move_partly_offscreen, &server),
          "capture: control move failed\n");
    CHECK(invoke_on_server_thread(toplevel_capture_repaint_screen, &server)
          && server.geometry_unchanged && server.screen_unchanged && server.offscreen_viewports == 0,
          "capture: partly-offscreen control changed without capture (geometry=%d pixels=%d)\n",
           server.geometry_unchanged, server.screen_unchanged);
    const int positions[][2] = {{420, 300}, {-60, -20}, {620, 410}, {100, 90}};
    for (size_t i = 0; i < sizeof(positions) / sizeof(positions[0]); ++i) {
        struct toplevel_capture_move move = {.x = positions[i][0], .y = positions[i][1]};
        CHECK(invoke_on_server_thread(toplevel_capture_move_window, &move), "capture: control move failed\n");
        CHECK(invoke_on_server_thread(toplevel_capture_repaint_screen, &server)
              && server.screen_unchanged && server.geometry_unchanged && server.offscreen_viewports == 0,
              "capture: move control changed without capture at %d,%d\n", move.x, move.y);
    }
    CHECK(invoke_on_server_thread(toplevel_capture_prepare, &server),
          "capture: restore fixture failed\n");

    struct ext_image_capture_source_v1 *source =
        ext_foreign_toplevel_image_capture_source_manager_v1_create_source(source_manager, foreign.target);
    struct capture_session session;
    CHECK(session_create(&connection, &session, manager, source) == 0, "capture: session failed\n");
    CHECK(session.width == 275 && session.height == (uint32_t)(server.titlebar_height + 206),
          "capture: wrong window bounds %ux%u (title=%d)\n", session.width, session.height, server.titlebar_height);
    struct shm_buffer captured = {0};
    CHECK(buffer_create(target.shm, &captured, session.width, session.height, session.format, 0xabcdef01),
          "capture: destination buffer failed\n");
    CHECK(capture(&connection, &session, &captured, 0) == 0, "capture: initial frame failed\n");
    save_capture(&captured);
    CHECK(check_pixels(&captured, &server, green) == 0, "capture: initial pixels wrong\n");

    // Case: capture creation/rendering does not move/hide/transform the window
    // or change its actual output pixels, including the occluding other window.
    CHECK(check_screen(&server) == 0 && server.offscreen_viewports == 1,
          "capture: screen changed after initial capture\n");
    // Case: quiet requests wait, including across empty/frame-callback commits.
    // Declared content updates then exercise repeated translucent SSD painting.
    uint32_t *reference = malloc(captured.size);
    CHECK(reference, "capture: repeated-frame reference allocation failed\n");
    memcpy(reference, captured.pixels, captured.size);
    CHECK(invoke_on_server_thread(toplevel_capture_check_screen, &server),
          "capture: static baseline read failed\n");
    struct frame_state waiting;
    struct ext_image_copy_capture_frame_v1 *waiting_frame;
    for (int i = 0; i < 3; ++i) {
        const int paints = server.capture_paints;
        waiting_frame = begin_capture(&session, &captured, &waiting);
        CHECK(check_waiting(&connection, &session, &waiting, &server, paints) == 0,
              "capture: unchanged content did not wait\n");
        wl_surface_commit(target.surface);
        CHECK(check_waiting(&connection, &session, &waiting, &server, paints) == 0,
              "capture: empty commit woke a quiet frame\n");
        int callbacks = 0;
        struct wl_callback *callback = wl_surface_frame(target.surface);
        wl_callback_add_listener(callback, &callback_listener, &callbacks);
        wl_surface_commit(target.surface);
        CHECK(check_waiting(&connection, &session, &waiting, &server, paints) == 0 && callbacks == 1,
              "capture: frame callback commit woke a quiet frame or lost its callback\n");
        attach(sub_surfaces[1], &sub_buffers[1]);
        CHECK(finish_capture(&connection, &session, waiting_frame, &waiting, 0) == 0
              && same_pixels(&captured, reference),
              "capture: repeated frame accumulated translucent SSD pixels\n");
        CHECK(check_screen(&server) == 0, "capture: repeated frame changed actual output\n");
    }
    free(reference);

    // Case: an explicit paint without content changes must not manufacture damage.
    CHECK(invoke_on_server_thread(toplevel_capture_check_screen, &server), "capture: extra render baseline failed\n");
    const int extra_paints = server.capture_paints;
    waiting_frame = begin_capture(&session, &captured, &waiting);
    CHECK(check_waiting(&connection, &session, &waiting, &server, extra_paints) == 0,
          "capture: request before extra paint did not wait\n");
    CHECK(invoke_on_server_thread(toplevel_capture_render_offscreen, &server), "capture: extra render failed\n");
    CHECK(server.capture_only_did_not_commit && server.geometry_unchanged && server.screen_unchanged
          && server.render_state_unchanged,
          "capture: offscreen viewport committed to or altered the real output\n");
    CHECK(check_waiting(&connection, &session, &waiting, &server, extra_paints + 1) == 0,
          "capture: extra paint manufactured damage\n");
    attach(sub_surfaces[1], &sub_buffers[1]);
    CHECK(finish_capture(&connection, &session, waiting_frame, &waiting, 0) == 0,
          "capture: content update after extra paint did not wake the original request\n");

    // Case: cancelling only the frame keeps the session, but no longer paints updates.
    CHECK(invoke_on_server_thread(toplevel_capture_check_screen, &server), "capture: cancel baseline failed\n");
    const int cancelled_paints = server.capture_paints;
    waiting_frame = begin_capture(&session, &captured, &waiting);
    CHECK(check_waiting(&connection, &session, &waiting, &server, cancelled_paints) == 0,
          "capture: cancellable frame did not wait\n");
    ext_image_copy_capture_frame_v1_destroy(waiting_frame);
    attach(sub_surfaces[1], &sub_buffers[1]);
    CHECK(wl_display_roundtrip(connection.display) >= 0
          && invoke_on_server_thread(toplevel_capture_sync_output, &server)
          && server.capture_paints == cancelled_paints && server.offscreen_viewports == 1,
          "capture: cancelled frame rendered or destroyed its session pipeline\n");
    CHECK(capture(&connection, &session, &captured, 0) == 0,
          "capture: accumulated content change was lost between requests\n");

    // Case: genuine layer content still wakes requests despite ignoring texture refreshes.
    uint32_t *texture_reference = malloc(captured.size);
    CHECK(texture_reference, "capture: texture reference allocation failed\n");
    memcpy(texture_reference, captured.pixels, captured.size);
    CHECK(invoke_on_server_thread(toplevel_capture_check_screen, &server), "capture: texture baseline failed\n");
    waiting_frame = begin_capture(&session, &captured, &waiting);
    CHECK(check_waiting(&connection, &session, &waiting, &server, server.capture_paints) == 0,
          "capture: texture request did not wait\n");
    int recolored = 0;
    CHECK(invoke_on_server_thread(toplevel_capture_toggle_title_color, &recolored) && recolored,
          "capture: no private title background available\n");
    CHECK(finish_capture(&connection, &session, waiting_frame, &waiting, 0) == 0
          && pixel_is(&captured, 20 + 60, 1 + 10, 0xff00ff00, "layered title color") == 0,
          "capture: title background change did not wake the request or change its pixels\n");
    CHECK(invoke_on_server_thread(toplevel_capture_toggle_title_color, &recolored) && recolored
          && capture(&connection, &session, &captured, 0) == 0
          && same_pixels(&captured, texture_reference), "capture: title background restore failed\n");
    free(texture_reference);

    // Case: only a desynchronized nested subsurface changes; root does not
    // commit. Changed content must be painted before it is copied.
    struct shm_buffer nested_update = {0};
    CHECK(buffer_create(target.shm, &nested_update, 50, 25, WL_SHM_FORMAT_ARGB8888, teal),
          "capture: nested update failed\n");
    CHECK(invoke_on_server_thread(toplevel_capture_pause_checks, NULL), "capture: pause screen comparison failed\n");
    CHECK(invoke_on_server_thread(toplevel_capture_check_screen, &server), "capture: update baseline read failed\n");
    const int update_paints = server.capture_frames_checked;
    attach(sub_surfaces[1], &nested_update);
    CHECK(capture(&connection, &session, &captured, 0) == 0, "capture: subsurface-only frame failed\n");
    CHECK(check_pixels(&captured, &server, teal) == 0, "capture: subsurface-only update missing\n");
    CHECK(invoke_on_server_thread(toplevel_capture_check_screen, &server)
          && server.capture_frames_checked > update_paints,
          "capture: changed subsurface reused a stale image\n");

    reference = malloc(captured.size);
    CHECK(reference, "capture: reference allocation failed\n");
    memcpy(reference, captured.pixels, captured.size);

    // Case: moving partly outside the output neither clips nor changes capture coordinates.
    CHECK(invoke_on_server_thread(toplevel_capture_move_partly_offscreen, &server), "capture: fixture move failed\n");
    attach(sub_surfaces[1], &nested_update);
    CHECK(capture(&connection, &session, &captured, 0) == 0, "capture: moved frame failed\n");
    CHECK(check_pixels(&captured, &server, teal) == 0 && same_pixels(&captured, reference),
          "capture: move changed isolated image or SSD local coordinates\n");
    CHECK(check_screen(&server) == 0, "capture: partly-offscreen screen changed\n");

    // Case: compare every pixel, including SSD, across repeated live moves.
    for (size_t i = 0; i < sizeof(positions) / sizeof(positions[0]); ++i) {
        struct toplevel_capture_move move = {.x = positions[i][0], .y = positions[i][1]};
        CHECK(invoke_on_server_thread(toplevel_capture_move_window, &move), "capture: move fixture failed\n");
        server = move.state;
        CHECK(server.position_x == move.x && server.position_y == move.y,
              "capture: target did not move to %d,%d\n", move.x, move.y);
        attach(sub_surfaces[1], &nested_update);
        CHECK(capture(&connection, &session, &captured, 0) == 0, "capture: moving frame failed\n");
        CHECK(same_pixels(&captured, reference),
              "capture: window/SSD drifted after move to %d,%d\n", move.x, move.y);
        CHECK(check_screen(&server) == 0, "capture: live move damaged actual output\n");
    }
    free(reference);

    // Case: independent sessions share the source; stopping one must not freeze the other.
    struct capture_session other;
    const int shared_paints = server.capture_paints;
    waiting_frame = begin_capture(&session, &captured, &waiting);
    CHECK(check_waiting(&connection, &session, &waiting, &server, shared_paints) == 0,
          "capture: shared quiet frame did not wait\n");
    CHECK(session_create(&connection, &other, manager, source) == 0, "capture: second session failed\n");
    struct frame_state cancelled_first;
    struct ext_image_copy_capture_frame_v1 *cancelled_frame = begin_capture(&other, &captured, &cancelled_first);
    ext_image_copy_capture_frame_v1_destroy(cancelled_frame);
    CHECK(check_waiting(&connection, &session, &waiting, &server, shared_paints) == 0,
          "capture: cancelled first frame left a shared render scheduled\n");
    CHECK(capture(&connection, &other, &captured, 0) == 0, "capture: second session initial frame failed\n");
    CHECK(wl_display_roundtrip(connection.display) >= 0 && !waiting.ready && !waiting.failed,
          "capture: another session's first frame manufactured damage\n");
    attach(sub_surfaces[1], &nested_update);
    CHECK(finish_capture(&connection, &session, waiting_frame, &waiting, 0) == 0,
          "capture: shared content did not wake the quiet session\n");
    // Case: a real swapchain acquire failure ends every pending frame, not the
    // sessions. The new commit makes the request require a paint; otherwise the
    // current image is copied without touching the exhausted swapchain.
    attach(sub_surfaces[1], &nested_update);
    CHECK(check_render_failure(&connection, &session, &other, &captured, &server) == 0
          && check_pixels(&captured, &server, teal) == 0 && check_screen(&server) == 0,
          "capture: shared sessions did not recover from render failure\n");
    ext_image_copy_capture_session_v1_destroy(session.session);
    CHECK(wl_display_roundtrip(connection.display) >= 0, "capture: destroy first session failed\n");
    CHECK(invoke_on_server_thread(toplevel_capture_pause_checks, NULL), "capture: pause screen comparison failed\n");
    attach(sub_surfaces[1], &sub_buffers[1]);
    CHECK(capture(&connection, &other, &captured, 0) == 0, "capture: stopping first session froze source\n");
    CHECK(check_pixels(&captured, &server, green) == 0, "capture: shared source update missing\n");

    // Case: resize re-advertises constraints; an old client buffer gets one
    // buffer_constraints failure and a newly sized buffer succeeds afterwards.
    struct shm_buffer resized = {0};
    CHECK(buffer_create(target.shm, &resized, 320, 260, WL_SHM_FORMAT_ARGB8888, main_color),
          "capture: resized source buffer failed\n");
    xdg_surface_set_window_geometry(target.xdg_surface, 0, 0, 320, 260);
    attach(target.surface, &resized);
    CHECK(wl_display_roundtrip(connection.display) >= 0, "capture: resize commit failed\n");
    CHECK(invoke_on_server_thread(toplevel_capture_save_screen, &server), "capture: save resized screen failed\n");
    CHECK(wl_display_roundtrip(connection.display) >= 0 && other.width == 341
          && other.height == (uint32_t)(server.titlebar_height + 262),
          "capture: resized constraints incorrect %ux%u\n", other.width, other.height);
    CHECK(capture(&connection, &other, &captured, 1) == 0, "capture: resize failure reason wrong\n");
    buffer_destroy(&captured);
    CHECK(buffer_create(target.shm, &captured, other.width, other.height, other.format, 0xabcdef01),
          "capture: resized capture buffer failed\n");
    CHECK(capture(&connection, &other, &captured, 0) == 0, "capture: resized frame failed\n");
    CHECK(pixel_is(&captured, 20 + 310, 1 + server.titlebar_height + 250, main_color, "resized main surface") == 0,
          "capture: resized viewport did not render the whole new window\n");
    CHECK(check_pixels(&captured, &server, green) == 0 && check_screen(&server) == 0,
          "capture: resize capture changed actual window geometry/pixels\n");

    // Case: a client-requested maximize (with production animation) followed
    // by real SSD dragging. Capture before and after the client's restore
    // buffer arrives, then throughout the still-active move transaction.
    reference = malloc(captured.size);
    const size_t restored_capture_size = captured.size;
    CHECK(reference, "capture: restore reference allocation failed\n");
    memcpy(reference, captured.pixels, captured.size);
    CHECK(invoke_on_server_thread(toplevel_capture_pause_checks, NULL), "capture: pause maximize checks failed\n");
    xdg_toplevel_set_maximized(target.toplevel);
    CHECK(wl_display_roundtrip(connection.display) >= 0, "capture: maximize request failed\n");
    CHECK(invoke_on_server_thread(toplevel_capture_wait_transition, &server), "capture: maximize transition failed\n");
    CHECK(wl_display_roundtrip(connection.display) >= 0 && target.configure_maximized
          && target.configure_width == 800 && target.configure_height == 600 - server.titlebar_height,
          "capture: maximize configure wrong (%dx%d state=%d)\n",
          target.configure_width, target.configure_height, target.configure_maximized);
    struct shm_buffer maximized = {0};
    CHECK(buffer_create(target.shm, &maximized, target.configure_width, target.configure_height,
                        WL_SHM_FORMAT_ARGB8888, main_color), "capture: maximized buffer failed\n");
    CHECK(commit_configured_buffer(&connection, &target, &maximized) == 0, "capture: maximize commit failed\n");
    CHECK(invoke_on_server_thread(toplevel_capture_save_screen, &server), "capture: maximize screen failed\n");
    CHECK(wl_display_roundtrip(connection.display) >= 0 && server.maximized && !server.border_visible
          && other.width == 820 && other.height == 600,
          "capture: maximize bounds wrong (%ux%u state=%d border=%d)\n",
          other.width, other.height, server.maximized, server.border_visible);
    CHECK(resize_destination(target.shm, &other, &captured) == 0, "capture: maximize destination failed\n");
    CHECK(capture(&connection, &other, &captured, 0) == 0 && check_pixels(&captured, &server, green) == 0,
          "capture: maximized image incomplete\n");
    CHECK(check_screen(&server) == 0, "capture: maximized capture changed screen\n");

    // Case: a visible SSD title wakes the pending request without a surface commit.
    CHECK(server.width > 8 * server.titlebar_height, "capture: title text has no visible area\n");
    uint32_t *title_reference = malloc(captured.size);
    CHECK(title_reference, "capture: title reference allocation failed\n");
    memcpy(title_reference, captured.pixels, captured.size);
    CHECK(invoke_on_server_thread(toplevel_capture_pause_checks, NULL), "capture: title fixture failed\n");
    waiting_frame = begin_capture(&other, &captured, &waiting);
    CHECK(check_waiting(&connection, &other, &waiting, &server, server.capture_paints) == 0,
          "capture: title frame did not wait\n");
    // Reorder existing glyphs rather than grow the GPU glyph atlas.
    xdg_toplevel_set_title(target.toplevel, "target Capture");
    CHECK(finish_capture(&connection, &other, waiting_frame, &waiting, 0) == 0
          && memcmp(captured.pixels, title_reference, captured.size) != 0,
          "capture: title change returned a stale image\n");
    xdg_toplevel_set_title(target.toplevel, "Capture target");
    CHECK(capture(&connection, &other, &captured, 0) == 0
          && same_pixels(&captured, title_reference)
          && invoke_on_server_thread(toplevel_capture_save_screen, &server),
          "capture: title restore failed\n");
    free(title_reference);

    CHECK(invoke_on_server_thread(toplevel_capture_drag_begin, &server), "capture: SSD drag failed\n");
    CHECK(wl_display_roundtrip(connection.display) >= 0 && !server.maximized && server.dragging
          && !target.configure_maximized && target.configure_width == 320 && target.configure_height == 260,
          "capture: SSD drag did not restore through production move path (max=%d drag=%d configure=%dx%d/%d)\n",
          server.maximized, server.dragging, target.configure_width, target.configure_height, target.configure_maximized);
    CHECK(invoke_on_server_thread(toplevel_capture_save_screen, &server), "capture: pending restore screen failed\n");
    CHECK(wl_display_roundtrip(connection.display) >= 0 && server.border_visible
          && other.width == 821 && other.height == 602,
          "capture: pending restore bounds clipped old committed content (%ux%u)\n", other.width, other.height);
    CHECK(resize_destination(target.shm, &other, &captured) == 0, "capture: pending restore destination failed\n");
    CHECK(capture(&connection, &other, &captured, 0) == 0 && check_pixels(&captured, &server, green) == 0,
          "capture: drag restore clipped content before client resize\n");
    CHECK(check_screen(&server) == 0, "capture: pending restore capture changed screen\n");

    CHECK(commit_configured_buffer(&connection, &target, &resized) == 0, "capture: restore buffer commit failed\n");
    CHECK(invoke_on_server_thread(toplevel_capture_save_screen, &server), "capture: restored screen failed\n");
    CHECK(wl_display_roundtrip(connection.display) >= 0 && other.width == 341
          && other.height == (uint32_t)(server.titlebar_height + 262),
          "capture: restore constraints retained maximized geometry (%ux%u)\n", other.width, other.height);
    CHECK(resize_destination(target.shm, &other, &captured) == 0, "capture: restore destination failed\n");
    CHECK(capture(&connection, &other, &captured, 0) == 0 && captured.size == restored_capture_size
          && same_pixels(&captured, reference),
          "capture: restored window or SSD is offset/truncated\n");
    CHECK(check_screen(&server) == 0, "capture: restored capture changed screen\n");

    const int drag_positions[][2] = {{430, 140}, {700, 410}, {130, 65}};
    for (size_t i = 0; i < sizeof(drag_positions) / sizeof(drag_positions[0]); ++i) {
        struct toplevel_capture_move move = {.x = drag_positions[i][0], .y = drag_positions[i][1]};
        CHECK(invoke_on_server_thread(toplevel_capture_drag_move, &move) && move.state.dragging,
              "capture: interactive move ended prematurely\n");
        CHECK(move.state.position_x != server.position_x || move.state.position_y != server.position_y,
              "capture: pointer motion did not move the restored window\n");
        server = move.state;
        attach(sub_surfaces[1], &sub_buffers[1]);
        CHECK(capture(&connection, &other, &captured, 0) == 0
              && same_pixels(&captured, reference),
              "capture: restored window/SSD drifted while dragging to %d,%d\n", move.x, move.y);
        CHECK(check_screen(&server) == 0, "capture: dragging capture damaged actual output\n");
    }
    CHECK(invoke_on_server_thread(toplevel_capture_drag_end, &server) && !server.dragging,
          "capture: pointer release did not end drag\n");
    attach(sub_surfaces[1], &sub_buffers[1]);
    CHECK(capture(&connection, &other, &captured, 0) == 0
          && same_pixels(&captured, reference) && check_screen(&server) == 0,
          "capture: released restore image changed\n");
    free(reference);

    // Case: stop/start in one batch keeps the source usable.
    ext_image_copy_capture_session_v1_destroy(other.session);
    CHECK(session_create(&connection, &other, manager, source) == 0,
          "capture: same-batch session restart failed\n");
    CHECK(invoke_on_server_thread(toplevel_capture_flush_source_deletions, &server)
          && server.capture_sources == 1 && server.offscreen_viewports == 1,
          "capture: recycle check removed a restarted source\n");
    CHECK(capture(&connection, &other, &captured, 0) == 0
          && check_pixels(&captured, &server, green) == 0,
          "capture: restarted session could not capture\n");

    // Case: the capture pipeline belongs to the requesting client, not to the
    // toplevel. Accepting a source must not allocate one, and the last session
    // plus the client's source resource must release it again.
    ext_image_copy_capture_session_v1_destroy(other.session);
    ext_image_capture_source_v1_destroy(source);
    CHECK(wl_display_roundtrip(connection.display) >= 0, "capture: source cleanup failed\n");
    CHECK(invoke_on_server_thread(toplevel_capture_flush_source_deletions, &server)
          && server.offscreen_viewports == 0 && server.capture_sources == 0,
          "capture: destroyed client kept its pipeline (%d sources, %d viewports)\n",
          server.capture_sources, server.offscreen_viewports);
    CHECK(check_screen(&server) == 0, "capture: released pipeline changed the output\n");

    // Case: an idle source is recycled without any session stop().
    struct ext_image_capture_source_v1 *idle_source =
        ext_foreign_toplevel_image_capture_source_manager_v1_create_source(source_manager, foreign.target);
    CHECK(idle_source, "capture: idle source creation failed\n");
    CHECK(wl_display_roundtrip(connection.display) >= 0
          && invoke_on_server_thread(toplevel_capture_flush_source_deletions, &server)
          && server.capture_sources == 1 && server.offscreen_viewports == 0,
          "capture: idle source allocated a viewport\n");
    ext_image_capture_source_v1_destroy(idle_source);
    CHECK(wl_display_roundtrip(connection.display) >= 0
          && invoke_on_server_thread(toplevel_capture_flush_source_deletions, &server)
          && server.capture_sources == 0 && server.offscreen_viewports == 0,
          "capture: idle source wrapper was not recycled\n");

    for (int i = 0; i < 3; ++i) {
        struct ext_image_capture_source_v1 *transient =
            ext_foreign_toplevel_image_capture_source_manager_v1_create_source(source_manager, foreign.target);
        CHECK(transient, "capture: transient source failed\n");
        CHECK(wl_display_roundtrip(connection.display) >= 0, "capture: transient roundtrip failed\n");
        CHECK(invoke_on_server_thread(toplevel_capture_check_screen, &server)
              && server.offscreen_viewports == 0 && server.capture_sources == 1,
              "capture: accepted source has unexpected objects (%d sources, %d viewports)\n",
              server.capture_sources, server.offscreen_viewports);

        struct capture_session transient_session;
        CHECK(session_create(&connection, &transient_session, manager, transient) == 0,
              "capture: transient session failed\n");
        CHECK(invoke_on_server_thread(toplevel_capture_check_screen, &server)
              && server.offscreen_viewports == 1,
              "capture: session did not create exactly one pipeline (%d viewports)\n",
              server.offscreen_viewports);
        CHECK(resize_destination(target.shm, &transient_session, &captured) == 0,
              "capture: transient destination failed\n");
        CHECK(capture(&connection, &transient_session, &captured, 0) == 0
              && check_pixels(&captured, &server, green) == 0,
              "capture: transient frame incomplete\n");

        if (i == 1) {
            // A session keeps working after the client dropped the source
            // resource; only the session still owns the pipeline.
            ext_image_capture_source_v1_destroy(transient);
            transient = NULL;
            CHECK(wl_display_roundtrip(connection.display) >= 0, "capture: source drop failed\n");
            CHECK(invoke_on_server_thread(toplevel_capture_flush_source_deletions, &server)
                  && server.offscreen_viewports == 1 && server.capture_sources == 1,
                  "capture: dropping the source resource released the live session (%d sources, %d viewports)\n",
                  server.capture_sources, server.offscreen_viewports);
            attach(sub_surfaces[1], &sub_buffers[1]);
            CHECK(capture(&connection, &transient_session, &captured, 0) == 0
                  && check_pixels(&captured, &server, green) == 0,
                  "capture: frame failed after source resource drop\n");
        }

        ext_image_copy_capture_session_v1_destroy(transient_session.session);
        if (transient)
            ext_image_capture_source_v1_destroy(transient);
        CHECK(wl_display_roundtrip(connection.display) >= 0, "capture: transient cleanup failed\n");
        CHECK(invoke_on_server_thread(toplevel_capture_flush_source_deletions, &server)
              && server.offscreen_viewports == 0 && server.capture_sources == 0,
              "capture: recycled pipeline still present after request %d (%d sources, %d viewports)\n",
              i, server.capture_sources, server.offscreen_viewports);
        CHECK(check_screen(&server) == 0, "capture: recycling changed the output\n");
    }

    source = ext_foreign_toplevel_image_capture_source_manager_v1_create_source(source_manager, foreign.target);
    CHECK(session_create(&connection, &session, manager, source) == 0, "capture: session recreate failed\n");
    CHECK(resize_destination(target.shm, &session, &captured) == 0, "capture: destination recreate failed\n");
    CHECK(capture(&connection, &session, &captured, 0) == 0, "capture: frame after recycle failed\n");

    // Case: a pending request survives its original output being powered off
    // after the real window has moved to another enabled output.
    struct wl_output *original_output = client_bind(&connection, "wl_output", &wl_output_interface, 1);
    struct zwlr_output_power_manager_v1 *power_manager = client_bind(&connection,
        zwlr_output_power_manager_v1_interface.name, &zwlr_output_power_manager_v1_interface, 1);
    CHECK(original_output && power_manager, "capture: output power globals missing\n");
    struct zwlr_output_power_v1 *power =
        zwlr_output_power_manager_v1_get_output_power(power_manager, original_output);
    struct power_state power_state = {.mode = ZWLR_OUTPUT_POWER_V1_MODE_OFF};
    zwlr_output_power_v1_add_listener(power, &power_listener, &power_state);
    CHECK(wl_display_roundtrip(connection.display) >= 0 && !power_state.failed
          && power_state.mode == ZWLR_OUTPUT_POWER_V1_MODE_ON,
          "capture: original output power control failed\n");
    struct toplevel_capture_output_state outputs;
    CHECK(invoke_on_server_thread(toplevel_capture_add_output, &outputs)
          && outputs.output_count == 2 && outputs.enabled_outputs == 2
          && outputs.target_on_other_output && outputs.viewport_unchanged,
          "capture: window did not move independently of its capture output\n");
    struct treeland_ddm_v1 *ddm = client_bind(&connection, treeland_ddm_v1_interface.name,
                                             &treeland_ddm_v1_interface, 1);
    CHECK(ddm, "capture: DDM global missing\n");
    treeland_ddm_v1_add_listener(ddm, &ddm_listener, NULL);

    // Case: pause/resume alone must not manufacture damage for a quiet request.
    CHECK(invoke_on_server_thread(toplevel_capture_check_screen, &server), "capture: quiet pause baseline failed\n");
    const int quiet_paints = server.capture_paints;
    waiting_frame = begin_capture(&session, &captured, &waiting);
    CHECK(check_waiting(&connection, &session, &waiting, &server, quiet_paints) == 0,
          "capture: quiet pause frame did not wait\n");
    CHECK(pause_rendering(&connection, ddm) == 0
          && invoke_on_server_thread(toplevel_capture_read_output_state, &outputs)
          && !outputs.render_enabled && !waiting.ready && !waiting.failed,
          "capture: quiet request did not remain paused\n");
    treeland_ddm_v1_enable_render(ddm);
    CHECK(check_waiting(&connection, &session, &waiting, &server, quiet_paints) == 0,
          "capture: resume manufactured damage\n");
    attach(sub_surfaces[1], &sub_buffers[1]);
    CHECK(finish_capture(&connection, &session, waiting_frame, &waiting, 0) == 0,
          "capture: content change after resume did not wake the original request\n");

    // Case: resume wakes a single request without new captures or fixture renders.
    attach(sub_surfaces[1], &sub_buffers[1]);
    CHECK(check_render_pause(&connection, ddm, &session, &captured, &outputs, 0) == 0
          && check_pixels(&captured, &server, green) == 0,
          "capture: enabled-output pause/resume failed\n");
    struct frame_state pending;
    // Commit first so the request needs a paint; a request on unchanged content
    // is answered from the current image and never reaches the output clock.
    attach(sub_surfaces[1], &nested_update);
    struct ext_image_copy_capture_frame_v1 *pending_frame = begin_capture(&session, &captured, &pending);
    zwlr_output_power_v1_set_mode(power, ZWLR_OUTPUT_POWER_V1_MODE_OFF);
    CHECK(finish_capture(&connection, &session, pending_frame, &pending, 0) == 0
          && power_state.mode == ZWLR_OUTPUT_POWER_V1_MODE_OFF && !power_state.failed,
          "capture: request issued before output disable stalled\n");
    CHECK(invoke_on_server_thread(toplevel_capture_read_output_state, &outputs)
          && !outputs.capture_output_enabled && outputs.enabled_outputs == 1
          && outputs.target_on_other_output && outputs.viewport_unchanged
          && outputs.disabled_frames > 0 && outputs.disabled_frames_did_not_commit,
          "capture: disabled-output fallback recreated the pipeline or committed an output\n");

    // Case: late SSD and subtree changes wake quiet requests on a disabled output.
    CHECK(invoke_on_server_thread(toplevel_capture_check_screen, &server), "capture: SSD baseline failed\n");
    uint32_t old_border = server.border_color;
    uint32_t new_border = 0xff00ff00;
    waiting_frame = begin_capture(&session, &captured, &waiting);
    CHECK(check_waiting(&connection, &session, &waiting, &server, server.capture_paints) == 0,
          "capture: disabled-output SSD request did not wait\n");
    CHECK(invoke_on_server_thread(toplevel_capture_set_border, &new_border), "capture: border change failed\n");
    CHECK(finish_capture(&connection, &session, waiting_frame, &waiting, 0) == 0
          && pixel_is(&captured, 20 + server.width, 1 + server.titlebar_height + 80,
                      new_border, "updated SSD border") == 0,
          "capture: disabled-output border change returned stale pixels\n");
    CHECK(invoke_on_server_thread(toplevel_capture_set_border, &old_border)
          && capture(&connection, &session, &captured, 0) == 0, "capture: border restore failed\n");
    CHECK(invoke_on_server_thread(toplevel_capture_check_screen, &server), "capture: removal baseline failed\n");
    waiting_frame = begin_capture(&session, &captured, &waiting);
    CHECK(check_waiting(&connection, &session, &waiting, &server, server.capture_paints) == 0,
          "capture: subsurface removal request did not wait\n");
    wl_subsurface_destroy(subs[2]);
    CHECK(finish_capture(&connection, &session, waiting_frame, &waiting, 0) == 0
          && pixel_is(&captured, 20 + 30, 1 + server.titlebar_height + 55, red,
                      "removed interior subsurface") == 0,
          "capture: interior subsurface removal returned stale pixels\n");
    subs[2] = wl_subcompositor_get_subsurface(subcompositor, sub_surfaces[2], target.surface);
    wl_subsurface_set_desync(subs[2]);
    wl_subsurface_set_position(subs[2], sub_x[2], sub_y[2]);
    attach(sub_surfaces[2], &sub_buffers[2]);
    wl_surface_commit(target.surface);
    CHECK(capture(&connection, &session, &captured, 0) == 0
          && pixel_is(&captured, 20 + 30, 1 + server.titlebar_height + 55, yellow,
                      "recreated interior subsurface") == 0,
          "capture: new subsurface was not observed\n");

    // Case: later requests still copy new committed content, not the last frame
    // rendered before the old output stopped producing frame events.
    attach(sub_surfaces[1], &nested_update);
    CHECK(capture(&connection, &session, &captured, 0) == 0
          && check_pixels(&captured, &server, teal) == 0,
          "capture: new pixels missing after output disable\n");
    CHECK(invoke_on_server_thread(toplevel_capture_read_output_state, &outputs)
          && outputs.disabled_frames >= 2 && outputs.disabled_frames_did_not_commit
          && outputs.viewport_unchanged && outputs.offscreen_viewports == 1,
          "capture: repeated fallback changed the output or leaked a pipeline\n");
    attach(sub_surfaces[1], &nested_update);
    CHECK(check_render_pause(&connection, ddm, &session, &captured, &outputs, 0) == 0
          && check_pixels(&captured, &server, teal) == 0
          && outputs.disabled_frames_did_not_commit,
          "capture: disabled-output pause/resume failed\n");
    // The fallback is queued before disable_render in the same request batch.
    attach(sub_surfaces[1], &nested_update);
    CHECK(check_render_pause(&connection, ddm, &session, &captured, &outputs, 1) == 0
          && check_pixels(&captured, &server, teal) == 0 && outputs.disabled_frames_did_not_commit,
          "capture: queued fallback did not survive rendering pause\n");
    // Case: render failure and recovery also work without an enabled capture output.
    attach(sub_surfaces[1], &nested_update);
    CHECK(check_render_failure(&connection, &session, NULL, &captured, &server) == 0
          && check_pixels(&captured, &server, teal) == 0,
          "capture: disabled-output session did not recover from render failure\n");
    CHECK(invoke_on_server_thread(toplevel_capture_read_output_state, &outputs)
          && !outputs.capture_output_enabled && outputs.disabled_frames_did_not_commit
          && outputs.viewport_unchanged && outputs.offscreen_viewports == 1,
          "capture: failed fallback changed the output or capture pipeline\n");
    zwlr_output_power_v1_set_mode(power, ZWLR_OUTPUT_POWER_V1_MODE_ON);
    CHECK(wl_display_roundtrip(connection.display) >= 0
          && power_state.mode == ZWLR_OUTPUT_POWER_V1_MODE_ON && !power_state.failed,
          "capture: output enable failed\n");
    attach(sub_surfaces[1], &nested_update);
    CHECK(capture(&connection, &session, &captured, 0) == 0,
          "capture: regular scheduling did not resume after output enable\n");

    // Case: a queued fallback must not render a viewport that stop() released.
    zwlr_output_power_v1_set_mode(power, ZWLR_OUTPUT_POWER_V1_MODE_OFF);
    CHECK(wl_display_roundtrip(connection.display) >= 0
          && invoke_on_server_thread(toplevel_capture_read_output_state, &outputs)
          && !outputs.capture_output_enabled, "capture: second output disable failed\n");
    const int disabled_frames = outputs.disabled_frames;
    attach(sub_surfaces[1], &nested_update);
    pending_frame = begin_capture(&session, &captured, &pending);
    ext_image_copy_capture_frame_v1_destroy(pending_frame);
    ext_image_copy_capture_session_v1_destroy(session.session);
    ext_image_capture_source_v1_destroy(source);
    CHECK(wl_display_roundtrip(connection.display) >= 0
          && invoke_on_server_thread(toplevel_capture_read_output_state, &outputs)
          && outputs.offscreen_viewports == 0 && outputs.disabled_frames == disabled_frames,
          "capture: stopped session retained or rendered its queued fallback\n");

    // Case: a new session resolves the window's current, enabled output while
    // the previous session's output remains disabled.
    source = ext_foreign_toplevel_image_capture_source_manager_v1_create_source(source_manager, foreign.target);
    CHECK(session_create(&connection, &session, manager, source) == 0,
          "capture: new session after output disable failed\n");
    CHECK(capture(&connection, &session, &captured, 0) == 0
          && check_pixels(&captured, &server, teal) == 0,
          "capture: first frame after output disable failed\n");
    CHECK(invoke_on_server_thread(toplevel_capture_read_output_state, &outputs)
          && !outputs.capture_output_enabled && outputs.target_on_other_output
          && outputs.offscreen_viewports == 1 && outputs.enabled_capture_viewports == 1
          && outputs.disabled_frames == disabled_frames,
          "capture: new session reused the disabled output\n");

    // Case: cancelling a paused request releases its viewport before rendering resumes.
    CHECK(pause_rendering(&connection, ddm) == 0
          && invoke_on_server_thread(toplevel_capture_read_output_state, &outputs),
          "capture: cancellation pause failed\n");
    const int cancelled_attempts = outputs.render_attempts;
    pending_frame = begin_capture(&session, &captured, &pending);
    ext_image_copy_capture_frame_v1_destroy(pending_frame);
    ext_image_copy_capture_session_v1_destroy(session.session);
    ext_image_capture_source_v1_destroy(source);
    CHECK(wl_display_roundtrip(connection.display) >= 0
          && invoke_on_server_thread(toplevel_capture_read_output_state, &outputs)
          && outputs.offscreen_viewports == 0 && outputs.render_attempts == cancelled_attempts,
          "capture: paused cancellation retained or rendered its viewport\n");
    treeland_ddm_v1_enable_render(ddm);
    CHECK(wl_display_roundtrip(connection.display) >= 0
          && invoke_on_server_thread(toplevel_capture_read_output_state, &outputs)
          && outputs.render_attempts == cancelled_attempts && outputs.offscreen_viewports == 0,
          "capture: resume rendered a cancelled capture viewport\n");

    zwlr_output_power_v1_set_mode(power, ZWLR_OUTPUT_POWER_V1_MODE_ON);
    CHECK(wl_display_roundtrip(connection.display) >= 0
          && power_state.mode == ZWLR_OUTPUT_POWER_V1_MODE_ON && !power_state.failed,
          "capture: output restore failed\n");
    zwlr_output_power_v1_destroy(power);
    zwlr_output_power_manager_v1_destroy(power_manager);
    wl_output_destroy(original_output);

    source = ext_foreign_toplevel_image_capture_source_manager_v1_create_source(source_manager, foreign.target);
    CHECK(session_create(&connection, &session, manager, source) == 0,
          "capture: session after output disable cleanup failed\n");
    CHECK(capture(&connection, &session, &captured, 0) == 0,
          "capture: frame after output disable cleanup failed\n");

    // Case: stop/restart and then destroy the toplevel while capture remains active.
    CHECK(pause_rendering(&connection, ddm) == 0, "capture: destruction pause failed\n");
    pending_frame = begin_capture(&session, &captured, &pending);
    CHECK(wl_display_roundtrip(connection.display) >= 0 && !pending.ready && !pending.failed,
          "capture: destruction request did not remain paused\n");
    CHECK(invoke_on_server_thread(toplevel_capture_pause_checks, NULL), "capture: pause destruction checks failed\n");
    for (int i = 3; i >= 0; --i) {
        wl_subsurface_destroy(subs[i]);
        wl_surface_destroy(sub_surfaces[i]);
    }
    zxdg_toplevel_decoration_v1_destroy(decoration);
    xdg_toplevel_client_destroy(&target);
    CHECK(wl_display_roundtrip(connection.display) >= 0 && session.stopped
          && pending.failed == 1 && !pending.ready
          && pending.reason == EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_STOPPED,
          "capture: destroying toplevel did not stop capture\n");
    ext_image_copy_capture_frame_v1_destroy(pending_frame);
    CHECK(invoke_on_server_thread(toplevel_capture_read_output_state, &outputs),
          "capture: destruction state read failed\n");
    const int destroyed_attempts = outputs.render_attempts;
    treeland_ddm_v1_enable_render(ddm);
    CHECK(wl_display_roundtrip(connection.display) >= 0
          && invoke_on_server_thread(toplevel_capture_read_output_state, &outputs)
          && outputs.render_attempts == destroyed_attempts && outputs.offscreen_viewports == 0,
          "capture: resume rendered a destroyed window\n");
    ext_image_copy_capture_session_v1_destroy(session.session);
    ext_image_capture_source_v1_destroy(source);
    buffer_destroy(&captured);
    buffer_destroy(&main_buffer);
    buffer_destroy(&resized);
    buffer_destroy(&maximized);
    buffer_destroy(&nested_update);
    for (int i = 0; i < 4; ++i)
        buffer_destroy(&sub_buffers[i]);
    xdg_toplevel_client_destroy(&occluder);
    ext_foreign_toplevel_list_v1_destroy(list);
    ext_foreign_toplevel_image_capture_source_manager_v1_destroy(source_manager);
    ext_image_copy_capture_manager_v1_destroy(manager);
    zxdg_decoration_manager_v1_destroy(decoration_manager);
    wl_subcompositor_destroy(subcompositor);
    wl_proxy_destroy((struct wl_proxy *)ddm);
    client_disconnect(&connection);
    return 0;
}
