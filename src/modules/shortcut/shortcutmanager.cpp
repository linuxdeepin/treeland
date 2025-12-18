// Copyright (C) 2023-2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwayland-server-treeland-shortcut-manager-v2.h"

#include "shortcutmanager.h"
#include "common/treelandlogging.h"
#include "shortcutcontroller.h"
#include "seat/helper.h"
#include "input/gestures.h"

#include <qwdisplay.h>
#include <wsocket.h>

#include <optional>

#define TREELAND_SHORTCUT_MANAGER_V2_VERSION 1

using ProtocolAction = QtWaylandServer::treeland_shortcut_manager_v2::action;
static_assert(static_cast<int>(ProtocolAction::action_notify) == static_cast<int>(ShortcutAction::Notify),
              "treeland-shortcut-manager protocol action enum mismatch");
static_assert(static_cast<int>(ProtocolAction::action_taskswitch_sameapp_prev)
              == static_cast<int>(ShortcutAction::TaskSwitchSameAppPrev),
              "treeland-shortcut-manager protocol action enum mismatch");

struct KeyShortcut {
    uint mode;
    QString name;
    QString key;
    ShortcutAction action;
};

struct SwipeShortcut {
    QString name;
    uint finger;
    SwipeGesture::Direction direction;
    ShortcutAction action;
};

struct HoldShortcut {
    QString name;
    uint finger;
    ShortcutAction action;
};

class UserShortcuts {
public:
    QList<KeyShortcut> keys;
    QList<SwipeShortcut> swipes;
    QList<HoldShortcut> holds;

    void append(const UserShortcuts &other)
    {
        keys.append(other.keys);
        swipes.append(other.swipes);
        holds.append(other.holds);
    }
};

static SwipeGesture::Direction toSwipeDirection(uint32_t direction)
{
    using Direction = QtWaylandServer::treeland_shortcut_manager_v2::direction;
    switch (direction) {
    case Direction::direction_down:
        return SwipeGesture::Direction::Down;
    case Direction::direction_left:
        return SwipeGesture::Direction::Left;
    case Direction::direction_up:
        return SwipeGesture::Direction::Up;
    case Direction::direction_right:
        return SwipeGesture::Direction::Right;
    default:
        return SwipeGesture::Direction::Invalid;
    }
}

class ShortcutManagerV2Private : public QtWaylandServer::treeland_shortcut_manager_v2
{
public:
    explicit ShortcutManagerV2Private(ShortcutManagerV2 *_q);

    wl_global *global() const;

    uint updateShortcuts(const UserShortcuts& shortcuts, QString &failName);

    void sendActivated(WSocket *socket, const QString &name, bool repeat);
    void sendCommitSuccess(WSocket *socket);
    void sendCommitFailure(WSocket *socket, const QString &name, uint error);
    void sendInvalidCommit(WSocket *socket);

    ShortcutManagerV2 *q;
    ShortcutController *m_controller = nullptr;

    QMap<WSocket*, Resource*> ownerClients;
    QMap<WSocket*, UserShortcuts> m_shortcuts;
    QMap<WSocket*, UserShortcuts> m_pendingShortcuts;
    QMap<WSocket*, UserShortcuts> m_pendingCommittedShortcuts;
    QMap<WSocket*, QList<QString>> m_pendingDeletes;

    WSocket* m_activeSessionSocket = nullptr;

protected:
    void treeland_shortcut_manager_v2_destroy_resource(Resource *resource) override;
    void treeland_shortcut_manager_v2_destroy(Resource *resource) override;
    void treeland_shortcut_manager_v2_acquire(Resource *resource) override;
    void treeland_shortcut_manager_v2_bind_key(Resource *resource,
                                               const QString &name,
                                               const QString &key_sequence,
                                               uint32_t mode,
                                               uint32_t action) override;
    void treeland_shortcut_manager_v2_bind_swipe_gesture(Resource *resource,
                                                         const QString &name,
                                                         uint32_t finger,
                                                         uint32_t direction,
                                                         uint32_t action) override;
    void treeland_shortcut_manager_v2_bind_hold_gesture(Resource *resource,
                                                        const QString &name,
                                                        uint32_t finger,
                                                        uint32_t action) override;
    void treeland_shortcut_manager_v2_commit(Resource *resource) override;
    void treeland_shortcut_manager_v2_unbind(Resource *resource, const QString &name) override;

private:
    WSocket *socketFromResource(Resource *resource);
};

ShortcutManagerV2Private::ShortcutManagerV2Private(ShortcutManagerV2 *_q)
    : q(_q)
{
}

wl_global *ShortcutManagerV2Private::global() const
{
    return m_global;
}

