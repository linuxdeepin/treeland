// Copyright (C) 2023-2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "shortcut_manager_impl.h"

#include <QMetaObject>
#include <QKeySequence>

#define SHORTCUT_MANAGEMENT_V1_VERSION 2

using QW_NAMESPACE::qw_display;


static inline std::optional<treeland_shortcut_v1_action> treeland_shortcut_v1_action_from_uint(uint32_t raw)
{
    switch (raw) {
        case TREELAND_SHORTCUT_V1_ACTION_WORKSPACE_1:
        case TREELAND_SHORTCUT_V1_ACTION_WORKSPACE_2:
        case TREELAND_SHORTCUT_V1_ACTION_WORKSPACE_3:
        case TREELAND_SHORTCUT_V1_ACTION_WORKSPACE_4:
        case TREELAND_SHORTCUT_V1_ACTION_WORKSPACE_5:
        case TREELAND_SHORTCUT_V1_ACTION_WORKSPACE_6:
        case TREELAND_SHORTCUT_V1_ACTION_PREV_WORKSPACE:
        case TREELAND_SHORTCUT_V1_ACTION_NEXT_WORKSPACE:
        case TREELAND_SHORTCUT_V1_ACTION_SHOW_DESKTOP:
        case TREELAND_SHORTCUT_V1_ACTION_MAXIMIZE:
        case TREELAND_SHORTCUT_V1_ACTION_CANCEL_MAXIMIZE:
        case TREELAND_SHORTCUT_V1_ACTION_TOGGLE_MULTITASK_VIEW:
        case TREELAND_SHORTCUT_V1_ACTION_TOGGLE_FPS_DISPLAY:
        case TREELAND_SHORTCUT_V1_ACTION_LOCKSCREEN:
        case TREELAND_SHORTCUT_V1_ACTION_SHUTDOWN_MENU:
        case TREELAND_SHORTCUT_V1_ACTION_QUIT:
        case TREELAND_SHORTCUT_V1_ACTION_CLOSE_WINDOW:
        case TREELAND_SHORTCUT_V1_ACTION_SHOW_WINDOW_MENU:
        case TREELAND_SHORTCUT_V1_ACTION_TASKSWITCH_NEXT:
        case TREELAND_SHORTCUT_V1_ACTION_TASKSWITCH_PREV:
        case TREELAND_SHORTCUT_V1_ACTION_TASKSWITCH_QUICK_ADVANCE:
            return static_cast<treeland_shortcut_v1_action>(raw);
        default:
            return std::nullopt;
    }
}

static inline SwipeGesture::Direction treeland_shortcut_v1_direction_from_uint(uint32_t raw)
{
    switch (raw) {
        case TREELAND_SHORTCUT_V1_DIRECTION_DOWN:
            return SwipeGesture::Direction::Down;
        case TREELAND_SHORTCUT_V1_DIRECTION_UP:
            return SwipeGesture::Direction::Up;
        case TREELAND_SHORTCUT_V1_DIRECTION_LEFT:
            return SwipeGesture::Direction::Left;
        case TREELAND_SHORTCUT_V1_DIRECTION_RIGHT:
            return SwipeGesture::Direction::Right;
        default:
            return SwipeGesture::Direction::Invalid;
    }
}

static treeland_shortcut_manager_v1 *shortcut_manager_from_resource(struct wl_resource *resource);

static void treeland_shortcut_v1_bind_keys(struct wl_client *client,
                                             struct wl_resource *resource,
                                             const char *keystr)
{
    Q_UNUSED(client)

    auto *shortcut = treeland_shortcut_v1::from_resource(resource);
    if (!shortcut)
        return;

    if (shortcut->workspace_swipe) {
        treeland_shortcut_v1_send_bind_failure(resource, TREELAND_SHORTCUT_V1_BIND_ERROR_INVALID_SHORTCUT);
        return;
    }

    auto keys = QKeySequence(QString::fromUtf8(keystr));
    // we do not support multi-step shortcuts
    if (keys.count() != 1) {
        treeland_shortcut_v1_send_bind_failure(resource, TREELAND_SHORTCUT_V1_BIND_ERROR_INVALID_KEY_SEQUENCE);
        return;
    }

    Q_EMIT shortcut->requestBindKeySequence(keys);
}

