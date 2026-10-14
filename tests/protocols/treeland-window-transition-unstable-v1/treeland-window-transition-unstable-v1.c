// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
/*
 * Coverage level P / I / E.
 *
 * The client drives treeland_window_transition_manager_v1 and reads back the
 * production WindowTransitionManagerInterfaceV1 pending set and the armed
 * SurfaceWindowTransition through the server bridge, so stored/associated
 * rectangles are proven rather than a dispatchable request.
 *
 * The activation scenarios need a real xdg-activation-v1 token: an originating
 * mapped toplevel attaches the rectangle to the token, then a target toplevel
 * activates the token before it maps (association, which also proves the armed
 * window is the unmapped target) or after it maps (the "activation arrived too
 * late" fallback that closes the rectangle).
 *
 * The protocol-error paths each use their own connection because
 * wl_resource_post_error tears the connection down.
 */

#include "client-connection.h"
#include "server-bridge-api.h"
#include "treeland-window-transition-unstable-v1-client-protocol.h"
#include "treeland-window-transition-unstable-v1.h"
#include "xdg-activation-v1-client-protocol.h"
#include "xdg-toplevel-client.h"

#include <stdio.h>
#include <string.h>

#define WINDOW_TRANSITION_MANAGER "treeland_window_transition_manager_v1"

struct rect_events {
    int closed_count;
};

static void rect_handle_closed(void *data, struct treeland_window_transition_rect_v1 *rect)
{
    (void)rect;
    ((struct rect_events *)data)->closed_count++;
}

static const struct treeland_window_transition_rect_v1_listener rect_listener = {
    .closed = rect_handle_closed,
};

static struct treeland_window_transition_rect_v1 *create_rect(
    struct treeland_window_transition_manager_v1 *manager,
    const char *token,
    struct rect_events *events)
{
    struct treeland_window_transition_rect_v1 *rect =
        treeland_window_transition_manager_v1_get_window_transition_rect(manager, token);
    if (rect)
        treeland_window_transition_rect_v1_add_listener(rect, &rect_listener, events);
    return rect;
}

static int read_state(const char *token, struct window_transition_server_state *state)
{
    memset(state, 0, sizeof(*state));
    snprintf(state->token, sizeof(state->token), "%s", token);
    if (!invoke_on_server_thread(window_transition_read_server_state, state))
        return 0;
    return state->manager_present;
}

/* 1 when the token is in the production pending set, 0 when it is not, -1 when
 * the production state could not be read at all. */
static int read_pending(const char *token)
{
    struct window_transition_server_state state;
    if (!read_state(token, &state))
        return -1;
    return state.pending;
}

/*
 * The associated rectangle is described by the production wrapper the manager
 * armed, so these assertions are about the real window transition rather than
 * about the request being dispatched.
 */
static int expect_association(const struct window_transition_server_state *state,
                              int expected_mapped)
{
    if (state->armed_count != 1) {
        TEST_ERROR("window-transition: expected exactly one armed transition, got %d\n",
                   state->armed_count);
        return 0;
    }
    if (!state->rect_present) {
        TEST_ERROR("window-transition: armed transition exposes no rectangle\n");
        return 0;
    }
    if (state->rect_mapped != expected_mapped) {
        TEST_ERROR("window-transition: armed transition mapped=%d, expected %d\n",
                   state->rect_mapped, expected_mapped);
        return 0;
    }
    if (!state->source_set) {
        TEST_ERROR("window-transition: armed transition has no source surface\n");
        return 0;
    }
    return 1;
}

static int expect_rect_size(const struct window_transition_server_state *state,
                            int width,
                            int height)
{
    if (!state->rect_present) {
        TEST_ERROR("window-transition: no associated rectangle to size\n");
        return 0;
    }
    if (state->rect_w != width || state->rect_h != height) {
        TEST_ERROR("window-transition: associated rect is %dx%d, expected %dx%d\n",
                   state->rect_w, state->rect_h, width, height);
        return 0;
    }
    return 1;
}

