// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "ddminterfacev2.h"

#include "common/treelandlogging.h"
#include "helper.h"
#include "treeland-ddm-v2-protocol.h"
#include "usermodel.h"

#include <wayland-server-core.h>
#include <wayland-server.h>
#include <wayland-util.h>

#include <QDebug>

/////////////////////////////
// Request Implementations //
/////////////////////////////

// Sync

static void capabilities([[maybe_unused]] struct wl_client *client,
                         struct wl_resource *resource,
                         uint32_t capabilities)
{
    auto interface = static_cast<DDMInterfaceV2 *>(wl_resource_get_user_data(resource));
    Q_EMIT interface->capabilities(capabilities);
}

static void userLoggedIn([[maybe_unused]] struct wl_client *client,
                         struct wl_resource *resource,
                         const char *username,
                         const char *session)
{
    auto interface = static_cast<DDMInterfaceV2 *>(wl_resource_get_user_data(resource));
    Q_EMIT interface->userLoggedIn(QString::fromLocal8Bit(username), QString::fromLocal8Bit(session));
}

// Authentication

static void authenticationFailed([[maybe_unused]] struct wl_client *client,
                                 struct wl_resource *resource,
                                 uint32_t error)
{
    auto interface = static_cast<DDMInterfaceV2 *>(wl_resource_get_user_data(resource));
    Q_EMIT interface->authenticationFailed(error);
}

// Greeter

static void switchToGreeter([[maybe_unused]] struct wl_client *client,
                            [[maybe_unused]] struct wl_resource *resource)
{
    Helper::instance()->showLockScreen(false);
}

static void switchToUser([[maybe_unused]] struct wl_client *client,
                         [[maybe_unused]] struct wl_resource *resource,
                         const char *username)
{
    auto user = QString::fromLocal8Bit(username);
    auto helper = Helper::instance();
    if (user == "dde") {
        helper->showLockScreen(false);
    } else if (user != helper->userModel()->currentUserName()) {
        helper->userModel()->setCurrentUserName(user);
        helper->showLockScreen(false);
    }
}

// DRM Control

static void activateSession([[maybe_unused]] struct wl_client *client,
                            [[maybe_unused]] struct wl_resource *resource)
{
    Helper::instance()->activateSession();
}

static void deactivateSession([[maybe_unused]] struct wl_client *client,
                              [[maybe_unused]] struct wl_resource *resource)
{
    Helper::instance()->deactivateSession();
}

static void enableRender([[maybe_unused]] struct wl_client *client,
                         [[maybe_unused]] struct wl_resource *resource)
{
    Helper::instance()->enableRender();
}

static void disableRender(struct wl_client *client,
                          [[maybe_unused]] struct wl_resource *resource,
                          uint32_t id)
{
    Helper::instance()->disableRender();
    auto callback = wl_resource_create(client, &wl_callback_interface, 1, id);
    auto serial = wl_display_get_serial(wl_client_get_display(client));
    wl_callback_send_done(callback, serial);
    wl_resource_destroy(callback);
}

////////////////////////////
// Wayland Object Binding //
////////////////////////////

static const struct treeland_ddm_v2_interface treeland_ddm_v2_impl{
    .capabilities = capabilities,
    .user_logged_in = userLoggedIn,
    .authentication_failed = authenticationFailed,
    .switch_to_greeter = switchToGreeter,
    .switch_to_user = switchToUser,
    .activate_session = activateSession,
    .deactivate_session = deactivateSession,
    .enable_render = enableRender,
    .disable_render = disableRender,
};

static void handleResourceDestroy(struct wl_resource *resource)
{
    qCWarning(treelandCore) << "DDM connection lost";
    auto interface = static_cast<DDMInterfaceV2 *>(wl_resource_get_user_data(resource));
    interface->setHandle(nullptr);
    Q_EMIT interface->disconnected();
}

static void handleBindingGlobal(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
    auto interface = static_cast<DDMInterfaceV2 *>(data);
    auto *resource = wl_resource_create(client, &treeland_ddm_v2_interface, version, id);
    wl_resource_set_user_data(resource, interface);
    wl_resource_set_implementation(resource, &treeland_ddm_v2_impl, interface, handleResourceDestroy);
    interface->setHandle(resource);
    qCDebug(treelandCore) << "DDM connection established";
    Q_EMIT interface->connected();
}

/////////////
// Methods //
/////////////

QByteArrayView DDMInterfaceV2::interfaceName() const
{
    return treeland_ddm_v2_interface.name;
}

void DDMInterfaceV2::setHandle(struct wl_resource *handle)
{
    m_handle = handle;
}

QString DDMInterfaceV2::authErrorToString(uint32_t error)
{
    switch (error) {
    case TREELAND_DDM_V2_AUTH_ERROR_AUTHENTICATION_FAILED:
		return "Authentication failed";
	case TREELAND_DDM_V2_AUTH_ERROR_INVALID_USER:
		return "Invalid user";
	case TREELAND_DDM_V2_AUTH_ERROR_INVALID_SESSION:
		return "Invalid session";
	case TREELAND_DDM_V2_AUTH_ERROR_EXISTING_AUTHENTICATION_ONGOING:
		return "Existing authentication ongoing";
	case TREELAND_DDM_V2_AUTH_ERROR_INTERNAL_ERROR:
		return "Internal error";
    default:
        return QString("Unknown error: %1").arg(error);
    }
}

