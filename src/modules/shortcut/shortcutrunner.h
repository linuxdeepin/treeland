// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QObject>
#include <QTimer>
#include <qcoreevent.h>
#include "shortcutmanager.h"

class ShortcutRunner : public QObject
{
    Q_OBJECT
public:
    explicit ShortcutRunner(QObject *parent = nullptr);

public Q_SLOTS:
    void onActionTrigger(ShortcutAction action, const QString &name, bool isGesture, ShortcutController::KeyFlags keyFlags = {});
    void onActionProgress(ShortcutAction action, qreal progress, const QString &name);
    void onActionFinish(ShortcutAction action, const QString &name, bool isTriggered);

private Q_SLOTS:
    void onModifierReleased(QKeyEvent *event);
    void onQuickSwitchTimeout();

private:
    void updateWorkspaceSwipe(qreal cb);
    void finishWorkspaceSwipe();
    void taskswitchAction(bool isRepeat, bool isSameApp, bool isPrev);

    qreal m_desktopOffset = 0;
    int m_fromId = 0;
    int m_toId = 0;
    bool m_slideEnable = false;
    bool m_slideBounce = false;

    // Quick Alt+Tab: a short timer distinguishes a quick A↔B toggle
    // from the full task switcher.
    // First KeyPress → pre-switch to next window in MRU chain + start timer.
    // Modifier released before timer fires → quick switch confirmed.
    // Timer fires → show the full task switcher UI (normal switching).
    QTimer *m_quickSwitchTimer = nullptr;
    ShortcutAction m_currentAction;
    bool m_quickSwitchPending = false;
    quint32 m_taskAltCount = 0;
};