struct activation_token_state {
    int done;
    char token[128];
};

static void token_handle_done(void *data,
                              struct xdg_activation_token_v1 *token,
                              const char *token_string)
{
    (void)token;
    struct activation_token_state *state = data;
    state->done = 1;
    if (token_string)
        snprintf(state->token, sizeof(state->token), "%s", token_string);
}

static const struct xdg_activation_token_v1_listener token_listener = {
    .done = token_handle_done,
};

/*
 * Obtains a committed activation token string. `originating` may be NULL, which
 * leaves the token without the set_surface hint: the compositor then has no
 * surface to map the rectangle to and must discard the pending rect.
 */
static int obtain_token(struct client_connection *connection,
                        struct xdg_activation_v1 *activation,
                        struct wl_surface *originating,
                        char *token,
                        size_t token_size)
{
    struct xdg_activation_token_v1 *request = xdg_activation_v1_get_activation_token(activation);
    if (!request)
        return 0;

    struct activation_token_state state = { 0 };
    if (originating)
        xdg_activation_token_v1_set_surface(request, originating);
    xdg_activation_token_v1_add_listener(request, &token_listener, &state);
    xdg_activation_token_v1_commit(request);

    const int ok = wl_display_roundtrip(connection->display) >= 0
        && state.done && state.token[0] != '\0';
    if (ok)
        snprintf(token, token_size, "%s", state.token);
    xdg_activation_token_v1_destroy(request);
    return ok;
}

static int run_pending_and_events(const char *socket_name)
{
    struct client_connection connection = { 0 };
    if (!client_connect(&connection, socket_name))
        return 0;

    struct treeland_window_transition_manager_v1 *manager =
        client_bind(&connection,
                    WINDOW_TRANSITION_MANAGER,
                    &treeland_window_transition_manager_v1_interface,
                    treeland_window_transition_manager_v1_interface.version);
    if (!manager) {
        TEST_ERROR("window-transition: manager missing\n");
        client_disconnect(&connection);
        return 0;
    }

    int ok = 1;
    struct window_transition_server_state state;

    /* A rectangle created for a token is kept in the production pending set. */
    struct rect_events events_a = { 0 };
    struct treeland_window_transition_rect_v1 *rect_a =
        create_rect(manager, "treeland-transition-token-a", &events_a);
    if (!rect_a || wl_display_roundtrip(connection.display) < 0) {
        TEST_ERROR("window-transition: failed to create the first rectangle\n");
        ok = 0;
    } else if (!read_state("treeland-transition-token-a", &state)) {
        TEST_ERROR("window-transition: production manager not reachable\n");
        ok = 0;
    } else if (!state.pending) {
        TEST_ERROR("window-transition: pending rect for token-a not stored\n");
        ok = 0;
    } else if (state.armed_count != 0 || state.rect_present != 0) {
        TEST_ERROR("window-transition: a stored rect armed a transition by itself\n");
        ok = 0;
    } else if (events_a.closed_count) {
        TEST_ERROR("window-transition: pending rect closed prematurely\n");
        ok = 0;
    }

    /* Creating another rectangle for the same token closes the previous one. */
    struct rect_events events_first = { 0 };
    struct rect_events events_second = { 0 };
    struct treeland_window_transition_rect_v1 *rect_first = NULL;
    struct treeland_window_transition_rect_v1 *rect_second = NULL;
    if (ok) {
        rect_first = create_rect(manager, "treeland-transition-token-dup", &events_first);
        rect_second = create_rect(manager, "treeland-transition-token-dup", &events_second);
        if (!rect_first || !rect_second || wl_display_roundtrip(connection.display) < 0) {
            TEST_ERROR("window-transition: failed to create the superseded pair\n");
            ok = 0;
        } else if (events_first.closed_count != 1) {
            TEST_ERROR("window-transition: superseded rect got %d closed events, expected 1\n",
                       events_first.closed_count);
            ok = 0;
        } else if (events_second.closed_count) {
            TEST_ERROR("window-transition: replacement rect closed unexpectedly\n");
            ok = 0;
        }
    }

    /* closed is a one-shot notification; the superseded rect must not repeat it. */
    if (ok) {
        if (wl_display_roundtrip(connection.display) < 0) {
            ok = 0;
        } else if (events_first.closed_count != 1) {
            TEST_ERROR("window-transition: superseded rect repeated closed\n");
            ok = 0;
        }
    }

    /* Destroying the superseded rect must not drop the replacement from the
     * pending set: only the rect the manager currently holds for the token is
     * removed. */
    if (ok) {
        treeland_window_transition_rect_v1_destroy(rect_first);
        rect_first = NULL;
        if (wl_display_roundtrip(connection.display) < 0) {
            ok = 0;
        } else if (read_pending("treeland-transition-token-dup") != 1) {
            TEST_ERROR("window-transition: destroying the superseded rect dropped the"
                       " replacement\n");
            ok = 0;
        }
    }

    /* Destroying the client object removes it from the pending set. */
    struct rect_events events_idle = { 0 };
    struct treeland_window_transition_rect_v1 *rect_idle = NULL;
    if (ok) {
        rect_idle = create_rect(manager, "treeland-transition-token-idle", &events_idle);
        if (!rect_idle || wl_display_roundtrip(connection.display) < 0) {
            TEST_ERROR("window-transition: failed to create the recyclable rect\n");
            ok = 0;
        } else {
            treeland_window_transition_rect_v1_destroy(rect_idle);
            rect_idle = NULL;
            if (wl_display_roundtrip(connection.display) < 0) {
                ok = 0;
            } else if (read_pending("treeland-transition-token-idle") != 0) {
                TEST_ERROR("window-transition: destroyed rect still pending\n");
                ok = 0;
            }
        }
    }

    if (rect_idle)
        treeland_window_transition_rect_v1_destroy(rect_idle);
    if (rect_second)
        treeland_window_transition_rect_v1_destroy(rect_second);
    if (rect_first)
        treeland_window_transition_rect_v1_destroy(rect_first);
    if (rect_a)
        treeland_window_transition_rect_v1_destroy(rect_a);
    treeland_window_transition_manager_v1_destroy(manager);
    client_disconnect(&connection);
    return ok;
}

