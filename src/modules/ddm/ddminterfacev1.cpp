// Copyright (C) 2025 April Lu <apr3vau@outlook.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "ddminterfacev1.h"
#include "treeland-ddm-v1-protocol.h"
#include "common/treelandlogging.h"
#include "helper.h"
#include "usermodel.h"

#include <wayland-server-core.h>
#include <wayland-server.h>
#include <wayland-util.h>
#include <QDebug>

struct treeland_ddm {
    wl_resource *resource;
};

// request implementation

static void switchToGreeter([[maybe_unused]] struct wl_client *client, [[maybe_unused]] struct wl_resource *resource) {
    Helper::instance()->showLockScreen(false);
}

static void switchToUser([[maybe_unused]] struct wl_client *client, [[maybe_unused]] struct wl_resource *resource, const char *username) {
    auto user = QString::fromLocal8Bit(username);
    auto helper = Helper::instance();
    if (user == "ddm") {
        helper->showLockScreen(false);
    } else if (user != helper->userModel()->currentUserName()) {
        helper->userModel()->setCurrentUserName(QString(username));
        helper->showLockScreen(false);
    }
}

static void activateSession([[maybe_unused]] struct wl_client *client, [[maybe_unused]] struct wl_resource *resource) {
    Helper::instance()->activateSession();
}

static void deactivateSession([[maybe_unused]] struct wl_client *client, [[maybe_unused]] struct wl_resource *resource) {
    Helper::instance()->deactivateSession();
}

static void enableRender([[maybe_unused]] struct wl_client *client, [[maybe_unused]] struct wl_resource *resource) {
    Helper::instance()->enableRender();
}

static void disableRender(struct wl_client *client, [[maybe_unused]] struct wl_resource *resource, uint32_t id) {
    Helper::instance()->disableRender();
    auto callback = wl_resource_create(client, &wl_callback_interface, 1, id);
    auto serial = wl_display_get_serial(wl_client_get_display(client));
    wl_callback_send_done(callback, serial);
    wl_resource_destroy(callback);
}

static const struct treeland_ddm_interface treeland_ddm_impl {
    .switch_to_greeter = switchToGreeter,
    .switch_to_user = switchToUser,
    .activate_session = activateSession,
    .deactivate_session = deactivateSession,
    .enable_render = enableRender,
    .disable_render = disableRender,
};

// wayland object binding

static void handleResourceDestroy(struct wl_resource *resource) {
    qCWarning(treelandCore) << "DDM connection lost";
    auto ddm = static_cast<struct treeland_ddm *>(wl_resource_get_user_data(resource));
    ddm->resource = nullptr;
}

void handleBindingGlobal(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
    auto ddm = static_cast<struct treeland_ddm *>(data);
    auto *resource = wl_resource_create(client, &treeland_ddm_interface, version, id);
    wl_resource_set_implementation(resource, &treeland_ddm_impl, ddm, handleResourceDestroy);
    ddm->resource = resource;
    qCDebug(treelandCore) << "DDM connection established";

    treeland_ddm_send_acquire_vt(resource, 0);
}

// DDMInterfaceV1

DDMInterfaceV1::DDMInterfaceV1() {

}

DDMInterfaceV1::~DDMInterfaceV1() {
}

QByteArrayView DDMInterfaceV1::interfaceName() const {
    QByteArray arr(treeland_ddm_interface.name);
    return QByteArrayView(arr);
}

bool DDMInterfaceV1::isConnected() const {
    auto ddm = static_cast<struct treeland_ddm *>(m_handle);
    return ddm && ddm->resource;
}

void DDMInterfaceV1::create(WServer *server) {
    auto ddm = new treeland_ddm { .resource = nullptr };
    m_handle = ddm;
    m_global = wl_global_create(server->handle()->handle(), &treeland_ddm_interface,
                                treeland_ddm_interface.version, ddm, handleBindingGlobal);
}

void DDMInterfaceV1::destroy([[maybe_unused]] WServer *server) {
    wl_global_destroy(m_global);
    auto ddm = static_cast<struct treeland_ddm *>(m_handle);
    delete ddm;
    m_handle = nullptr;
}

wl_global *DDMInterfaceV1::global() const {
    return m_global;
}

// Event wrapper

void DDMInterfaceV1::switchToVt(const int vtnr) {
    auto ddm = static_cast<struct treeland_ddm *>(m_handle);
    if (isConnected())
        treeland_ddm_send_switch_to_vt(ddm->resource, vtnr);
    else
        qCWarning(treelandCore) << "DDM is not conected when trying to call switchToVt";
}

void DDMInterfaceV1::acquireVt(const int vtnr) {
    auto ddm = static_cast<struct treeland_ddm *>(m_handle);
    if (isConnected())
        treeland_ddm_send_acquire_vt(ddm->resource, vtnr);
    else
        qCWarning(treelandCore) << "DDM is not connected when trying to call acquireVt";
}
