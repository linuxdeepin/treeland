// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "activenotifymanagerinterfacev1.h"

#include "qwayland-server-treeland-active-notify-unstable-v1.h"

#include <cstring>

static QList<ActiveNotifyV1 *> s_notifies;

struct TreelandActiveNotifyManagerInterfaceV1Private
    : public QtWaylandServer::treeland_active_notify_manager_v1
{
public:
    explicit TreelandActiveNotifyManagerInterfaceV1Private(TreelandActiveNotifyManagerInterfaceV1 *_q);

    wl_global *global() const;

    TreelandActiveNotifyManagerInterfaceV1 *q;

protected:
    void destroy(Resource *resource) override;
    void get_active_notify(Resource *resource,
                           uint32_t id,
                           struct ::wl_resource *seat) override;
};

TreelandActiveNotifyManagerInterfaceV1Private::TreelandActiveNotifyManagerInterfaceV1Private(
    TreelandActiveNotifyManagerInterfaceV1 *_q)
    : q(_q)
{
}

wl_global *TreelandActiveNotifyManagerInterfaceV1Private::global() const
{
    return m_global;
}

void TreelandActiveNotifyManagerInterfaceV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void TreelandActiveNotifyManagerInterfaceV1Private::get_active_notify(Resource *resource,
                                                                       uint32_t id,
                                                                       struct ::wl_resource *seat)
{
    if (!seat || strcmp(wl_resource_get_class(seat), "wl_seat") != 0) {
        wl_resource_post_error(resource->handle, 0, "seat resource is NULL or not a wl_seat!");
        return;
    }

    struct wlr_seat_client *seat_client = wlr_seat_client_from_resource(seat);
    if (!seat_client) {
        wl_resource_post_error(resource->handle, 0, "seat resource is inert!");
        return;
    }
    WSeat *wseat = WSeat::fromHandle(seat_client->seat);
    if (!wseat) {
        wl_resource_post_error(resource->handle, 0, "seat resource has no WSeat!");
        return;
    }

    wl_resource *notifyResource = wl_resource_create(resource->client(),
                                                     &treeland_active_notify_v1_interface,
                                                     resource->version(),
                                                     id);
    if (!notifyResource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto notify = new ActiveNotifyV1(notifyResource, wseat);
    s_notifies.append(notify);

    QObject::connect(notify, &QObject::destroyed, [notify]() {
        s_notifies.removeOne(notify);
    });

    Q_EMIT q->activeNotifyCreated(notify);
}

TreelandActiveNotifyManagerInterfaceV1::TreelandActiveNotifyManagerInterfaceV1(QObject *parent)
    : QObject(parent)
    , d(new TreelandActiveNotifyManagerInterfaceV1Private(this))
{
}

TreelandActiveNotifyManagerInterfaceV1::~TreelandActiveNotifyManagerInterfaceV1() = default;

void TreelandActiveNotifyManagerInterfaceV1::create(WServer *server)
{
    d->init(server->handle(), InterfaceVersion);
}

void TreelandActiveNotifyManagerInterfaceV1::destroy([[maybe_unused]] WServer *server)
{
    d->globalRemove();
}

wl_global *TreelandActiveNotifyManagerInterfaceV1::global() const
{
    return d->global();
}

QByteArrayView TreelandActiveNotifyManagerInterfaceV1::interfaceName() const
{
    return d->interfaceName();
}

class ActiveNotifyV1Private : public QtWaylandServer::treeland_active_notify_v1
{
public:
    ActiveNotifyV1Private(ActiveNotifyV1 *_q, wl_resource *_resource, WSeat *_seat);

    ActiveNotifyV1 *q;
    QPointer<WSeat> wseat;

protected:
    void destroy_resource([[maybe_unused]] Resource *resource) override;
    void destroy([[maybe_unused]] Resource *resource) override;
};

ActiveNotifyV1Private::ActiveNotifyV1Private(ActiveNotifyV1 *_q,
                                             wl_resource *_resource,
                                             WSeat *_seat)
    : QtWaylandServer::treeland_active_notify_v1(_resource)
    , q(_q)
    , wseat(_seat)
{
}

void ActiveNotifyV1Private::destroy_resource([[maybe_unused]] Resource *resource)
{
    delete q;
}

void ActiveNotifyV1Private::destroy([[maybe_unused]] Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

ActiveNotifyV1::ActiveNotifyV1(wl_resource *resource, WSeat *seat, QObject *parent)
    : QObject(parent)
    , d(new ActiveNotifyV1Private(this, resource, seat))
{
}

ActiveNotifyV1::~ActiveNotifyV1() = default;

WSeat *ActiveNotifyV1::wSeat() const
{
    return d->wseat;
}

void ActiveNotifyV1::sendActivityChanged(Reason reason, ActivityState state)
{
    d->send_activity_changed(reason, state);
}

void ActiveNotifyV1::sendDragChanged(DragState state)
{
    d->send_drag_changed(state);
}

void ActiveNotifyV1::sendActivityChanged(Reason reason, ActivityState state, const WSeat *seat)
{
    for (auto notify : std::as_const(s_notifies)) {
        if (notify->wSeat() == seat) {
            notify->sendActivityChanged(reason, state);
        }
    }
}

void ActiveNotifyV1::sendDragChanged(DragState state, const WSeat *seat)
{
    for (auto notify : std::as_const(s_notifies)) {
        if (notify->wSeat() == seat) {
            notify->sendDragChanged(state);
        }
    }
}