/*
 * E: once the rectangle is associated the object stays live. The armed
 * transition must follow later set_geometry calls, a 0x0 geometry must disable
 * it without dropping the association, and the source surface may be cleared
 * and attached again.
 */
struct associated_rect {
    struct wl_display *display;
    struct treeland_window_transition_rect_v1 *rect;
    struct wl_surface *source;
    struct rect_events *events;
    const char *token;
};

static int run_associated_rect_updates(const struct associated_rect *associated)
{
    struct window_transition_server_state state;
    int ok = 1;

    /* The client may update the geometry; the armed transition must follow the
     * latest value. */
    treeland_window_transition_rect_v1_set_geometry(associated->rect, 3, 4, 20, 30);
    if (wl_display_roundtrip(associated->display) < 0) {
        ok = 0;
    } else if (!read_state(associated->token, &state)) {
        ok = 0;
    } else if (!expect_association(&state, 0) || !expect_rect_size(&state, 20, 30)) {
        ok = 0;
    }

    /* A 0x0 geometry is the documented way to disable the rectangle: the
     * association stays alive, only the animation source goes away. */
    if (ok) {
        treeland_window_transition_rect_v1_set_geometry(associated->rect, 0, 0, 0, 0);
        if (wl_display_roundtrip(associated->display) < 0) {
            ok = 0;
        } else if (!read_state(associated->token, &state)) {
            ok = 0;
        } else if (state.armed_count != 0) {
            TEST_ERROR("window-transition: 0x0 geometry did not disable the rect\n");
            ok = 0;
        } else if (!state.rect_present) {
            TEST_ERROR("window-transition: 0x0 geometry dropped the association\n");
            ok = 0;
        } else if (!expect_rect_size(&state, 0, 0)) {
            ok = 0;
        }
    }

    /* ...and a positive size again re-enables it. */
    if (ok) {
        treeland_window_transition_rect_v1_set_geometry(associated->rect, 0, 0, 40, 40);
        if (wl_display_roundtrip(associated->display) < 0) {
            ok = 0;
        } else if (!read_state(associated->token, &state)) {
            ok = 0;
        } else if (!expect_association(&state, 0) || !expect_rect_size(&state, 40, 40)) {
            ok = 0;
        }
    }

    /* Passing null clears the source surface without disarming the rectangle. */
    if (ok) {
        treeland_window_transition_rect_v1_set_source_surface(associated->rect, NULL);
        if (wl_display_roundtrip(associated->display) < 0) {
            ok = 0;
        } else if (!read_state(associated->token, &state)) {
            ok = 0;
        } else if (state.source_set) {
            TEST_ERROR("window-transition: null source surface did not clear the source\n");
            ok = 0;
        } else if (state.armed_count != 1 || !state.rect_present) {
            TEST_ERROR("window-transition: clearing the source disarmed the rect\n");
            ok = 0;
        }
    }

    /* ...and the same surface may be attached again, repeatedly. */
    if (ok) {
        treeland_window_transition_rect_v1_set_source_surface(associated->rect, associated->source);
        treeland_window_transition_rect_v1_set_source_surface(associated->rect, associated->source);
        if (wl_display_roundtrip(associated->display) < 0) {
            ok = 0;
        } else if (!read_state(associated->token, &state)) {
            ok = 0;
        } else if (!expect_association(&state, 0) || !expect_rect_size(&state, 40, 40)) {
            ok = 0;
        } else if (associated->events->closed_count) {
            TEST_ERROR("window-transition: associated rect was closed during the updates\n");
            ok = 0;
        }
    }

    return ok;
}

