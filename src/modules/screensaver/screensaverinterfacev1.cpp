// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "screensaverinterfacev1.h"
#include "treeland-screensaver-v1-protocol.h"
#include "helper.h"

#include <wayland-server.h>
#include <wayland-util.h>
#include <QDebug>

// request implementation

static void inhibit([[maybe_unused]] struct wl_client *client, struct wl_resource *resource, const char *appName, const char *reason) {
    auto screensaver = static_cast<ScreensaverInterfaceV1 *>(wl_resource_get_user_data(resource));
    screensaver->inhibit(resource, appName, reason);
}

static void uninhibit([[maybe_unused]] struct wl_client *client, struct wl_resource *resource) {
    auto screensaver = static_cast<ScreensaverInterfaceV1 *>(wl_resource_get_user_data(resource));
    screensaver->uninhibit(resource);
}

static const struct treeland_screensaver_interface treeland_screensaver_impl {
    .inhibit = inhibit,
    .uninhibit = uninhibit,
};

// wayland object binding

static void handleResourceDestroy(struct wl_resource *resource) {
    auto screensaver = static_cast<ScreensaverInterfaceV1 *>(wl_resource_get_user_data(resource));
    screensaver->uninhibit(resource);
}

static void handleBindingGlobal(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
    auto screensaver = static_cast<ScreensaverInterfaceV1 *>(data);
    auto *resource = wl_resource_create(client, &treeland_screensaver_interface, version, id);
    wl_resource_set_implementation(resource, &treeland_screensaver_impl, screensaver, handleResourceDestroy);
}

// ScreensaverInterfaceV1

QByteArrayView ScreensaverInterfaceV1::interfaceName() const {
    static const QByteArray arr(treeland_screensaver_interface.name);
    return QByteArrayView(arr);
}

void ScreensaverInterfaceV1::inhibit(wl_resource *res, const char *appName, const char *reason) {
    if (m_inhibits.contains(res))
        return wl_resource_post_error(res, TREELAND_SCREENSAVER_ERROR_ALREADY_INHIBITED,
                                      "Trying to inhibit with an existing inhibit active");
    m_inhibits.insert(res, std::make_tuple(QByteArray(appName), QByteArray(reason)));
    Helper::instance()->updateIdleInhibitor();
}

void ScreensaverInterfaceV1::uninhibit(wl_resource *res) {
    if (!m_inhibits.contains(res))
        return wl_resource_post_error(res, TREELAND_SCREENSAVER_ERROR_NOT_YET_INHIBITED,
                                      "Trying to uninhibit but no active inhibit existed");
    m_inhibits.remove(res);
    Helper::instance()->updateIdleInhibitor();
}

void ScreensaverInterfaceV1::create(WServer *server) {
    m_global = wl_global_create(server->handle()->handle(), &treeland_screensaver_interface,
                                treeland_screensaver_interface.version, this, handleBindingGlobal);
}

void ScreensaverInterfaceV1::destroy([[maybe_unused]] WServer *server) {
    wl_global_destroy(m_global);
}
