// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wseat.h>
#include <wserver.h>

#include <wayland-server-core.h>

#include <QObject>
#include <QPointer>

WAYLIB_SERVER_USE_NAMESPACE

class ActiveNotifyV1Private;
class ActiveNotifyV1;
class TreelandActiveNotifyManagerInterfaceV1Private;

class TreelandActiveNotifyManagerInterfaceV1 : public QObject, public WServerInterface
{
    Q_OBJECT
public:
    explicit TreelandActiveNotifyManagerInterfaceV1(QObject *parent = nullptr);
    ~TreelandActiveNotifyManagerInterfaceV1() override;

    QByteArrayView interfaceName() const override;

    static constexpr int InterfaceVersion = 1;

Q_SIGNALS:
    void activeNotifyCreated(ActiveNotifyV1 *notify);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private:
    std::unique_ptr<TreelandActiveNotifyManagerInterfaceV1Private> d;
};

class ActiveNotifyV1 : public QObject
{
    Q_OBJECT
public:
    ~ActiveNotifyV1() override;

    enum Reason {
        Mouse = 0,
        Wheel = 1,
    };
    Q_ENUM(Reason)

    enum ActivityState {
        Inactive = 0,
        Active = 1,
    };
    Q_ENUM(ActivityState)

    enum DragState {
        Started = 0,
        Dropped = 1,
        Cancelled = 2,
    };
    Q_ENUM(DragState)

    WSeat *wSeat() const;

    void sendActivityChanged(Reason reason, ActivityState state);
    void sendDragChanged(DragState state);

    static void sendActivityChanged(Reason reason, ActivityState state, const WSeat *seat);
    static void sendDragChanged(DragState state, const WSeat *seat);

private:
    explicit ActiveNotifyV1(wl_resource *resource, WSeat *seat, QObject *parent = nullptr);

    friend class TreelandActiveNotifyManagerInterfaceV1Private;
    std::unique_ptr<ActiveNotifyV1Private> d;
};
