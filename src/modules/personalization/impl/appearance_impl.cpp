// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "appearance_impl.h"

#include "modules/personalization/impl/personalization_manager_impl.h"
#include "treeland-personalization-manager-protocol.h"
#include "modules/personalization/impl/util.h"

#include <wayland-server-core.h>
#include <wayland-server.h>

static const struct treeland_personalization_appearance_context_v1_interface
    personalization_appearance_context_impl = {
        .set_round_corner_radius = dispatch_member_function<
            &personalization_appearance_context_v1::setRoundCornerRadius>(),
        .get_round_corner_radius =
            [](struct wl_client *client, struct wl_resource *resource) {
                Q_EMIT personalization_appearance_context_v1::fromResource(resource)
                    ->requestRoundCornerRadius();
            },
        .set_icon_theme =
            dispatch_member_function<&personalization_appearance_context_v1::setIconTheme>(),
        .get_icon_theme =
            [](struct wl_client *client, struct wl_resource *resource) {
                Q_EMIT personalization_appearance_context_v1::fromResource(resource)->requestIconTheme();
            },
        .set_active_color =
            dispatch_member_function<&personalization_appearance_context_v1::setActiveColor>(),
        .get_active_color =
            [](struct wl_client *client, struct wl_resource *resource) {
                Q_EMIT personalization_appearance_context_v1::fromResource(resource)->requestActiveColor();
            },
        .set_window_opacity =
            dispatch_member_function<&personalization_appearance_context_v1::setWindowOpacity>(),
        .get_window_opacity =
            [](struct wl_client *client, struct wl_resource *resource) {
                Q_EMIT personalization_appearance_context_v1::fromResource(resource)
                    ->requestWindowOpacity();
            },
        .set_window_theme_type =
            dispatch_member_function<&personalization_appearance_context_v1::setWindowThemeType>(),
        .get_window_theme_type =
            [](struct wl_client *client, struct wl_resource *resource) {
                Q_EMIT personalization_appearance_context_v1::fromResource(resource)
                    ->requestWindowThemeType();
            },
        .set_window_titlebar_height = dispatch_member_function<
            &personalization_appearance_context_v1::setWindowTitlebarHeight>(),
        .get_window_titlebar_height =
            [](struct wl_client *client, struct wl_resource *resource) {
                Q_EMIT personalization_appearance_context_v1::fromResource(resource)
                    ->requestWindowTitlebarHeight();
            },
        .destroy =
            []([[maybe_unused]] struct wl_client *client, struct wl_resource *resource) {
                wl_resource_destroy(resource);
            }
    };

personalization_appearance_context_v1 *personalization_appearance_context_v1::fromResource(
    struct wl_resource *resource)
{
    assert(wl_resource_instance_of(resource,
                                   &treeland_personalization_appearance_context_v1_interface,
                                   &personalization_appearance_context_impl));

    return static_cast<struct personalization_appearance_context_v1 *>(
        wl_resource_get_user_data(resource));
}

personalization_appearance_context_v1::personalization_appearance_context_v1(
    struct wl_client *client,
    struct wl_resource *manager_resource,
    uint32_t id)
    : QObject()
{
    auto *manager = treeland_personalization_manager_v1::from_resource(manager_resource);
    Q_ASSERT(manager);

    m_manager = manager;

    uint32_t version = wl_resource_get_version(manager_resource);
    struct wl_resource *resource =
        wl_resource_create(client,
                           &treeland_personalization_appearance_context_v1_interface,
                           version,
                           id);
    if (resource == NULL) {
        wl_resource_post_no_memory(manager_resource);
    }

    m_resource = resource;

    wl_resource_set_implementation(
        resource,
        &personalization_appearance_context_impl,
        this,
        [](struct wl_resource *resource) {
            auto *p = personalization_appearance_context_v1::fromResource(resource);
            Q_EMIT p->beforeDestroy();
            delete p;
            wl_list_remove(wl_resource_get_link(resource));
        });

    wl_list_insert(&manager->resources, wl_resource_get_link(resource));

    Q_EMIT manager->appearanceContextCreated(this);
}

personalization_appearance_context_v1::~personalization_appearance_context_v1()
{
    // wl_list_remove(wl_resource_get_link(m_resource));
}

void personalization_appearance_context_v1::setRoundCornerRadius(int32_t radius)
{
    Q_EMIT roundCornerRadiusChanged(radius);
}

void personalization_appearance_context_v1::setIconTheme(const char *theme_name)
{
    Q_EMIT iconThemeChanged(theme_name);
}

void personalization_appearance_context_v1::sendRoundCornerRadius(int32_t radius)
{
    treeland_personalization_appearance_context_v1_send_round_corner_radius(m_resource, radius);
}

void personalization_appearance_context_v1::sendIconTheme(const char *icon_theme)
{
    treeland_personalization_appearance_context_v1_send_icon_theme(m_resource, icon_theme);
}

void personalization_appearance_context_v1::setActiveColor(const char *color)
{
    Q_EMIT activeColorChanged(color);
}

void personalization_appearance_context_v1::sendActiveColor(const char *color)
{
    treeland_personalization_appearance_context_v1_send_active_color(m_resource, color);
}

void personalization_appearance_context_v1::setWindowOpacity(uint32_t opacity)
{
    Q_EMIT windowOpacityChanged(opacity);
}

void personalization_appearance_context_v1::sendWindowOpacity(uint32_t opacity)
{
    treeland_personalization_appearance_context_v1_send_window_opacity(m_resource, opacity);
}

void personalization_appearance_context_v1::setWindowThemeType(uint32_t type)
{
    Q_EMIT windowThemeTypeChanged(type);
}

void personalization_appearance_context_v1::sendWindowThemeType(uint32_t type)
{
    treeland_personalization_appearance_context_v1_send_window_theme_type(m_resource, type);
}

void personalization_appearance_context_v1::setWindowTitlebarHeight(uint32_t height)
{
    Q_EMIT titlebarHeightChanged(height);
}

void personalization_appearance_context_v1::sendWindowTitlebarHeight(uint32_t height)
{
    treeland_personalization_appearance_context_v1_send_window_titlebar_height(m_resource, height);
}
