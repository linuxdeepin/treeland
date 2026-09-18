// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "activenotifymanagerinterfacev1.h"

#include "qwayland-server-treeland-active-notify-unstable-v1.h"

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
    if (!seat) {
        wl_resource_post_error(resource->handle, 0, "seat resource is NULL!");
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

    auto notify = new ActiveNotifyV1(notifyResource, seat);
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
    ActiveNotifyV1Private(ActiveNotifyV1 *_q, wl_resource *_resource, wl_resource *_seat);

    ActiveNotifyV1 *q;
    wl_resource *seatResource{ nullptr };
    wl_listener seatResourceDestroyListener;
    bool seatResourceListenerRemoved{ false };

    static void handleSeatResourceDestroy(wl_listener *listener, void *data);

protected:
    void destroy_resource([[maybe_unused]] Resource *resource) override;
    void destroy([[maybe_unused]] Resource *resource) override;
};

ActiveNotifyV1Private::ActiveNotifyV1Private(ActiveNotifyV1 *_q,
                                             wl_resource *_resource,
                                             wl_resource *_seat)
    : QtWaylandServer::treeland_active_notify_v1(_resource)
    , q(_q)
    , seatResource(_seat)
{
    // Track the seat resource lifetime: a client may release its wl_seat
    // (or the seat may go inert) while the notifier is still alive, and
    // broadcasting against a stale seat resource would be a use-after-free.
    seatResourceDestroyListener.notify = handleSeatResourceDestroy;
    wl_resource_add_destroy_listener(_seat, &seatResourceDestroyListener);
}

void ActiveNotifyV1Private::handleSeatResourceDestroy(wl_listener *listener, [[maybe_unused]] void *data)
{
    ActiveNotifyV1Private *self = wl_container_of(listener, self, seatResourceDestroyListener);
    wl_list_remove(&self->seatResourceDestroyListener.link);
    self->seatResourceListenerRemoved = true;
    self->seatResource = nullptr;
}

void ActiveNotifyV1Private::destroy_resource([[maybe_unused]] Resource *resource)
{
    if (!seatResourceListenerRemoved)
        wl_list_remove(&seatResourceDestroyListener.link);
    delete q;
}

void ActiveNotifyV1Private::destroy([[maybe_unused]] Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

ActiveNotifyV1::ActiveNotifyV1(wl_resource *resource, wl_resource *seat, QObject *parent)
    : QObject(parent)
    , d(new ActiveNotifyV1Private(this, resource, seat))
{
}

ActiveNotifyV1::~ActiveNotifyV1() = default;

wl_resource *ActiveNotifyV1::seat() const
{
    return d->seatResource;
}

WSeat *ActiveNotifyV1::wSeat() const
{
    if (!d->seatResource)
        return nullptr;

    struct wlr_seat_client *seat_client =
        wlr_seat_client_from_resource(d->seatResource);
    if (!seat_client)
        return nullptr;
    return WSeat::fromHandle(seat_client->seat);
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
