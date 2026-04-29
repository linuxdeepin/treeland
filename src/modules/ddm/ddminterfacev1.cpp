// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "ddminterfacev1.h"
#include "common/treelandlogging.h"
#include "helper.h"
#include "usermodel.h"
#include "qwayland-server-treeland-ddm-v1.h"

#include <wayland-server-core.h>
#include <wayland-server.h>
#include <wayland-util.h>

#include <qwdisplay.h>

#include <QDebug>

class DDMInterfaceV1Private : public QtWaylandServer::treeland_ddm_v1
{
public:
    explicit DDMInterfaceV1Private(DDMInterfaceV1 *_q);
    wl_global *global() const;

    DDMInterfaceV1 *q = nullptr;
protected:
    void destroy(Resource *resource) override;
    void bind_resource(Resource *resource) override;
    void switch_to_greeter(Resource *resource) override;
    void switch_to_user(Resource *resource, const QString &username) override;
    void activate_session(Resource *resource) override;
    void deactivate_session(Resource *resource) override;
    void enable_render(Resource *resource) override;
    void disable_render(Resource *resource, uint32_t callback) override;
};

DDMInterfaceV1Private::DDMInterfaceV1Private(DDMInterfaceV1 *_q)
    : QtWaylandServer::treeland_ddm_v1()
    , q(_q)
{
}

wl_global *DDMInterfaceV1Private::global() const
{
    return m_global;
}

void DDMInterfaceV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void DDMInterfaceV1Private::bind_resource(Resource *resource)
{
    send_acquire_vt(resource->handle, 0);
}

void DDMInterfaceV1Private::switch_to_greeter([[maybe_unused]] Resource *resource)
{
    Helper::instance()->showLockScreen(false);
}

void DDMInterfaceV1Private::switch_to_user([[maybe_unused]] Resource *resource, const QString &username)
{
    auto helper = Helper::instance();
    if (username == "ddm") {
        helper->showLockScreen(false);
    } else if (username != helper->userModel()->currentUserName()) {
        helper->userModel()->setCurrentUserName(username);
        helper->showLockScreen(false);
    }
}

void DDMInterfaceV1Private::activate_session([[maybe_unused]] Resource *resource)
{
    Helper::instance()->activateSession();
}

void DDMInterfaceV1Private::deactivate_session([[maybe_unused]] Resource *resource)
{
    Helper::instance()->deactivateSession();
}

void DDMInterfaceV1Private::enable_render([[maybe_unused]] Resource *resource)
{
    Helper::instance()->enableRender();
}

void DDMInterfaceV1Private::disable_render(Resource *resource, uint32_t id)
{
    Helper::instance()->disableRender();
    auto callback = wl_resource_create(resource->client(), &wl_callback_interface, 1, id);
    auto serial = wl_display_get_serial(wl_client_get_display(resource->client()));
    wl_callback_send_done(callback, serial);
    wl_resource_destroy(callback);
}

DDMInterfaceV1::DDMInterfaceV1(QObject *parent)
    : QObject(parent)
    , WServerInterface()
    , d(new DDMInterfaceV1Private(this))
{
}

DDMInterfaceV1::~DDMInterfaceV1() = default;

QByteArrayView DDMInterfaceV1::interfaceName() const
{
    return d->interfaceName();
}

bool DDMInterfaceV1::isConnected() const
{
    return !d->resourceMap().isEmpty();
}

void DDMInterfaceV1::switchToVt(const int vtnr)
{
    if (isConnected()) {
        for (const auto &resource : d->resourceMap()) {
            d->send_switch_to_vt(resource->handle, vtnr);
        }
    } else {
        qCWarning(treelandCore) << "DDM is not connected when trying to call switchToVt";
    }
}

void DDMInterfaceV1::acquireVt(const int vtnr)
{
    if (isConnected()) {
        for (const auto &resource : d->resourceMap()) {
            d->send_acquire_vt(resource->handle, vtnr);
        }
    } else {
        qCWarning(treelandCore) << "DDM is not connected when trying to call acquireVt";
    }
}

void DDMInterfaceV1::create(WServer *server)
{
    d->init(server->handle()->handle(), InterfaceVersion);
}

void DDMInterfaceV1::destroy([[maybe_unused]] WServer *server)
{
    d->globalRemove();
}

wl_global *DDMInterfaceV1::global() const
{
    return d->global();
}