/*
 * E: an originating toplevel attaches a rectangle (geometry + live source
 * surface) to an activation token, and a target toplevel activates that token
 * before it maps. The production manager must consume the pending rect and arm
 * the still unmapped target with the rectangle and the source surface.
 */
static int run_associate_on_activation(const char *socket_name)
{
    struct client_connection connection = { 0 };
    struct xdg_toplevel_client origin = { 0 };
    struct xdg_toplevel_client target = { 0 };
    struct wl_surface *source = NULL;
    struct treeland_window_transition_rect_v1 *rect = NULL;
    struct treeland_window_transition_rect_v1 *decoy = NULL;
    if (!client_connect(&connection, socket_name))
        return 0;

    struct treeland_window_transition_manager_v1 *manager =
        client_bind(&connection,
                    WINDOW_TRANSITION_MANAGER,
                    &treeland_window_transition_manager_v1_interface,
                    treeland_window_transition_manager_v1_interface.version);
    struct xdg_activation_v1 *activation =
        client_bind(&connection,
                    xdg_activation_v1_interface.name,
                    &xdg_activation_v1_interface,
                    xdg_activation_v1_interface.version);
    struct wl_compositor *compositor =
        client_bind(&connection, "wl_compositor", &wl_compositor_interface, 1);
    if (!manager || !activation || !compositor) {
        TEST_ERROR("window-transition: missing manager, activation or compositor\n");
        client_disconnect(&connection);
        return 0;
    }

    int ok = 1;
    char token[128];
    struct window_transition_server_state state;

    /* A: the mapped originating toplevel that owns the activation token. */
    if (!xdg_toplevel_client_create(&connection, &origin)) {
        TEST_ERROR("window-transition: failed to create the originating toplevel\n");
        ok = 0;
    }

    if (ok) {
        source = wl_compositor_create_surface(compositor);
        if (!source) {
            TEST_ERROR("window-transition: failed to create the source surface\n");
            ok = 0;
        }
    }

    if (ok && !obtain_token(&connection, activation, origin.surface, token, sizeof(token))) {
        TEST_ERROR("window-transition: failed to obtain activation token\n");
        ok = 0;
    }

    struct rect_events events = { 0 };
    if (ok) {
        rect = treeland_window_transition_manager_v1_get_window_transition_rect(manager, token);
        if (!rect) {
            TEST_ERROR("window-transition: failed to create the transition rect\n");
            ok = 0;
        } else {
            treeland_window_transition_rect_v1_add_listener(rect, &rect_listener, &events);
            treeland_window_transition_rect_v1_set_geometry(rect, 0, 0, 40, 40);
            treeland_window_transition_rect_v1_set_source_surface(rect, source);
            if (wl_display_roundtrip(connection.display) < 0) {
                TEST_ERROR("window-transition: rect configuration was rejected\n");
                ok = 0;
            }
        }
    }

    /* A second rectangle under an unrelated token must survive the activation:
     * the manager consumes the pending rect of this token only. */
    struct rect_events decoy_events = { 0 };
    if (ok) {
        decoy = create_rect(manager, "treeland-transition-token-decoy", &decoy_events);
        if (!decoy || wl_display_roundtrip(connection.display) < 0) {
            TEST_ERROR("window-transition: failed to create the decoy rectangle\n");
            ok = 0;
        }
    }

    if (ok) {
        if (!read_state(token, &state)) {
            TEST_ERROR("window-transition: production manager not reachable\n");
            ok = 0;
        } else if (!state.pending) {
            TEST_ERROR("window-transition: rect not pending before activation\n");
            ok = 0;
        } else if (state.armed_count != 0 || state.rect_present != 0) {
            TEST_ERROR("window-transition: a transition was armed before activation\n");
            ok = 0;
        }
    }

    /* B: the target toplevel, created but not mapped yet. */
    if (ok
        && (!xdg_toplevel_client_create_pending(&connection, &target)
            || wl_display_roundtrip(connection.display) < 0)) {
        TEST_ERROR("window-transition: failed to create the pending target\n");
        ok = 0;
    }

    if (ok) {
        xdg_activation_v1_activate(activation, token, target.surface);
        if (wl_display_roundtrip(connection.display) < 0) {
            TEST_ERROR("window-transition: activation was rejected\n");
            ok = 0;
        }
    }

    /* The pending rect is consumed and the still unmapped target is armed with
     * the rectangle and the live source surface. */
    if (ok) {
        if (!read_state(token, &state)) {
            TEST_ERROR("window-transition: production manager not reachable\n");
            ok = 0;
        } else if (state.pending) {
            TEST_ERROR("window-transition: pending rect survived association\n");
            ok = 0;
        } else if (!expect_association(&state, 0)) {
            ok = 0;
        } else if (!expect_rect_size(&state, 40, 40)) {
            ok = 0;
        } else if (events.closed_count) {
            TEST_ERROR("window-transition: associated rect was closed while the target lives\n");
            ok = 0;
        } else if (read_pending("treeland-transition-token-decoy") != 1) {
            TEST_ERROR("window-transition: association consumed an unrelated token's rect\n");
            ok = 0;
        } else if (decoy_events.closed_count) {
            TEST_ERROR("window-transition: decoy rect was closed by the activation\n");
            ok = 0;
        }
    }

    /* The rectangle stays live after the association. */
    if (ok) {
        const struct associated_rect associated = {
            .display = connection.display,
            .rect = rect,
            .source = source,
            .events = &events,
            .token = token,
        };
        ok = run_associated_rect_updates(&associated);
    }

    if (decoy)
        treeland_window_transition_rect_v1_destroy(decoy);
    if (rect)
        treeland_window_transition_rect_v1_destroy(rect);
    if (source)
        wl_surface_destroy(source);
    if (origin.surface)
        xdg_toplevel_client_destroy(&origin);
    if (target.surface)
        xdg_toplevel_client_destroy(&target);
    client_disconnect(&connection);
    return ok;
}

