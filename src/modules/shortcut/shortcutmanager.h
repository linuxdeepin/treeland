// Copyright (C) 2023-2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>
#include "modules/shortcut/impl/shortcut_manager_impl.h"
#include <QObject>
#include <QQmlEngine>

class QAction;

WAYLIB_SERVER_BEGIN_NAMESPACE
class WServer;
WAYLIB_SERVER_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE

class ShortcutV1;

class ShortcutManagerV1
    : public QObject
    , public WAYLIB_SERVER_NAMESPACE::WServerInterface
{
    Q_OBJECT
public:
    explicit ShortcutManagerV1(QObject *parent = nullptr);
    QByteArrayView interfaceName() const override;

    QMap<QKeySequence, ShortcutV1*> &keyMap(uid_t uid);
    bool dispatchKeySequence(uid_t uid, const QKeySequence &sequence);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

Q_SIGNALS:
    void before_destroy();
    void requestCompositorAction(treeland_shortcut_v1_action action);

private Q_SLOTS:
    void onNewShortcut(treeland_shortcut_v1 *shortcut);

private:
    treeland_shortcut_manager_v1 *m_manager = nullptr;

    QMap<uid_t, QMap<QKeySequence, ShortcutV1*>> m_userKeyMap;
};

class ShortcutV1 : public QObject
{
    Q_OBJECT
public:
    explicit ShortcutV1(treeland_shortcut_v1 *shortcut, ShortcutManagerV1 *manager);
    ~ShortcutV1();

    void activate();
Q_SIGNALS:
    void before_destroy();

private Q_SLOTS:
    void handleBindKeySequence(const QKeySequence &keys);
    void handleBindSwipeGesture(SwipeGesture::Direction direction, uint finger);
    void handleBindHoldGesture(uint finger);
    void handleBindAction(treeland_shortcut_v1_action action);
    void handleBindWorkspaceSwipe(SwipeGesture::Direction direction, uint finger);
    void handleUnbind(uint binding_id);

private:
    uint newId();

    treeland_shortcut_v1 *m_shortcut;
    ShortcutManagerV1 *m_manager = nullptr;

    QList<treeland_shortcut_v1_action> m_actions;
    std::map<uint, std::function<void()>> m_bindingDeleters;
};
