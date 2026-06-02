// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "shortcutcontroller.h"

#include "qwayland-server-treeland-shortcut-manager-v2.h"

#include "common/treelandlogging.h"
#include "input/inputdevice.h"
#include "input/gestures.h"

#include <QKeyEvent>
#include <QKeySequence>
#include <cstdint>

static_assert(
    static_cast<uint32_t>(ShortcutController::KeyPress) ==
        QtWaylandServer::treeland_shortcut_manager_v2::
            keybind_flag_key_press &&
    static_cast<uint32_t>(ShortcutController::KeyRelease) ==
        QtWaylandServer::treeland_shortcut_manager_v2::
            keybind_flag_key_release &&
    static_cast<uint32_t>(ShortcutController::Repeat) ==
        QtWaylandServer::treeland_shortcut_manager_v2::keybind_flag_repeat,
    "treeland-shortcut-manager-v2: protocol's keybind_flag disagree with "
    "Treeland in value");

using BindError = QtWaylandServer::treeland_shortcut_manager_v2::bind_error;

ShortcutController::ShortcutController(QObject *parent)
    : QObject(parent)
{
}

ShortcutController::~ShortcutController()
{
    clear();
}

uint ShortcutController::registerKey(const QString &name, const QString& key, ShortcutController::KeyFlags keybindFlags, ShortcutAction action)
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
    if (!isValidShortcutCombination(keyComb)) {
        return BindError::bind_error_invalid_argument;
    }
    int combined = keyComb.toCombined();

    if (keybindFlags & ~ShortcutController::KeyFlag::All) {
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
    m_actionCombinedMap[action] = combined;
    m_deleters[name] = [this, combined, action]() {
        m_keyMap[combined].remove(action);
        m_actionCombinedMap.remove(action);
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
    ShortcutController::KeyFlags keyFlags = (kevent->isAutoRepeat() ? ShortcutController::Repeat : ShortcutController::None)
        | (kevent->type() == QEvent::KeyPress ? ShortcutController::KeyPress : ShortcutController::None)
        | (kevent->type() == QEvent::KeyRelease ? ShortcutController::KeyRelease : ShortcutController::None);
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

Qt::KeyboardModifiers ShortcutController::modifierForAction(ShortcutAction action) const
{
    auto it = m_actionCombinedMap.find(action);
    if (it != m_actionCombinedMap.end()) {
        return QKeyCombination::fromCombined(it.value()).keyboardModifiers()
            & (Qt::AltModifier | Qt::MetaModifier | Qt::ControlModifier);
    }
    return Qt::NoModifier;
}

void ShortcutController::clear()
{
    for (const auto& deleter : std::as_const(m_deleters)) {
        deleter();
    }
    m_deleters.clear();
    m_actionCombinedMap.clear();
}

QKeyCombination ShortcutController::normalizeKeyCombination(QKeyCombination combination) {
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


// Decide whether a normalized (modifiers + key) combination is accepted as a
// valid shortcut. Mirrors dde-daemon's IsGood()/isGoodNoMods()/isGoodModShift().
bool ShortcutController::isValidShortcutCombination(QKeyCombination combination)
{
    const auto normalized = normalizeKeyCombination(combination);
    const Qt::KeyboardModifiers mods = normalized.keyboardModifiers()
        & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier | Qt::ShiftModifier);
    const Qt::Key key = normalized.key();

    // Modifier-only combinations: only Meta is accepted as a standalone shortcut.
    if (key == Qt::Key_unknown)
        return mods == Qt::MetaModifier;

    // Rule 1: Ctrl / Alt / Meta + any non-modifier key.
    if (mods & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))
        return true;

    // Rule 2: Function keys.
    if (key >= Qt::Key_F1 && key <= Qt::Key_F35)
        return true;

    // Rule 3: Misc function keys.
    switch (key) {
    case Qt::Key_Delete:
    case Qt::Key_Backspace:
    case Qt::Key_Pause:
    case Qt::Key_Print:
    case Qt::Key_Insert:
    case Qt::Key_Help:
    case Qt::Key_Menu:
    case Qt::Key_Cancel:
        return true;
    default:
        break;
    }

    // Rule 4: Shift + selected special/navigation keys.
    if (mods == Qt::ShiftModifier) {
        switch (key) {
        case Qt::Key_Space:
        case Qt::Key_Escape:
        case Qt::Key_Tab:
            return true;
        default:
            break;
        }

        if ((key >= Qt::Key_Home && key <= Qt::Key_PageDown)
            || (key >= Qt::Key_Left && key <= Qt::Key_Down))
            return true;
    }

    // Rule 5: Media / browser / hardware keys without modifiers.
    if (mods == Qt::NoModifier) {
        if (key >= Qt::Key_Back && key <= Qt::Key_KeyboardBrightnessDown)
            return true;
        if (key >= Qt::Key_LaunchMail && key <= Qt::Key_Launch9)
            return true;
        if (key >= Qt::Key_MediaPlay && key <= Qt::Key_MediaLast)
            return true;
    }

    return false;
}