static void treeland_shortcut_v1_bind_swipe_gesture(struct wl_client *client,
                                              struct wl_resource *resource,
                                              uint32_t finger,
                                              uint32_t direction)
{
    Q_UNUSED(client)

    auto *shortcut = treeland_shortcut_v1::from_resource(resource);
    if (!shortcut)
        return;

    if (shortcut->workspace_swipe) {
        treeland_shortcut_v1_send_bind_failure(resource, TREELAND_SHORTCUT_V1_BIND_ERROR_INVALID_SHORTCUT);
        return;
    }

    auto d = treeland_shortcut_v1_direction_from_uint(direction);
    if (!finger || d == SwipeGesture::Direction::Invalid) {
        treeland_shortcut_v1_send_bind_failure(resource, TREELAND_SHORTCUT_V1_BIND_ERROR_INVALID_DIRECTION);
        return;
    }

    Q_EMIT shortcut->requestBindSwipeGesture(d, finger);
}

static void treeland_shortcut_v1_bind_hold_gesture(struct wl_client *client,
                                                   struct wl_resource *resource,
                                                   uint32_t finger)
{
    Q_UNUSED(client)

    auto *shortcut = treeland_shortcut_v1::from_resource(resource);
    if (!shortcut)
        return;

    if (shortcut->workspace_swipe) {
        treeland_shortcut_v1_send_bind_failure(resource, TREELAND_SHORTCUT_V1_BIND_ERROR_INVALID_SHORTCUT);
        return;
    }

    if (!finger) {
        treeland_shortcut_v1_send_bind_failure(resource, TREELAND_SHORTCUT_V1_BIND_ERROR_INVALID_DIRECTION);
        return;
    }

    Q_EMIT shortcut->requestBindHoldGesture(finger);
}

static void treeland_shortcut_v1_bind_workspace_swipe(struct wl_client *client,
                                                      struct wl_resource *resource,
                                                      uint32_t finger,
                                                      uint32_t direction)
{
    Q_UNUSED(client)

    auto *shortcut = treeland_shortcut_v1::from_resource(resource);
    if (!shortcut)
        return;

    if (shortcut->workspace_swipe) {
        treeland_shortcut_v1_send_bind_failure(resource, TREELAND_SHORTCUT_V1_BIND_ERROR_INVALID_SHORTCUT);
        return;
    }

    auto d = treeland_shortcut_v1_direction_from_uint(direction);
    if (!finger || d == SwipeGesture::Direction::Invalid) {
        treeland_shortcut_v1_send_bind_failure(resource, TREELAND_SHORTCUT_V1_BIND_ERROR_INVALID_SHORTCUT);
        return;
    }

    shortcut->workspace_swipe = true;
    Q_EMIT shortcut->requestBindWorkspaceSwipe(d, finger);
}

static void treeland_shortcut_v1_bind_action(struct wl_client *client,
                                             struct wl_resource *resource,
                                             uint32_t action)
{
    Q_UNUSED(client)
    auto *shortcut = treeland_shortcut_v1::from_resource(resource);
    if (!shortcut)
        return;

    if (shortcut->workspace_swipe) {
        treeland_shortcut_v1_send_bind_failure(resource, TREELAND_SHORTCUT_V1_BIND_ERROR_INVALID_SHORTCUT);
        return;
    }

    auto a = treeland_shortcut_v1_action_from_uint(action);
    if (!a) {
        treeland_shortcut_v1_send_bind_failure(resource, TREELAND_SHORTCUT_V1_BIND_ERROR_INVALID_ACTION);
        return;
    }

    Q_EMIT shortcut->requestBindAction(*a);
}

static void treeland_shortcut_v1_unbind(struct wl_client *client,
                                        struct wl_resource *resource,
                                        uint32_t binding_id)
{
    Q_UNUSED(client)

    auto *shortcut = treeland_shortcut_v1::from_resource(resource);
    if (!shortcut)
        return;

    Q_EMIT shortcut->requestUnbind(binding_id);
}

static void treeland_shortcut_v1_destroy([[maybe_unused]] struct wl_client *client,
                                              struct wl_resource *resource)
{
    wl_resource_destroy(resource);
}

static void treeland_shortcut_v1_resource_destroy(struct wl_resource *resource)
{
    if (!resource)
        return;

    auto *shortcut = treeland_shortcut_v1::from_resource(resource);
    if (!shortcut) {
        return;
    }

    delete shortcut;
}

static const struct treeland_shortcut_v1_interface shortcut_impl
{
    .bind_keys = treeland_shortcut_v1_bind_keys,
    .bind_swipe_gesture = treeland_shortcut_v1_bind_swipe_gesture,
    .bind_hold_gesture = treeland_shortcut_v1_bind_hold_gesture,
    .bind_workspace_swipe = treeland_shortcut_v1_bind_workspace_swipe,
    .bind_action = treeland_shortcut_v1_bind_action,
    .unbind = treeland_shortcut_v1_unbind,
    .destroy = treeland_shortcut_v1_destroy,
};