void DDMInterfaceV2::create(WServer *server)
{
    m_global = wl_global_create(server->handle()->handle(),
                                &treeland_ddm_v2_interface,
                                treeland_ddm_v2_interface.version,
                                this,
                                handleBindingGlobal);
}

void DDMInterfaceV2::destroy([[maybe_unused]] WServer *server)
{
    if (m_handle) {
        wl_resource_destroy(static_cast<struct wl_resource *>(m_handle));
        m_handle = nullptr;
    }
    if (m_global) {
        wl_global_destroy(m_global);
        m_global = nullptr;
    }
}

wl_global *DDMInterfaceV2::global() const
{
    return m_global;
}

////////////////////
// Event Wrappers //
////////////////////

// Session Management

void DDMInterfaceV2::login(const QString &username,
                           const QString &password,
                           DDM::Session::Type sessionType,
                           const QString &sessionFile) const
{
    if (isValid()) {
        treeland_ddm_v2_send_login(static_cast<struct wl_resource *>(m_handle),
                                   qPrintable(username),
                                   qPrintable(password),
                                   sessionType,
                                   qPrintable(sessionFile));
        wl_display_flush_clients(wl_global_get_display(m_global));
    } else {
        qCWarning(treelandCore) << "DDM is not connected when trying to call login";
    }
}

void DDMInterfaceV2::logout(const QString &session) const
{
    if (isValid()) {
        treeland_ddm_v2_send_logout(static_cast<struct wl_resource *>(m_handle),
                                    qPrintable(session));
        wl_display_flush_clients(wl_global_get_display(m_global));
    } else {
        qCWarning(treelandCore) << "DDM is not connected when trying to call logout";
    }
}

void DDMInterfaceV2::lock(const QString &session) const
{
    if (isValid()) {
        treeland_ddm_v2_send_lock(static_cast<struct wl_resource *>(m_handle), qPrintable(session));
        wl_display_flush_clients(wl_global_get_display(m_global));
    } else {
        qCWarning(treelandCore) << "DDM is not connected when trying to call lock";
    }
}

void DDMInterfaceV2::unlock(const QString &session, const QString &password) const
{
    if (isValid()) {
        treeland_ddm_v2_send_unlock(static_cast<struct wl_resource *>(m_handle),
                                    qPrintable(session),
                                    qPrintable(password));
        wl_display_flush_clients(wl_global_get_display(m_global));
    } else {
        qCWarning(treelandCore) << "DDM is not connected when trying to call unlock";
    }
}

// Power Management

void DDMInterfaceV2::powerOff() const
{
    if (isValid()) {
        treeland_ddm_v2_send_poweroff(static_cast<struct wl_resource *>(m_handle));
        wl_display_flush_clients(wl_global_get_display(m_global));
    } else {
        qCWarning(treelandCore) << "DDM is not connected when trying to call powerOff";
    }
}

void DDMInterfaceV2::reboot() const
{
    if (isValid()) {
        treeland_ddm_v2_send_reboot(static_cast<struct wl_resource *>(m_handle));
        wl_display_flush_clients(wl_global_get_display(m_global));
    } else {
        qCWarning(treelandCore) << "DDM is not connected when trying to call reboot";
    }
}

void DDMInterfaceV2::suspend() const
{
    if (isValid()) {
        treeland_ddm_v2_send_suspend(static_cast<struct wl_resource *>(m_handle));
        wl_display_flush_clients(wl_global_get_display(m_global));
    } else {
        qCWarning(treelandCore) << "DDM is not connected when trying to call suspend";
    }
}

void DDMInterfaceV2::hibernate() const
{
    if (isValid()) {
        treeland_ddm_v2_send_hibernate(static_cast<struct wl_resource *>(m_handle));
        wl_display_flush_clients(wl_global_get_display(m_global));
    } else {
        qCWarning(treelandCore) << "DDM is not connected when trying to call hibernate";
    }
}

void DDMInterfaceV2::hybridSleep() const
{
    if (isValid()) {
        treeland_ddm_v2_send_hybrid_sleep(static_cast<struct wl_resource *>(m_handle));
        wl_display_flush_clients(wl_global_get_display(m_global));
    } else {
        qCWarning(treelandCore) << "DDM is not connected when trying to call hybridSleep";
    }
}

// DRM Control

void DDMInterfaceV2::switchToVt(int vtnr) const
{
    if (isValid()) {
        treeland_ddm_v2_send_switch_to_vt(static_cast<struct wl_resource *>(m_handle), vtnr);
        wl_display_flush_clients(wl_global_get_display(m_global));
    } else {
        qCWarning(treelandCore) << "DDM is not connected when trying to call switchToVt";
    }
}
