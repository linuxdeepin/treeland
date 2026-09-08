// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "showdesktopinterfacev1.h"
#include "qwayland-server-treeland-show-desktop-unstable-v1.h"

#include <wserver.h>

#include <QDebug>
#include <QQmlInfo>

#include <wayland-server-core.h>

class ShowDesktopInterfaceV1Private : public QtWaylandServer::treeland_show_desktop_v1
{
public:
    ShowDesktopInterfaceV1Private(ShowDesktopInterfaceV1 *_q);
    wl_global *global() const;

    ShowDesktopInterfaceV1 *q;

    uint32_t state = 0; // state 0: normal, 1: show;
protected:
    void bind_resource(Resource *resource) override;
    void destroy(Resource *resource) override;
    void set_show_desktop_state(Resource *resource, uint32_t state) override;
};

ShowDesktopInterfaceV1Private::ShowDesktopInterfaceV1Private(ShowDesktopInterfaceV1 *_q)
    : QtWaylandServer::treeland_show_desktop_v1()
    , q(_q)
{
}

wl_global *ShowDesktopInterfaceV1Private::global() const
{
    return m_global;
}

void ShowDesktopInterfaceV1Private::bind_resource(Resource *resource)
{
    send_show_desktop_state(resource->handle, state);
}

void ShowDesktopInterfaceV1Private::destroy(Resource *resource) {
    wl_resource_destroy(resource->handle);
}

void ShowDesktopInterfaceV1Private::set_show_desktop_state([[maybe_unused]] Resource *resource, uint32_t state)
{
    q->setDesktopState(static_cast<ShowDesktopInterfaceV1::State>(state));
}

ShowDesktopInterfaceV1::ShowDesktopInterfaceV1(QObject *parent)
    : QObject(parent)
    , d(new ShowDesktopInterfaceV1Private(this))
{
    qRegisterMetaType<State>("State");
}

ShowDesktopInterfaceV1::~ShowDesktopInterfaceV1() = default;

ShowDesktopInterfaceV1::State ShowDesktopInterfaceV1::desktopState()
{
    // TODO: When the protocol is not initialized,
    // qml calls the current interface m_handle is empty
    return d->global() ? static_cast<State>(d->state) : State::Normal;
}

void ShowDesktopInterfaceV1::setDesktopState(State state)
{
    d->state = static_cast<uint32_t>(state);

    for (const auto &resource : d->resourceMap()) {
        d->send_show_desktop_state(resource->handle, d->state);
    }

    Q_EMIT desktopStateChanged();

    qmlInfo(this) << QString("Try to show desktop state (%1)!").arg(d->state);
}

void ShowDesktopInterfaceV1::create(WServer *server)
{
    d->init(server->handle(), InterfaceVersion);
}

void ShowDesktopInterfaceV1::destroy([[maybe_unused]] WServer *server) {
    d->globalRemove();
}

wl_global *ShowDesktopInterfaceV1::global() const
{
    return d->global();
}

QByteArrayView ShowDesktopInterfaceV1::interfaceName() const
{
    return d->interfaceName();
}