WSocket *ShortcutManagerV2Private::socketFromResource(Resource *resource)
{
    return WSocket::get(wl_resource_get_client(resource->handle))->rootSocket();
}

uint ShortcutManagerV2Private::updateShortcuts(const UserShortcuts& shortcuts, QString &failName)
{
    uint status = 0;
    QList<QString> names;

    const auto tryRegisterAll = [&]() {
        for (const auto& [mode, name, key, action] : std::as_const(shortcuts.keys)) {
            status = m_controller->registerKey(name, key, mode, action);
            if (status) {
                failName = name;
                return;
            }
            names.append(name);
        }
        for (const auto& [name, finger, direction, action] : std::as_const(shortcuts.swipes)) {
            status = m_controller->registerSwipeGesture(name, finger, direction, action);
            if (status) {
                failName = name;
                return;
            }
            names.append(name);
        }
        for (const auto& [name, finger, action] : std::as_const(shortcuts.holds)) {
            status = m_controller->registerHoldGesture(name, finger, action);
            if (status) {
                failName = name;
                return;
            }
            names.append(name);
        }
    };

    tryRegisterAll();
    if (status) {
        for (const auto& name : std::as_const(names)) {
            m_controller->unregisterShortcut(name);
        }
    }
    return status;
}

void ShortcutManagerV2Private::sendActivated(WSocket *socket, const QString &name, bool repeat)
{
    Resource *resource = ownerClients.value(socket, nullptr);
    if (!resource)
        return;

    send_activated(resource->handle, name, repeat ? 1 : 0);
}

void ShortcutManagerV2Private::sendCommitSuccess(WSocket *socket)
{
    Resource *resource = ownerClients.value(socket, nullptr);
    if (!resource)
        return;

    send_commit_success(resource->handle);
}

void ShortcutManagerV2Private::sendCommitFailure(WSocket *socket, const QString &name, uint error)
{
    Resource *resource = ownerClients.value(socket, nullptr);
    if (!resource)
        return;

    send_commit_failure(resource->handle, name, error);
}

void ShortcutManagerV2Private::sendInvalidCommit(WSocket *socket)
{
    Resource *resource = ownerClients.value(socket, nullptr);
    if (!resource)
        return;

    wl_resource_post_error(resource->handle,
                           error_invalid_commit,
                           "Commit sent before last commit is processed.");
}

void ShortcutManagerV2Private::treeland_shortcut_manager_v2_destroy_resource(Resource *resource)
{
    for (auto it = ownerClients.begin(); it != ownerClients.end(); ) {
        if (it.value() == resource) {
            it = ownerClients.erase(it);
        } else {
            ++it;
        }
    }
}

void ShortcutManagerV2Private::treeland_shortcut_manager_v2_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ShortcutManagerV2Private::treeland_shortcut_manager_v2_acquire(Resource *resource)
{
    WSocket *socket = socketFromResource(resource);
    if (ownerClients.contains(socket)) {
        wl_resource_post_error(resource->handle,
                               error_occupied,
                               "Another client has already acquired the shortcut manager.");
        return;
    }

    ownerClients.insert(socket, resource);
}

void ShortcutManagerV2Private::treeland_shortcut_manager_v2_bind_key(Resource *resource,
                                                                     const QString &name,
                                                                     const QString &key_sequence,
                                                                     uint32_t mode,
                                                                     uint32_t action)
{
    WSocket *socket = socketFromResource(resource);
    if (ownerClients.value(socket, nullptr) != resource) {
        wl_resource_post_error(resource->handle,
                               error_not_acquired,
                               "Client has not acquired the shortcut manager.");
        return;
    }

    m_pendingShortcuts[socket].keys.append(KeyShortcut{
        .mode = mode,
        .name = name,
        .key = key_sequence,
        .action = static_cast<ShortcutAction>(action),
    });
}

void ShortcutManagerV2Private::treeland_shortcut_manager_v2_bind_swipe_gesture(Resource *resource,
                                                                               const QString &name,
                                                                               uint32_t finger,
                                                                               uint32_t direction,
                                                                               uint32_t action)
{
    WSocket *socket = socketFromResource(resource);
    if (ownerClients.value(socket, nullptr) != resource) {
        wl_resource_post_error(resource->handle,
                               error_not_acquired,
                               "Client has not acquired the shortcut manager.");
        return;
    }

    m_pendingShortcuts[socket].swipes.append(SwipeShortcut{
        .name = name,
        .finger = finger,
        .direction = toSwipeDirection(direction),
        .action = static_cast<ShortcutAction>(action),
    });
}

