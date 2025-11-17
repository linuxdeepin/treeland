// Copyright (C) 2023-2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "shortcutmanager.h"
#include "seat/helper.h"
#include "input/inputdevice.h"
#include "input/workspaceswipe.h"

#include "modules/shortcut/impl/shortcut_manager_impl.h"

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <qwdisplay.h>
#include <QAction>

#include <pwd.h>
#include <sys/socket.h>
#include <unistd.h>

ShortcutManagerV1::ShortcutManagerV1(QObject *parent)
    : QObject(parent)
{
}

void ShortcutManagerV1::onNewShortcut(treeland_shortcut_v1 *shortcut)
{
    auto shortcutV1 = new ShortcutV1(shortcut, this);
    connect(this, &ShortcutManagerV1::before_destroy, shortcutV1, &ShortcutV1::deleteLater);
}

void ShortcutManagerV1::create(WServer *server)
{
    m_manager = treeland_shortcut_manager_v1::create(server->handle());
    connect(m_manager, &treeland_shortcut_manager_v1::newShortcut, this, &ShortcutManagerV1::onNewShortcut);
}

void ShortcutManagerV1::destroy([[maybe_unused]] WServer *server)
{
    Q_EMIT before_destroy();
}

wl_global *ShortcutManagerV1::global() const
{
    return m_manager->global;
}

QByteArrayView ShortcutManagerV1::interfaceName() const
{
    return "treeland_shortcut_manager_v1";
}

QMap<QKeySequence, ShortcutV1*> &ShortcutManagerV1::keyMap(uid_t uid)
{
    return m_userKeyMap[uid];
}

bool ShortcutManagerV1::dispatchKeySequence(uid_t uid, const QKeySequence &sequence)
{
    auto currentKeyMap = keyMap(uid);
    if (currentKeyMap.contains(sequence)) {
        currentKeyMap[sequence]->activate();
        return true;
    }
    return false;
}

ShortcutV1::ShortcutV1(treeland_shortcut_v1 *shortcut, ShortcutManagerV1 *manager)
    : QObject(manager)
    , m_shortcut(shortcut)
    , m_manager(manager)
{
    connect(m_shortcut, &treeland_shortcut_v1::before_destroy, this, [this]() {
        this->deleteLater();
    });

    connect(m_shortcut,
            &treeland_shortcut_v1::requestBindKeySequence,
            this,
            &ShortcutV1::handleBindKeySequence);
    connect(m_shortcut, 
            &treeland_shortcut_v1::requestBindSwipeGesture,
            this,
            &ShortcutV1::handleBindSwipeGesture);
    connect(m_shortcut, 
            &treeland_shortcut_v1::requestBindHoldGesture,
            this,
            &ShortcutV1::handleBindHoldGesture);
    connect(m_shortcut, 
            &treeland_shortcut_v1::requestBindAction,
            this,
            &ShortcutV1::handleBindAction);
    connect(m_shortcut,
            &treeland_shortcut_v1::requestBindWorkspaceSwipe,
            this,
            &ShortcutV1::handleBindWorkspaceSwipe);
    connect(m_shortcut, 
            &treeland_shortcut_v1::requestUnbind,
            this,
            &ShortcutV1::handleUnbind);
}

ShortcutV1::~ShortcutV1()
{
    Q_EMIT before_destroy();

    std::map<uint, std::function<void()>> deletors;
    QList<treeland_shortcut_v1_action> actions;
    m_actions.swap(actions);
    deletors.swap(m_bindingDeleters);

    for (const auto &[binding_id, deletor] : deletors) {
        deletor();
    }
}

uint ShortcutV1::newId()
{
    static uint s_id = 1;
    assert(s_id != 0);
    return s_id++;
}

void ShortcutV1::handleBindKeySequence(const QKeySequence &keys)
{
    if (!m_shortcut || !m_manager)
        return;

    uid_t uid = m_shortcut->uid;
    auto &keyMap = m_manager->keyMap(uid);
    if (keyMap.contains(keys)) {
        m_shortcut->sendErrorConflict();
        return;
    }

    uint id = newId();
    keyMap[keys] = this;
    m_bindingDeleters.emplace(id, [this, keys, uid]() {
        if (m_manager) {
            auto &keyMap = m_manager->keyMap(uid);
            keyMap.remove(keys);
        }
    });
    m_shortcut->sendBindSuccess(id);
}

void ShortcutV1::handleBindSwipeGesture(SwipeGesture::Direction direction, uint finger)
{
    if (!m_shortcut || !m_manager)
        return;

    uint id = newId();
    auto gesture = InputDevice::instance()->registerTouchpadSwipe(
        SwipeFeedBack{
            .direction = direction,
            .fingerCount = finger,
            .actionCallback = [this]() {
                this->activate();
            },
            .progressCallback = nullptr,
        });
    
    m_bindingDeleters.emplace(id, [gesture]() {
        InputDevice::instance()->unregisterTouchpadSwipe(gesture);
    });
    m_shortcut->sendBindSuccess(id);
}

void ShortcutV1::handleBindHoldGesture(uint finger)
{
    if (!m_shortcut || !m_manager)
        return;

    uint id = newId();
    auto gesture = InputDevice::instance()->registerTouchpadHold(
        HoldFeedBack{
            .fingerCount = finger,
            .actionCallback = nullptr,
            .longProcessCallback = [this]() {
                this->activate();
            }
        });
    
    m_bindingDeleters.emplace(id, [gesture]() {
        InputDevice::instance()->unregisterTouchpadHold(gesture);
    });
    m_shortcut->sendBindSuccess(id);
}

void ShortcutV1::handleBindAction(treeland_shortcut_v1_action action)
{
    if (!m_shortcut || !m_manager)
        return;

    uint id = newId();
    m_actions.append(action);
    m_bindingDeleters.emplace(id, [this, action]() {
        m_actions.removeAll(action);
    });
    m_shortcut->sendBindSuccess(id);
}

void ShortcutV1::handleBindWorkspaceSwipe(SwipeGesture::Direction direction, uint finger) {
    if (!m_shortcut || !m_manager)
        return;

    uint id = newId();
    auto *workspaceSwipe = new WorkspaceSwipeGesture(direction, finger);
    m_bindingDeleters.emplace(id, [workspaceSwipe]() {
        workspaceSwipe->destroy();
    });

}

void ShortcutV1::handleUnbind(uint binding_id)
{
    if (!m_shortcut)
        return;

    auto it = m_bindingDeleters.find(binding_id);
    if (it != m_bindingDeleters.end()) {
        it->second();
        m_bindingDeleters.erase(it);
    }

    if (m_bindingDeleters.empty()) {
        m_shortcut->resetWorkspaceSwipe();
    }
}

void ShortcutV1::activate()
{
    if (!m_shortcut)
        return;

    for (const auto &action : m_actions) {
        Q_EMIT m_manager->requestCompositorAction(action);
    }

    m_shortcut->sendActivated();
}