/*
 * E: the target is already mapped when the activation arrives. The compositor
 * falls back to its default animation, drops the pending rectangle and closes
 * it without arming any transition.
 */
static int run_closed_when_activation_late(const char *socket_name)
{
    struct client_connection connection = { 0 };
    struct xdg_toplevel_client origin = { 0 };
    struct xdg_toplevel_client target = { 0 };
    struct treeland_window_transition_rect_v1 *rect = NULL;
    if (!client_connect(&connection, socket_name))
        return 0;

    struct treeland_window_transition_manager_v1 *manager =
        client_bind(&connection,
                    WINDOW_TRANSITION_MANAGER,
                    &treeland_window_transition_manager_v1_interface,
                    treeland_window_transition_manager_v1_interface.version);
    struct xdg_activation_v1 *activation =
        client_bind(&connection,
                    xdg_activation_v1_interface.name,
                    &xdg_activation_v1_interface,
                    xdg_activation_v1_interface.version);
    if (!manager || !activation) {
        TEST_ERROR("window-transition: missing manager or activation\n");
        client_disconnect(&connection);
        return 0;
    }

    int ok = 1;
    char token[128];

    /* Both the origin and the target are mapped before the activation. */
    if (!xdg_toplevel_client_create(&connection, &origin)) {
        TEST_ERROR("window-transition: failed to create the originating toplevel\n");
        ok = 0;
    }
    if (ok && !xdg_toplevel_client_create(&connection, &target)) {
        TEST_ERROR("window-transition: failed to create the mapped target\n");
        ok = 0;
    }
    if (ok && !obtain_token(&connection, activation, origin.surface, token, sizeof(token))) {
        TEST_ERROR("window-transition: failed to obtain activation token\n");
        ok = 0;
    }

    struct rect_events events = { 0 };
    if (ok) {
        rect = treeland_window_transition_manager_v1_get_window_transition_rect(manager, token);
        if (!rect) {
            TEST_ERROR("window-transition: failed to create the transition rect\n");
            ok = 0;
        } else {
            treeland_window_transition_rect_v1_add_listener(rect, &rect_listener, &events);
            treeland_window_transition_rect_v1_set_geometry(rect, 0, 0, 40, 40);
            if (wl_display_roundtrip(connection.display) < 0) {
                TEST_ERROR("window-transition: rect configuration was rejected\n");
                ok = 0;
            }
        }
    }

    struct window_transition_server_state state;
    if (ok) {
        if (!read_state(token, &state)) {
            TEST_ERROR("window-transition: production manager not reachable\n");
            ok = 0;
        } else if (!state.pending) {
            TEST_ERROR("window-transition: rect not pending before late activation\n");
            ok = 0;
        }
    }

    if (ok) {
        /* target.surface is already mapped, so the compositor discards the
         * pending rect and must emit closed. */
        xdg_activation_v1_activate(activation, token, target.surface);
        if (wl_display_roundtrip(connection.display) < 0) {
            TEST_ERROR("window-transition: late activation was rejected\n");
            ok = 0;
        }
    }

    if (ok && events.closed_count != 1) {
        TEST_ERROR("window-transition: late activation got %d closed events, expected 1\n",
                   events.closed_count);
        ok = 0;
    }

    if (ok) {
        if (!read_state(token, &state)) {
            ok = 0;
        } else if (state.pending) {
            TEST_ERROR("window-transition: late activation left the rect pending\n");
            ok = 0;
        } else if (state.armed_count != 0 || state.rect_present != 0) {
            TEST_ERROR("window-transition: late activation armed a transition\n");
            ok = 0;
        }
    }

    if (rect)
        treeland_window_transition_rect_v1_destroy(rect);
    if (origin.surface)
        xdg_toplevel_client_destroy(&origin);
    if (target.surface)
        xdg_toplevel_client_destroy(&target);
    client_disconnect(&connection);
    return ok;
}

