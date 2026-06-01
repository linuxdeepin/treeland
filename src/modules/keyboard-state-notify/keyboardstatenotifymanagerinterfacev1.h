// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wseat.h>
#include <wserver.h>

#include <wayland-server-core.h>

#include <QObject>

WAYLIB_SERVER_USE_NAMESPACE

class KeyboardStateWatcherV1Private;
class KeyboardStateWatcherV1;
class TreelandKeyboardStateNotifyManagerInterfaceV1Private;

class TreelandKeyboardStateNotifyManagerInterfaceV1 : public QObject, public WServerInterface
{
    Q_OBJECT
public:
    explicit TreelandKeyboardStateNotifyManagerInterfaceV1(QObject *parent = nullptr);
    ~TreelandKeyboardStateNotifyManagerInterfaceV1() override;

    QByteArrayView interfaceName() const override;

    static constexpr int InterfaceVersion = 1;

Q_SIGNALS:
    void keyboardStateWatcherCreated(KeyboardStateWatcherV1 *watcher);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private:
    std::unique_ptr<TreelandKeyboardStateNotifyManagerInterfaceV1Private> d;
};

class KeyboardStateWatcherV1 : public QObject
{
    Q_OBJECT
public:
    ~KeyboardStateWatcherV1() override;

    enum ModifierType {
        CapsLock = 1,
        NumLock = 2,
    };
    Q_DECLARE_FLAGS(ModifierTypes, ModifierType)
    Q_FLAG(ModifierTypes)

    enum WatchFlag {
        WatchLocked = 1,
        WatchUnlocked = 2,
    };
    Q_DECLARE_FLAGS(WatchFlags, WatchFlag)
    Q_FLAG(WatchFlags)

    enum ModifierState {
        Unlocked = 0,
        Locked = 1,
    };
    Q_ENUM(ModifierState)

    ModifierTypes modifiers() const;
    WatchFlags flags() const;

    wl_resource *seat() const;
    WSeat *wSeat() const;

    void sendStateChanged(uint32_t modifier, ModifierState state);
    void sendCurrentState(uint32_t modifier, ModifierState state);

private:
    explicit KeyboardStateWatcherV1(wl_resource *resource,
                                     wl_resource *seat,
                                     QObject *parent = nullptr);

private:
    friend class TreelandKeyboardStateNotifyManagerInterfaceV1Private;
    std::unique_ptr<KeyboardStateWatcherV1Private> d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(KeyboardStateWatcherV1::ModifierTypes)
Q_DECLARE_OPERATORS_FOR_FLAGS(KeyboardStateWatcherV1::WatchFlags)
