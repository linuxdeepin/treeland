// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "shortcutmanager.h"
#include "input/gestures.h"

#include <QMap>
#include <QKeySequence>
class Gesture;

class ShortcutController : public QObject
{
    Q_OBJECT
public:
    explicit ShortcutController(QObject *parent = nullptr);
    ~ShortcutController() override;

    uint registerKeySequence(const QString &name, const QKeySequence &sequence, uint mode, ShortcutAction action);
    uint registerSwipeGesture(const QString &name, uint finger, SwipeGesture::Direction direction, ShortcutAction action);
    uint registerHoldGesture(const QString &name, uint finger, ShortcutAction action);
    void unregisterShortcut(const QString &name);

    void clear();
    bool dispatchKeyPress(const QKeySequence &sequence, bool repeat);
    bool dispatchKeyRelease(const QKeySequence &sequence);

Q_SIGNALS:
    void actionTriggered(ShortcutAction action, const QString &name, bool isGesture, bool isRepeat = false);
    void actionProgress(ShortcutAction action, qreal progress, const QString &name);
    void actionFinished(ShortcutAction action, const QString &name, bool isTriggered);

private:
    QMap<QKeySequence, QMap<ShortcutAction, std::pair<QString, bool>>> m_keyPressMap;
    QMap<QKeySequence, QMap<ShortcutAction, QString>> m_keyReleaseMap;
    QMap<std::pair<uint, SwipeGesture::Direction>, QMap<ShortcutAction, QString>> m_gesturemap;
    QMap<std::pair<uint, SwipeGesture::Direction>, QObject*> m_gestures;
    QMap<QString, std::function<void()>> m_deleters;
};