/*
 * E: the activation token carries no originating surface (the client never
 * called set_surface). The rectangle cannot be mapped to global coordinates,
 * so the compositor must discard the pending rect right away and close it
 * instead of keeping it until the pending sweep expires.
 */
static int run_discarded_without_origin_surface(const char *socket_name)
{
    struct client_connection connection = { 0 };
    struct xdg_toplevel_client target = { 0 };
    struct treeland_window_transition_rect_v1 *rect = NULL;
    if (!client_connect(&connection, socket_name))
        return 0;

    struct treeland_window_transition_manager_v1 *manager =
        client_bind(&connection,
                    WINDOW_TRANSITION_MANAGER,
                    &treeland_window_transition_manager_v1_interface,
                    treeland_window_transition_manager_v1_interface.version);
    struct xdg_activation_v1 *activation =
        client_bind(&connection,
                    xdg_activation_v1_interface.name,
                    &xdg_activation_v1_interface,
                    xdg_activation_v1_interface.version);
    if (!manager || !activation) {
        TEST_ERROR("window-transition: missing manager or activation\n");
        client_disconnect(&connection);
        return 0;
    }

    int ok = 1;
    char token[128];
    struct window_transition_server_state state;

    /* The target is a real toplevel but stays unmapped, so only the missing
     * originating surface can make the compositor drop the rect. */
    if (!xdg_toplevel_client_create_pending(&connection, &target)
        || wl_display_roundtrip(connection.display) < 0) {
        TEST_ERROR("window-transition: failed to create the pending target\n");
        ok = 0;
    }

    if (ok && !obtain_token(&connection, activation, NULL, token, sizeof(token))) {
        TEST_ERROR("window-transition: failed to obtain activation token\n");
        ok = 0;
    }

    struct rect_events events = { 0 };
    if (ok) {
        rect = treeland_window_transition_manager_v1_get_window_transition_rect(manager, token);
        if (!rect) {
            TEST_ERROR("window-transition: failed to create the transition rect\n");
            ok = 0;
        } else {
            treeland_window_transition_rect_v1_add_listener(rect, &rect_listener, &events);
            treeland_window_transition_rect_v1_set_geometry(rect, 0, 0, 40, 40);
            if (wl_display_roundtrip(connection.display) < 0) {
                TEST_ERROR("window-transition: rect configuration was rejected\n");
                ok = 0;
            }
        }
    }

    if (ok && read_pending(token) != 1) {
        TEST_ERROR("window-transition: rect without an originating surface is not pending\n");
        ok = 0;
    }

    if (ok) {
        xdg_activation_v1_activate(activation, token, target.surface);
        if (wl_display_roundtrip(connection.display) < 0) {
            TEST_ERROR("window-transition: activation was rejected\n");
            ok = 0;
        }
    }

    if (ok) {
        if (events.closed_count != 1) {
            TEST_ERROR("window-transition: rect without an originating surface got %d closed"
                       " events, expected 1\n",
                       events.closed_count);
            ok = 0;
        } else if (!read_state(token, &state)) {
            TEST_ERROR("window-transition: production manager not reachable\n");
            ok = 0;
        } else if (state.pending) {
            TEST_ERROR("window-transition: rect without an originating surface stayed pending\n");
            ok = 0;
        } else if (state.armed_count != 0 || state.rect_present != 0) {
            TEST_ERROR("window-transition: rect without an originating surface armed a"
                       " transition\n");
            ok = 0;
        }
    }

    if (rect)
        treeland_window_transition_rect_v1_destroy(rect);
    if (target.surface)
        xdg_toplevel_client_destroy(&target);
    client_disconnect(&connection);
    return ok;
}

