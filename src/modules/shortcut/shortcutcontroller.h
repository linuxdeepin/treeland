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
    // Values defined in treeland-shortcut-manager-unstable-v3 protocol
    enum class ShortcutAction : uint32_t {
        Notify                    = 0,
        Workspace1                = 1,
        Workspace2                = 2,
        Workspace3                = 3,
        Workspace4                = 4,
        Workspace5                = 5,
        Workspace6                = 6,
        Workspace7                = 7,
        Workspace8                = 8,
        Workspace9                = 9,
        Workspace10               = 10,
        Workspace11               = 11,
        Workspace12               = 12,
        PrevWorkspace             = 13,
        NextWorkspace             = 14,
        Maximize                  = 15,
        CancelMaximize            = 16,
        Minimize                  = 17,
        MoveWindow                = 18,
        ResizeWindow              = 19,
        CloseWindow               = 20,
        ShowWindowMenu            = 21,
        MoveWindowToPrevWorkspace = 22,
        MoveWindowToNextWorkspace = 23,
        ShowDesktop               = 24,
        OpenMultiTaskView         = 25,
        CloseMultiTaskView        = 26,
        ToggleMultitaskView       = 27,
        TaskSwitchNext            = 28,
        TaskSwitchPrev            = 29,
        TaskSwitchSameAppNext     = 30,
        TaskSwitchSameAppPrev     = 31,
        TileLeft                  = 32,
        TileRight                 = 33,
        TileTop                   = 34,
        TileBottom                = 35,
        TileTopLeft               = 36,
        TileTopRight              = 37,
        TileBottomLeft            = 38,
        TileBottomRight           = 39,
        ZoomIn                    = 40,
        ZoomOut                   = 41,
        ZoomReset                 = 42,
        ToggleFpsDisplay          = 43,
        Lockscreen                = 44,
        ShutdownMenu              = 45,
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
