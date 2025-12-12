// Copyright (C) 2023-2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "shortcut_manager_impl.h"

#include "treeland-shortcut-manager-protocol.h"

#include "qwdisplay.h"
#include "wsocket.h"

#include <QKeySequence>

WAYLIB_SERVER_USE_NAMESPACE

#define SHORTCUT_MANAGEMENT_V2_VERSION 1

static treeland_shortcut_manager_v2 *shortcut_manager_from_resource(struct wl_resource *resource);

static void shortcut_manager_destroy(struct wl_client *client,
                                     struct wl_resource *resource)
{
    Q_UNUSED(client);

    wl_resource_destroy(resource);
}

static void shortcut_manager_handle_acquire(struct wl_client *client,
                                           struct wl_resource *resource)
{
    auto *manager = shortcut_manager_from_resource(resource);

    auto *socket = WSocket::get(client)->rootSocket();
    if (manager->ownerClients.contains(socket)) {
        wl_resource_post_error(resource,
                               TREELAND_SHORTCUT_MANAGER_V2_ERROR_OCCUPIED,
                               "Another client has already acquired the shortcut manager.");
        return;
    }

    manager->ownerClients.insert(socket, resource);
}

static void shortcut_manager_handle_bind_key(struct wl_client *client,
                                             struct wl_resource *resource,
                                             const char *name,
                                             const char *key_sequence,
                                             uint mode,
                                             const uint action)
{
    auto *manager = shortcut_manager_from_resource(resource);

    WSocket *socket = WSocket::get(client)->rootSocket();
    if (manager->ownerClients.value(socket, nullptr) != resource) {
        wl_resource_post_error(resource,
                               TREELAND_SHORTCUT_MANAGER_V2_ERROR_NOT_ACQUIRED,
                               "Client has not acquired the shortcut manager.");
        return;
    }

    Q_EMIT manager->requestBindKey(socket,
                                   QString::fromUtf8(name),
                                   QString::fromUtf8(key_sequence),
                                   mode,
                                   action);
}

static void shortcut_manager_handle_bind_swipe_gesture(struct wl_client *client,
                                                       struct wl_resource *resource,
                                                       const char *name,
                                                       const uint finger,
                                                       const uint direction,
                                                       const uint action)
{
    auto *manager = shortcut_manager_from_resource(resource);

    WSocket *socket = WSocket::get(client)->rootSocket();
    if (manager->ownerClients.value(socket, nullptr) != resource) {
        wl_resource_post_error(resource,
                               TREELAND_SHORTCUT_MANAGER_V2_ERROR_NOT_ACQUIRED,
                               "Client has not acquired the shortcut manager.");
        return;
    }

    Q_EMIT manager->requestBindSwipeGesture(socket,
                                            QString::fromUtf8(name),
                                            finger,
                                            direction,
                                            action);
}

static void shortcut_manager_handle_bind_hold_gesture(struct wl_client *client,
                                                      struct wl_resource *resource,
                                                      const char *name,
                                                      const uint finger,
                                                      const uint action)
{
    auto *manager = shortcut_manager_from_resource(resource);

    WSocket *socket = WSocket::get(client)->rootSocket();
    if (manager->ownerClients.value(socket, nullptr) != resource) {
        wl_resource_post_error(resource,
                               TREELAND_SHORTCUT_MANAGER_V2_ERROR_NOT_ACQUIRED,
                               "Client has not acquired the shortcut manager.");
        return;
    }

    Q_EMIT manager->requestBindHoldGesture(socket,
                                           QString::fromUtf8(name),
                                           finger,
                                           action);
}

static void shortcut_manager_handle_commit(struct wl_client *client,
                                           struct wl_resource *resource)
{
    auto *manager = shortcut_manager_from_resource(resource);

    WSocket *socket = WSocket::get(client)->rootSocket();
    if (manager->ownerClients.value(socket, nullptr) != resource) {
        wl_resource_post_error(resource,
                               TREELAND_SHORTCUT_MANAGER_V2_ERROR_NOT_ACQUIRED,
                               "Client has not acquired the shortcut manager.");
        return;
    }

    Q_EMIT manager->requestCommit(socket);
}

