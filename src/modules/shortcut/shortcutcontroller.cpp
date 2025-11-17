// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "shortcutcontroller.h"
#include "input/inputdevice.h"
#include "input/gestures.h"
#include "treeland-shortcut-manager-protocol.h"

ShortcutController::ShortcutController(QObject *parent)
    : QObject(parent)
{
}

ShortcutController::~ShortcutController()
{
    clear();
}

uint ShortcutController::registerKeySequence(const QString &name, const QKeySequence &sequence, uint mode, ShortcutAction action)
{
    if (m_deleters.contains(name)) {
        return TREELAND_SHORTCUT_MANAGER_V2_BIND_ERROR_NAME_CONFLICT;
    }

    if (sequence.count() != 1) {
        return TREELAND_SHORTCUT_MANAGER_V2_BIND_ERROR_INVALID_ARGUMENT;
    }

    switch (mode) {
    case TREELAND_SHORTCUT_MANAGER_V2_KEYBIND_MODE_KEY_PRESS:
    case TREELAND_SHORTCUT_MANAGER_V2_KEYBIND_MODE_KEY_PRESS_REPEAT: {
        auto &entry = m_keyPressMap[sequence];
        if (entry.contains(action)) {
            return TREELAND_SHORTCUT_MANAGER_V2_BIND_ERROR_DUPLICATE_BINDING;
        }
        entry.insert(action, std::make_pair(name, mode == TREELAND_SHORTCUT_MANAGER_V2_KEYBIND_MODE_KEY_PRESS_REPEAT));
        m_deleters[name] = [this, sequence, action]() {
            m_keyPressMap[sequence].remove(action);
        };
        return 0;
    }
    case TREELAND_SHORTCUT_MANAGER_V2_KEYBIND_MODE_KEY_RELEASE: {
        auto &entry = m_keyReleaseMap[sequence];
        if (entry.contains(action)) {
            return TREELAND_SHORTCUT_MANAGER_V2_BIND_ERROR_DUPLICATE_BINDING;
        }
        entry.insert(action, name);
        m_deleters[name] = [this, sequence, action]() {
            m_keyReleaseMap[sequence].remove(action);
        };
        return 0;
    }
    default:
        return TREELAND_SHORTCUT_MANAGER_V2_BIND_ERROR_INVALID_ARGUMENT;
    };

    Q_UNREACHABLE();
}

uint ShortcutController::registerSwipeGesture(const QString &name, uint finger, SwipeGesture::Direction direction, ShortcutAction action)
{
    if (m_deleters.contains(name)) {
        return TREELAND_SHORTCUT_MANAGER_V2_BIND_ERROR_NAME_CONFLICT;
    }

    if (direction == SwipeGesture::Direction::Invalid) {
        return TREELAND_SHORTCUT_MANAGER_V2_BIND_ERROR_INVALID_ARGUMENT;
    }

    auto gestureKey = std::make_pair(finger, direction);

    if (!m_gestures.contains(gestureKey)) {
        auto gesture = InputDevice::instance()->registerTouchpadSwipe(
            SwipeFeedBack {
                direction,
                finger,
                [this, gestureKey](bool triggered) {
                    for (const auto& [act, nm] : std::as_const(m_gesturemap[gestureKey]).asKeyValueRange()) {
                        emit actionFinished(act, nm, triggered);
                    }
                },
                [this, gestureKey](qreal progress) {
                    for (const auto& [act, nm] : std::as_const(m_gesturemap[gestureKey]).asKeyValueRange()) {
                        emit actionProgress(act, progress, nm);
                    }
                }
        });

        if (!gesture) {
            return TREELAND_SHORTCUT_MANAGER_V2_BIND_ERROR_INTERNAL_ERROR;
        }

        m_gestures[gestureKey] = gesture;
    }

    auto &entry = m_gesturemap[gestureKey];

    if (entry.contains(action)) {
        return TREELAND_SHORTCUT_MANAGER_V2_BIND_ERROR_DUPLICATE_BINDING;
    }

    entry.insert(action, name);
    m_deleters[name] = [this, gestureKey, action]() {
        auto &entry = m_gesturemap[gestureKey];
        entry.remove(action);
        if (entry.isEmpty()) {
            auto gesture = qobject_cast<SwipeGesture*>(m_gestures.take(gestureKey));
            if (gesture) {
                InputDevice::instance()->unregisterTouchpadSwipe(gesture);
            }
        }
    };

    return 0;
}

uint ShortcutController::registerHoldGesture(const QString &name, uint finger, ShortcutAction action)
{
    if (m_deleters.contains(name)) {
        return TREELAND_SHORTCUT_MANAGER_V2_BIND_ERROR_NAME_CONFLICT;
    }

    auto gestureKey = std::make_pair(finger, SwipeGesture::Direction::Invalid);

    if (!m_gestures.contains(gestureKey)) {
        auto gesture = InputDevice::instance()->registerTouchpadHold(
            HoldFeedBack {
                finger,
                nullptr,
                [this, gestureKey]() {
                    for (const auto& [act, nm] : std::as_const(m_gesturemap[gestureKey]).asKeyValueRange()) {
                        emit actionTriggered(act, nm, true);
                    }
                }
        });

        if (!gesture) {
            return TREELAND_SHORTCUT_MANAGER_V2_BIND_ERROR_INTERNAL_ERROR;
        }

        m_gestures[gestureKey] = gesture;
    }

    auto &entry = m_gesturemap[gestureKey];

    if (entry.contains(action)) {
        return TREELAND_SHORTCUT_MANAGER_V2_BIND_ERROR_DUPLICATE_BINDING;
    }

    entry.insert(action, name);
    m_deleters[name] = [this, gestureKey, action]() {
        auto &entry = m_gesturemap[gestureKey];
        entry.remove(action);
        if (entry.isEmpty()) {
            auto gesture = qobject_cast<HoldGesture*>(m_gestures.take(gestureKey));
            if (gesture) {
                InputDevice::instance()->unregisterTouchpadHold(gesture);
            }
        }
    };

    return 0;
}

void ShortcutController::unregisterShortcut(const QString &name)
{
    if (m_deleters.contains(name)) {
        m_deleters.take(name)();
    }
}

bool ShortcutController::dispatchKeyPress(const QKeySequence &sequence, bool repeat)
{
    if (m_keyPressMap.contains(sequence)) {
        for (const auto &[action, keybind] : std::as_const(m_keyPressMap[sequence]).asKeyValueRange()) {
            const auto& [name, canRepeat] = keybind;
            if (repeat && !canRepeat) {
                continue;
            }
            emit actionTriggered(action, name, false, repeat);
        }
        return true;
    }
    return false;
}

bool ShortcutController::dispatchKeyRelease(const QKeySequence &sequence)
{
    if (m_keyReleaseMap.contains(sequence)) {
        for (const auto &[action, name] : std::as_const(m_keyReleaseMap[sequence]).asKeyValueRange()) {
            emit actionTriggered(action, name, false);
        }
        return true;
    }
    return false;
}

void ShortcutController::clear()
{
    for (const auto& deleter : std::as_const(m_deleters)) {
        deleter();
    }
    m_deleters.clear();
}
