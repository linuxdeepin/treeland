// Copyright (C) 2023-2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "shortcutmanager.h"
#include "common/treelandlogging.h"
#include "shortcutcontroller.h"
#include "seat/helper.h"
#include "input/gestures.h"

#include "modules/shortcut/impl/shortcut_manager_impl.h"

#include "treeland-shortcut-manager-protocol.h"

#include <qwdisplay.h>
#include <optional>

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
    switch (direction) {
    case TREELAND_SHORTCUT_MANAGER_V2_DIRECTION_DOWN:
        return SwipeGesture::Direction::Down;
    case TREELAND_SHORTCUT_MANAGER_V2_DIRECTION_LEFT:
        return SwipeGesture::Direction::Left;
    case TREELAND_SHORTCUT_MANAGER_V2_DIRECTION_UP:
        return SwipeGesture::Direction::Up;
    case TREELAND_SHORTCUT_MANAGER_V2_DIRECTION_RIGHT:
        return SwipeGesture::Direction::Right;
    default:
        return SwipeGesture::Direction::Invalid;
    }
}

class ShortcutManagerV2Private {
public:
    ShortcutManagerV2Private(ShortcutManagerV2 *q) : q_ptr(q) {}

    uint updateShortcuts(const UserShortcuts& shortcuts, QString &failName);

    treeland_shortcut_manager_v2 *m_manager = nullptr;
    ShortcutController *m_controller = nullptr;

    QMap<WSocket*, UserShortcuts> m_shortcuts;
    QMap<WSocket*, UserShortcuts> m_pendingShortcuts;
    QMap<WSocket*, UserShortcuts> m_pendingCommittedShortcuts;
    QMap<WSocket*, QList<QString>> m_pendingDeletes;

    WSocket* m_activeSessionSocket = nullptr;

    ShortcutManagerV2 *q_ptr;
    Q_DECLARE_PUBLIC(ShortcutManagerV2)
};

uint ShortcutManagerV2Private::updateShortcuts(const UserShortcuts& shortcuts, QString &failName)
{
    uint status;
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

ShortcutManagerV2::ShortcutManagerV2(QObject *parent)
    : QObject(parent)
    , d_ptr(new ShortcutManagerV2Private(this))
{
    Q_D(ShortcutManagerV2);
    d->m_controller = new ShortcutController(this);
}

ShortcutManagerV2::~ShortcutManagerV2() = default;

void ShortcutManagerV2::create(WServer *server)
{
    Q_D(ShortcutManagerV2);
    d->m_manager = treeland_shortcut_manager_v2::create(server->handle());
    Q_ASSERT(d->m_manager);

    connect(d->m_manager, &treeland_shortcut_manager_v2::requestUnregisterShortcut,
            this, &ShortcutManagerV2::handleUnregisterShortcut);
    connect(d->m_manager, &treeland_shortcut_manager_v2::requestBindKey,
            this, &ShortcutManagerV2::handleBindKey);
    connect(d->m_manager, &treeland_shortcut_manager_v2::requestBindSwipeGesture,
            this, &ShortcutManagerV2::handleBindSwipeGesture);
    connect(d->m_manager, &treeland_shortcut_manager_v2::requestBindHoldGesture,
            this, &ShortcutManagerV2::handleBindHoldGesture);
    connect(d->m_manager, &treeland_shortcut_manager_v2::requestCommit,
            this, &ShortcutManagerV2::handleCommit);
}

void ShortcutManagerV2::destroy(WServer *server)
{
    Q_UNUSED(server);
    Q_EMIT before_destroy();
}

wl_global *ShortcutManagerV2::global() const
{
    Q_D(const ShortcutManagerV2);
    return d->m_manager->global;
}

QByteArrayView ShortcutManagerV2::interfaceName() const
{
    return "treeland_shortcut_manager_v2";
}

ShortcutController* ShortcutManagerV2::controller()
{
    Q_D(ShortcutManagerV2);
    return d->m_controller;
}

void ShortcutManagerV2::sendActivated(const QString& name, bool repeat)
{
    Q_D(ShortcutManagerV2);
    d->m_manager->sendActivated(d->m_activeSessionSocket, name, repeat);
}

void ShortcutManagerV2::onSessionChanged()
{
    QString commitFailName;
    Q_D(ShortcutManagerV2);
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
            d->m_manager->sendCommitSuccess(socket);
        } else {
            d->m_manager->sendCommitFailure(socket, commitFailName, status);
        }
    }
}

void ShortcutManagerV2::handleCommit(WSocket* sessionSocket)
{
    Q_D(ShortcutManagerV2);
    if (!d->m_pendingShortcuts.contains(sessionSocket)) {
        d->m_manager->sendCommitSuccess(sessionSocket);
        return;
    }

    if (sessionSocket != d->m_activeSessionSocket) {
        if (d->m_pendingCommittedShortcuts.contains(sessionSocket)) {
            d->m_manager->sendInvalidCommit(sessionSocket);
            return;
        }
        d->m_pendingCommittedShortcuts[sessionSocket] = d->m_pendingShortcuts.take(sessionSocket);
        return;
    }

    const auto pendingShortcuts = d->m_pendingShortcuts.take(sessionSocket);
    QString commitFailName;
    uint status = d->updateShortcuts(pendingShortcuts, commitFailName);
    if (!status) {
        d->m_shortcuts[sessionSocket].append(pendingShortcuts);
        d->m_manager->sendCommitSuccess(sessionSocket);
    } else {
        d->m_manager->sendCommitFailure(sessionSocket, commitFailName, status);
    }
}

void ShortcutManagerV2::handleBindHoldGesture(WSocket* sessionSocket, const QString& name, uint finger, uint action)
{
    Q_D(ShortcutManagerV2);
    d->m_pendingShortcuts[sessionSocket].holds.append(HoldShortcut{
        .name = name,
        .finger = finger,
        .action = static_cast<ShortcutAction>(action),
    });
}

void ShortcutManagerV2::handleBindSwipeGesture(WSocket* sessionSocket, const QString& name, uint finger, uint direction, uint action)
{
    Q_D(ShortcutManagerV2);
    d->m_pendingShortcuts[sessionSocket].swipes.append(SwipeShortcut{
        .name = name,
        .finger = finger,
        .direction = toSwipeDirection(direction),
        .action = static_cast<ShortcutAction>(action),
    });
}

void ShortcutManagerV2::handleBindKey(WSocket* sessionSocket,
                                      const QString& name,
                                      const QString& key,
                                      uint mode,
                                      uint action)
{
    Q_D(ShortcutManagerV2);
    d->m_pendingShortcuts[sessionSocket].keys.append(KeyShortcut{
        .mode = mode,
        .name = name,
        .key = key,
        .action = static_cast<ShortcutAction>(action),
    });
}

void ShortcutManagerV2::handleUnregisterShortcut(WSocket* sessionSocket, const QString& name)
{
    Q_D(ShortcutManagerV2);
    if (sessionSocket != d->m_activeSessionSocket) {
        d->m_pendingDeletes[sessionSocket].append(name);
        return;
    }

    d->m_controller->unregisterShortcut(name);
}
