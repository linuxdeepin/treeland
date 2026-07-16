// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "input/gestures.h"

#include <QMap>
#include <QKeyCombination>

class Gesture;
class QKeyEvent;

class ShortcutController : public QObject
{
    Q_OBJECT
public:
    // Values defined in treeland-shortcut-manager-v2 protocol
    enum class ShortcutAction : uint32_t {
        Notify                = 1,
        Workspace1            = 2,
        Workspace2            = 3,
        Workspace3            = 4,
        Workspace4            = 5,
        Workspace5            = 6,
        Workspace6            = 7,
        PrevWorkspace         = 8,
        NextWorkspace         = 9,
        ShowDesktop           = 10,
        Maximize              = 11,
        CancelMaximize        = 12,
        MoveWindow            = 13,
        CloseWindow           = 14,
        ShowWindowMenu        = 15,
        OpenMultiTaskView     = 16,
        CloseMultiTaskView    = 17,
        ToggleMultitaskView   = 18,
        ToggleFpsDisplay      = 19,
        Lockscreen            = 20,
        ShutdownMenu          = 21,
        Quit                  = 22,
        TaskSwitchNext        = 24,
        TaskSwitchPrev        = 25,
        TaskSwitchSameAppNext = 26,
        TaskSwitchSameAppPrev = 27,
    };
    Q_ENUM(ShortcutAction)
    static const char *actionName(ShortcutAction action);

    enum KeyFlag : uint32_t {
        None = 0,
        KeyPress = 0x1,
        KeyRelease = 0x2,
        Repeat = 0x4,
        All = KeyPress | KeyRelease | Repeat
    };
    Q_DECLARE_FLAGS(KeyFlags, KeyFlag)
    explicit ShortcutController(QObject *parent = nullptr);
    ~ShortcutController() override;

    uint registerKey(const QString &name, const QString& key, KeyFlags keybindFlags, ShortcutAction action);
    uint registerSwipeGesture(const QString &name, uint finger, SwipeGesture::Direction direction, ShortcutAction action);
    uint registerHoldGesture(const QString &name, uint finger, ShortcutAction action);
    void unregisterShortcut(const QString &name);

    void clear();
    bool dispatchKeyEvent(const QKeyEvent *event);
    static QKeyCombination normalizeKeyCombination(QKeyCombination combination);
    static bool isValidShortcutCombination(QKeyCombination combination);
    Qt::KeyboardModifiers modifierForAction(ShortcutAction action) const;

Q_SIGNALS:
    void actionTriggered(ShortcutAction action, const QString &name, bool isGesture, KeyFlags keyFlags = {});
    void actionProgress(ShortcutAction action, qreal progress, const QString &name);
    void actionFinished(ShortcutAction action, const QString &name, bool isTriggered);

private:

    QMap<int, QMap<ShortcutAction, std::pair<QString, KeyFlags>>> m_keyMap;
    QMap<std::pair<uint, SwipeGesture::Direction>, QMap<ShortcutAction, QString>> m_gesturemap;
    QMap<std::pair<uint, SwipeGesture::Direction>, QObject*> m_gestures;
    QMap<QString, std::function<void()>> m_deleters;
    QMap<ShortcutAction, int> m_actionCombinedMap;
};

// Convenience alias so existing bare `ShortcutAction` references keep working
using ShortcutAction = ShortcutController::ShortcutAction;

Q_DECLARE_OPERATORS_FOR_FLAGS(ShortcutController::KeyFlags)