treeland_shortcut_v1::~treeland_shortcut_v1()
{
    manager = nullptr;

    Q_EMIT before_destroy();
}

struct treeland_shortcut_v1 *treeland_shortcut_v1::from_resource(struct wl_resource *resource)
{
    assert(wl_resource_instance_of(resource,
                                   &treeland_shortcut_v1_interface,
                                   &shortcut_impl));
    return static_cast<struct treeland_shortcut_v1 *>(
        wl_resource_get_user_data(resource));
}

void treeland_shortcut_v1::resetWorkspaceSwipe() {
    if (workspace_swipe)
        workspace_swipe = false;
}

void treeland_shortcut_v1::sendBindSuccess(uint binding_id)
{
    if (resource) {
        treeland_shortcut_v1_send_bind_success(resource, binding_id);
    }
}

void treeland_shortcut_v1::sendErrorConflict()
{
    if (resource) {
        treeland_shortcut_v1_send_bind_failure(resource, TREELAND_SHORTCUT_V1_BIND_ERROR_CONFLICT);
    }
}

void treeland_shortcut_v1::sendActivated()
{
    if (resource) {
        treeland_shortcut_v1_send_activated(resource);
    }
}

void treeland_shortcut_manager_v1_handle_create_shortcut(struct wl_client *client,
                                      struct wl_resource *manager_resource,
                                      uint32_t id)
{
    auto *manager = shortcut_manager_from_resource(manager_resource);

    auto *shortcut = new treeland_shortcut_v1;
    if (shortcut == nullptr) {
        wl_resource_post_no_memory(manager_resource);
        return;
    }

    uint32_t version = wl_resource_get_version(manager_resource);
    struct wl_resource *resource =
        wl_resource_create(client,
                           &treeland_shortcut_v1_interface,
                           version,
                           id);
    if (resource == nullptr) {
        delete shortcut;
        wl_resource_post_no_memory(manager_resource);
        return;
    }

    wl_resource_set_implementation(resource,
                                   &shortcut_impl,
                                   shortcut,
                                   treeland_shortcut_v1_resource_destroy);

    uid_t uid;
    wl_client_get_credentials(client, nullptr, &uid, nullptr);

    shortcut->manager = manager;
    shortcut->resource = resource;
    shortcut->uid = uid;

    Q_EMIT manager->newShortcut(shortcut);
}

static void treeland_shortcut_manager_v1_destroy([[maybe_unused]] struct wl_client *client,
                                                 struct wl_resource *resource)
{
    wl_resource_destroy(resource);
}

static void shortcut_manager_resource_destroy(struct wl_resource *resource)
{
    Q_UNUSED(resource);
}


static const struct treeland_shortcut_manager_v1_interface shortcut_manager_impl
{
    .register_shortcut = treeland_shortcut_manager_v1_handle_create_shortcut,
    .destroy = treeland_shortcut_manager_v1_destroy,
};

static treeland_shortcut_manager_v1 *shortcut_manager_from_resource(struct wl_resource *resource)
{
    assert(wl_resource_instance_of(resource,
                                   &treeland_shortcut_manager_v1_interface,
                                   &shortcut_manager_impl));
    auto *manager =
        static_cast<treeland_shortcut_manager_v1 *>(wl_resource_get_user_data(resource));
    assert(manager != nullptr);
    return manager;
}

treeland_shortcut_manager_v1::~treeland_shortcut_manager_v1()
{
    Q_EMIT before_destroy();
    if (global)
        wl_global_destroy(global);
}

static void treeland_shortcut_manager_bind(struct wl_client *client,
                                           void *data,
                                           uint32_t version,
                                           uint32_t id)
{
    auto *manager = static_cast<treeland_shortcut_manager_v1 *>(data);
    struct wl_resource *resource =
        wl_resource_create(client, &treeland_shortcut_manager_v1_interface, version, id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource,
                                   &shortcut_manager_impl,
                                   manager,
                                   shortcut_manager_resource_destroy);
}

treeland_shortcut_manager_v1 *treeland_shortcut_manager_v1::create(qw_display *display)
{
    auto *manager = new treeland_shortcut_manager_v1;
    if (!manager) {
        return nullptr;
    }

    manager->global = wl_global_create(display->handle(),
                                       &treeland_shortcut_manager_v1_interface,
                                       SHORTCUT_MANAGEMENT_V1_VERSION,
                                       manager,
                                       treeland_shortcut_manager_bind);
    if (!manager->global) {
        delete manager;
        return nullptr;
    }

    connect(display, &qw_display::before_destroy, manager, [manager]() {
        delete manager;
    });

    return manager;
}
