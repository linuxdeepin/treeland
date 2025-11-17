// Copyright (C) 2023-2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

// #include <wayland-server-core.h>
#include "treeland-shortcut-manager-protocol.h"
#include "input/gestures.h"

#include <qwdisplay.h>

#include <QList>
#include <QObject>

#include <functional>
#include <memory>
#include <unordered_map>

struct treeland_shortcut_manager_v1 : public QObject
{
    Q_OBJECT
public:
    ~treeland_shortcut_manager_v1();

    static treeland_shortcut_manager_v1 *create(QW_NAMESPACE::qw_display *display);
    
    wl_global *global{ nullptr };
    
Q_SIGNALS:
    void newShortcut(treeland_shortcut_v1 *shortcut);
    void before_destroy();
};

struct treeland_shortcut_v1 : public QObject
{
    Q_OBJECT
public:
    ~treeland_shortcut_v1();
    static treeland_shortcut_v1 *from_resource(struct wl_resource *resource);

    void resetWorkspaceSwipe();

    treeland_shortcut_manager_v1 *manager{ nullptr };

    wl_resource *resource{ nullptr };

    uid_t uid = 0;

    bool workspace_swipe = false;

Q_SIGNALS:
    void before_destroy();
    void requestBindKeySequence(const QKeySequence &sequence);
    void requestBindSwipeGesture(SwipeGesture::Direction direction, uint finger);
    void requestBindWorkspaceSwipe(SwipeGesture::Direction direction, uint finger);
    void requestBindHoldGesture(uint finger);
    void requestBindAction(treeland_shortcut_v1_action action);
    void requestUnbind(uint binding_id);
public Q_SLOTS:
    void sendActivated();
    void sendBindSuccess(uint binding_id);
    void sendErrorConflict();
private:
};