static int run_invalid_geometry(const char *socket_name)
{
    struct client_connection connection = { 0 };
    if (!client_connect(&connection, socket_name))
        return 0;

    struct treeland_window_transition_manager_v1 *manager =
        client_bind(&connection,
                    WINDOW_TRANSITION_MANAGER,
                    &treeland_window_transition_manager_v1_interface,
                    treeland_window_transition_manager_v1_interface.version);
    if (!manager) {
        TEST_ERROR("window-transition: manager missing\n");
        client_disconnect(&connection);
        return 0;
    }

    struct treeland_window_transition_rect_v1 *rect =
        treeland_window_transition_manager_v1_get_window_transition_rect(
            manager, "treeland-transition-token-geometry");
    if (!rect) {
        TEST_ERROR("window-transition: failed to create the transition rect\n");
        client_disconnect(&connection);
        return 0;
    }

    /* A negative width or height is the invalid_geometry path. */
    treeland_window_transition_rect_v1_set_geometry(rect, 0, 0, -1, 4);

    const struct wl_interface *error_interface = NULL;
    uint32_t error_id = 0;
    const int rejected =
        wl_display_roundtrip(connection.display) < 0
        && wl_display_get_protocol_error(connection.display, &error_interface, &error_id)
               == TREELAND_WINDOW_TRANSITION_RECT_V1_ERROR_INVALID_GEOMETRY
        && error_interface == &treeland_window_transition_rect_v1_interface;
    if (!rejected)
        TEST_ERROR("window-transition: negative geometry did not raise invalid_geometry\n");
    client_disconnect(&connection);
    return rejected;
}

