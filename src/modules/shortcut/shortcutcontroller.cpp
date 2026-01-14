// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "shortcutcontroller.h"

#include "qwayland-server-treeland-shortcut-manager-v2.h"

#include "common/treelandlogging.h"
#include "input/inputdevice.h"
#include "input/gestures.h"

#include <QKeyEvent>
#include <QKeySequence>

using BindError = QtWaylandServer::treeland_shortcut_manager_v2::bind_error;
using KeybindFlag = QtWaylandServer::treeland_shortcut_manager_v2::keybind_flag;
const uint KeybindFlagMask = static_cast<uint>(KeybindFlag::keybind_flag_repeat)
                             | static_cast<uint>(KeybindFlag::keybind_flag_key_press)
                             | static_cast<uint>(KeybindFlag::keybind_flag_key_release);

ShortcutController::ShortcutController(QObject *parent)
    : QObject(parent)
{
}

ShortcutController::~ShortcutController()
{
    clear();
}

uint ShortcutController::registerKey(const QString &name, const QString& key, uint keybindFlags, ShortcutAction action)
{
    if (m_deleters.contains(name)) {
        return BindError::bind_error_name_conflict;
    }

    // For all-modifier bindings: Ctrl is a valid modifier but not a valid key.
    auto keySeq = QKeySequence::fromString(key.endsWith("+Ctrl") ? key + "+Control"
                                                                                      : key,
                                                         QKeySequence::PortableText);
    if (keySeq.count() != 1) {
        return BindError::bind_error_invalid_argument;
    }
    auto keyComb = normalizeKeyCombination(keySeq[0]);
    if (keyComb == QKeyCombination(Qt::NoModifier, Qt::Key_unknown)) {
        return BindError::bind_error_invalid_argument;
    }
    int combined = keyComb.toCombined();

    if (keybindFlags & ~KeybindFlagMask) {
        return BindError::bind_error_invalid_argument;
    }

    auto &entry = m_keyMap[combined];
    if (entry.contains(action)) {
        const auto &[prevName, flags] = entry[action];
        m_deleters.remove(prevName);
        qCInfo(treelandShortcut) << "Overriding existing key binding of"
                                 << keySeq[0] << "for action" << static_cast<int>(action)
                                 << "by name" << prevName << "with new name" << name << "and flags" << keybindFlags;
    }
    m_keyMap[combined][action] = std::make_pair(name, keybindFlags);
    m_deleters[name] = [this, combined, action]() {
        m_keyMap[combined].remove(action);
    };
    return 0;
}

uint ShortcutController::registerSwipeGesture(const QString &name, uint finger, SwipeGesture::Direction direction, ShortcutAction action)
{
    if (m_deleters.contains(name)) {
        return BindError::bind_error_name_conflict;
    }

    if (direction == SwipeGesture::Direction::Invalid) {
        return BindError::bind_error_invalid_argument;
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
            return BindError::bind_error_internal_error;
        }

        m_gestures[gestureKey] = gesture;
    }

    auto &entry = m_gesturemap[gestureKey];

    if (entry.contains(action)) {
        return BindError::bind_error_duplicate_binding;
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
        return BindError::bind_error_name_conflict;
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
            return BindError::bind_error_internal_error;
        }

        m_gestures[gestureKey] = gesture;
    }

    auto &entry = m_gesturemap[gestureKey];

    if (entry.contains(action)) {
        return BindError::bind_error_duplicate_binding;
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

bool ShortcutController::dispatchKeyEvent(const QKeyEvent *kevent)
{
    auto combined = normalizeKeyCombination(kevent->keyCombination()).toCombined();
    uint keyFlags = (kevent->isAutoRepeat() ? static_cast<uint>(KeybindFlag::keybind_flag_repeat) : 0u)
                    | (kevent->type() == QEvent::KeyPress ? static_cast<uint>(KeybindFlag::keybind_flag_key_press) : 0u)
                    | (kevent->type() == QEvent::KeyRelease ? static_cast<uint>(KeybindFlag::keybind_flag_key_release) : 0u);
    if (m_keyMap.contains(combined)) {
        for (const auto &[action, keybind] : std::as_const(m_keyMap[combined]).asKeyValueRange()) {
            const auto& [name, bindFlags] = keybind;
            if ((bindFlags & keyFlags) != keyFlags) {
                continue;
            }
            emit actionTriggered(action, name, false, keyFlags);
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

constexpr QKeyCombination ShortcutController::normalizeKeyCombination(QKeyCombination combination) {
    Qt::KeyboardModifiers mods = combination.keyboardModifiers();
    Qt::Key key = combination.key(), nornalizedKey = Qt::Key_unknown;

    switch (key) {
        case Qt::Key_Control:
            mods |= Qt::ControlModifier;
            break;
        case Qt::Key_Shift:
            mods |= Qt::ShiftModifier;
            break;
        case Qt::Key_Alt:
            mods |= Qt::AltModifier;
            break;
        case Qt::Key_Meta:
        case Qt::Key_Super_L:
        case Qt::Key_Super_R:
            mods |= Qt::MetaModifier;
            break;
        default:
            nornalizedKey = key;
            break;
    }

    return QKeyCombination(mods, nornalizedKey);
}