static void shortcut_manager_handle_unbind(struct wl_client *client,
                                           struct wl_resource *resource,
                                           const char *name)
{
    auto *manager = shortcut_manager_from_resource(resource);

    WSocket *socket = WSocket::get(client)->rootSocket();
    if (manager->ownerClients.value(socket, nullptr) != resource) {
        wl_resource_post_error(resource,
                               TREELAND_SHORTCUT_MANAGER_V2_ERROR_NOT_ACQUIRED,
                               "Client has not acquired the shortcut manager.");
        return;
    }

    Q_EMIT manager->requestUnregisterShortcut(socket, QString::fromUtf8(name));
}

struct treeland_shortcut_manager_v2_interface shortcut_manager_impl {
    .destroy = shortcut_manager_destroy,
    .acquire = shortcut_manager_handle_acquire,
    .bind_key = shortcut_manager_handle_bind_key,
    .bind_swipe_gesture = shortcut_manager_handle_bind_swipe_gesture,
    .bind_hold_gesture = shortcut_manager_handle_bind_hold_gesture,
    .commit = shortcut_manager_handle_commit,
    .unbind = shortcut_manager_handle_unbind,
};

static void shortcut_manager_resource_destroy(struct wl_resource *resource)
{
    auto *manager = shortcut_manager_from_resource(resource);

    for (auto it = manager->ownerClients.begin(); it != manager->ownerClients.end(); ) {
        if (it.value() == resource) {
            it = manager->ownerClients.erase(it);
        } else {
            ++it;
        }
    }
}

static void treeland_shortcut_manager_bind(struct wl_client *client,
                                           void *data,
                                           uint32_t version,
                                           uint32_t id)
{
    auto *manager = static_cast<treeland_shortcut_manager_v2 *>(data);
    struct wl_resource *resource =
        wl_resource_create(client, &treeland_shortcut_manager_v2_interface, version, id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource,
                                   &shortcut_manager_impl,
                                   manager,
                                   shortcut_manager_resource_destroy);
}

treeland_shortcut_manager_v2::~treeland_shortcut_manager_v2()
{
    Q_EMIT before_destroy();
    if (global) {
        wl_global_destroy(global);
        global = nullptr;
    }
}

treeland_shortcut_manager_v2 *treeland_shortcut_manager_v2::create(qw_display *display)
{
    auto *manager = new treeland_shortcut_manager_v2;
    if (!manager) {
        return nullptr;
    }

    manager->global = wl_global_create(display->handle(),
                                       &treeland_shortcut_manager_v2_interface,
                                       SHORTCUT_MANAGEMENT_V2_VERSION,
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

void treeland_shortcut_manager_v2::sendActivated(WSocket *socket, const QString& name, bool repeat)
{
    wl_resource *resource = ownerClients.value(socket, nullptr);
    if (!resource) {
        return;
    }

    treeland_shortcut_manager_v2_send_activated(resource, name.toUtf8().constData(), repeat);
}

void treeland_shortcut_manager_v2::sendCommitSuccess(WSocket *socket)
{
    wl_resource *resource = ownerClients.value(socket, nullptr);
    if (!resource) {
        return;
    }

    treeland_shortcut_manager_v2_send_commit_success(resource);
}

void treeland_shortcut_manager_v2::sendCommitFailure(WSocket *socket, const QString &name, uint error)
{
    wl_resource *resource = ownerClients.value(socket, nullptr);
    if (!resource) {
        return;
    }

    treeland_shortcut_manager_v2_send_commit_failure(resource, name.toUtf8().constData(), error);
}

void treeland_shortcut_manager_v2::sendInvalidCommit(WSocket *socket)
{
    wl_resource *resource = ownerClients.value(socket, nullptr);
    if (!resource) {
        return;
    }

    wl_resource_post_error(resource,
                           TREELAND_SHORTCUT_MANAGER_V2_ERROR_INVALID_COMMIT,
                           "Commit sent before last commit is processed.");
}

static treeland_shortcut_manager_v2 *shortcut_manager_from_resource(struct wl_resource *resource)
{
    assert(wl_resource_instance_of(resource,
                                   &treeland_shortcut_manager_v2_interface,
                                   &shortcut_manager_impl));
    auto *manager =
        static_cast<treeland_shortcut_manager_v2 *>(wl_resource_get_user_data(resource));
    assert(manager != nullptr);
    return manager;
}