static int run_invalid_surface(const char *socket_name)
{
    struct client_connection connection = { 0 };
    struct xdg_toplevel_client toplevel = { 0 };
    if (!client_connect(&connection, socket_name))
        return 0;

    struct treeland_window_transition_manager_v1 *manager =
        client_bind(&connection,
                    WINDOW_TRANSITION_MANAGER,
                    &treeland_window_transition_manager_v1_interface,
                    treeland_window_transition_manager_v1_interface.version);
    if (!manager) {
        TEST_ERROR("window-transition: manager missing\n");
        client_disconnect(&connection);
        return 0;
    }

    /* A mapped toplevel already carries an xdg role, so it must be rejected as
     * a transition source surface. */
    if (!xdg_toplevel_client_create(&connection, &toplevel)) {
        TEST_ERROR("window-transition: failed to create the role-holding toplevel\n");
        treeland_window_transition_manager_v1_destroy(manager);
        client_disconnect(&connection);
        return 0;
    }

    struct treeland_window_transition_rect_v1 *rect =
        treeland_window_transition_manager_v1_get_window_transition_rect(
            manager, "treeland-transition-token-surface");
    if (!rect) {
        TEST_ERROR("window-transition: failed to create the transition rect\n");
        client_disconnect(&connection);
        return 0;
    }

    treeland_window_transition_rect_v1_set_source_surface(rect, toplevel.surface);

    const struct wl_interface *error_interface = NULL;
    uint32_t error_id = 0;
    const int rejected =
        wl_display_roundtrip(connection.display) < 0
        && wl_display_get_protocol_error(connection.display, &error_interface, &error_id)
               == TREELAND_WINDOW_TRANSITION_RECT_V1_ERROR_INVALID_SURFACE
        && error_interface == &treeland_window_transition_rect_v1_interface;
    if (!rejected)
        TEST_ERROR("window-transition: role-holding surface did not raise invalid_surface\n");
    client_disconnect(&connection);
    return rejected;
}

int protocol_test_run(const char *socket_name)
{
    int failed = 0;
    if (!run_pending_and_events(socket_name))
        failed = 1;
    if (!run_associate_on_activation(socket_name))
        failed = 1;
    if (!run_closed_when_activation_late(socket_name))
        failed = 1;
    if (!run_discarded_without_origin_surface(socket_name))
        failed = 1;
    if (!run_invalid_geometry(socket_name))
        failed = 1;
    if (!run_invalid_surface(socket_name))
        failed = 1;
    return failed;
}