void ShortcutManagerV2Private::treeland_shortcut_manager_v2_bind_hold_gesture(Resource *resource,
                                                                              const QString &name,
                                                                              uint32_t finger,
                                                                              uint32_t action)
{
    WSocket *socket = socketFromResource(resource);
    if (ownerClients.value(socket, nullptr) != resource) {
        wl_resource_post_error(resource->handle,
                               error_not_acquired,
                               "Client has not acquired the shortcut manager.");
        return;
    }

    m_pendingShortcuts[socket].holds.append(HoldShortcut{
        .name = name,
        .finger = finger,
        .action = static_cast<ShortcutAction>(action),
    });
}

void ShortcutManagerV2Private::treeland_shortcut_manager_v2_commit(Resource *resource)
{
    WSocket *socket = socketFromResource(resource);
    if (ownerClients.value(socket, nullptr) != resource) {
        wl_resource_post_error(resource->handle,
                               error_not_acquired,
                               "Client has not acquired the shortcut manager.");
        return;
    }

    if (!m_pendingShortcuts.contains(socket)) {
        sendCommitSuccess(socket);
        return;
    }

    if (socket != m_activeSessionSocket) {
        if (m_pendingCommittedShortcuts.contains(socket)) {
            sendInvalidCommit(socket);
            return;
        }
        m_pendingCommittedShortcuts[socket] = m_pendingShortcuts.take(socket);
        return;
    }

    const auto pendingShortcuts = m_pendingShortcuts.take(socket);
    QString commitFailName;
    uint status = updateShortcuts(pendingShortcuts, commitFailName);
    if (!status) {
        m_shortcuts[socket].append(pendingShortcuts);
        sendCommitSuccess(socket);
    } else {
        sendCommitFailure(socket, commitFailName, status);
    }
}

void ShortcutManagerV2Private::treeland_shortcut_manager_v2_unbind(Resource *resource, const QString &name)
{
    WSocket *socket = socketFromResource(resource);
    if (ownerClients.value(socket, nullptr) != resource) {
        wl_resource_post_error(resource->handle,
                               error_not_acquired,
                               "Client has not acquired the shortcut manager.");
        return;
    }

    if (socket != m_activeSessionSocket) {
        m_pendingDeletes[socket].append(name);
        return;
    }

    m_controller->unregisterShortcut(name);
}

// ShortcutManagerV2 implementation

ShortcutManagerV2::ShortcutManagerV2(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<ShortcutManagerV2Private>(this))
{
    d->m_controller = new ShortcutController(this);
}

ShortcutManagerV2::~ShortcutManagerV2() = default;

void ShortcutManagerV2::create(WServer *server)
{
    d->init(server->handle()->handle(), TREELAND_SHORTCUT_MANAGER_V2_VERSION);
}

void ShortcutManagerV2::destroy(WServer *server)
{
    Q_UNUSED(server);
    Q_EMIT before_destroy();
}

wl_global *ShortcutManagerV2::global() const
{
    return d->global();
}

QByteArrayView ShortcutManagerV2::interfaceName() const
{
    return "treeland_shortcut_manager_v2";
}

ShortcutController* ShortcutManagerV2::controller()
{
    return d->m_controller;
}

void ShortcutManagerV2::sendActivated(const QString& name, bool repeat)
{
    d->sendActivated(d->m_activeSessionSocket, name, repeat);
}

void ShortcutManagerV2::onSessionChanged()
{
    QString commitFailName;
    auto session = Helper::instance()->activeSession().lock();
    if (!session) {
        return;
    }

    auto *socket = session->socket;

    if (d->m_activeSessionSocket == socket)
        return;

    d->m_controller->clear();
    d->m_activeSessionSocket = socket;

    if (d->m_shortcuts.contains(socket)) {
        uint status = d->updateShortcuts(d->m_shortcuts[socket], commitFailName);
        if (status) {
            qCWarning(treelandShortcut) << "Failed to restore shortcuts" << commitFailName
                                        << "by reason" << status
                                        << "for session" << session->id
                                        << "for user" << session->uid;
        }
        return;
    }

    if (d->m_pendingDeletes.contains(socket)) {
        const auto names = d->m_pendingDeletes.take(socket);
        for (const auto& name : std::as_const(names)) {
            d->m_controller->unregisterShortcut(name);
        }
    }

    if (d->m_pendingCommittedShortcuts.contains(socket)) {
        const auto pendingShortcuts = d->m_pendingCommittedShortcuts.take(socket);
        uint status = d->updateShortcuts(pendingShortcuts, commitFailName);
        if (!status) {
            d->m_shortcuts[socket].append(pendingShortcuts);
            d->sendCommitSuccess(socket);
        } else {
            d->sendCommitFailure(socket, commitFailName, status);
        }
    }
}
