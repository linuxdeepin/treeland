// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "appearance_impl.h"

#include "personalization_manager_impl.h"
#include "treeland-personalization-manager-protocol.h"
#include "util.h"

#include <wayland-server-core.h>

static const struct treeland_personalization_appearance_context_v1_interface
    personalization_appearance_context_impl = {
        .set_round_corner_radius = dispatch_member_function<
            &personalization_appearance_context_v1::setRoundCornerRadius>(),
        .get_round_corner_radius = dispatch_member_function<
            &personalization_appearance_context_v1::sendRoundCornerRadius>(),
        .set_icon_theme =
            dispatch_member_function<&personalization_appearance_context_v1::setIconTheme>(),
        .get_icon_theme =
            dispatch_member_function<&personalization_appearance_context_v1::sendIconTheme>(),
        .set_active_color =
            dispatch_member_function<&personalization_appearance_context_v1::setActiveColor>(),
        .get_active_color =
            dispatch_member_function<&personalization_appearance_context_v1::sendActiveColor>(),
        .set_window_opacity =
            dispatch_member_function<&personalization_appearance_context_v1::setWindowOpacity>(),
        .get_window_opacity =
            dispatch_member_function<&personalization_appearance_context_v1::sendWindowOpacity>(),
        .set_window_theme_type =
            dispatch_member_function<&personalization_appearance_context_v1::setWindowThemeType>(),
        .get_window_theme_type =
            dispatch_member_function<&personalization_appearance_context_v1::sendWindowThemeType>(),
        .set_window_titlebar_height = dispatch_member_function<
            &personalization_appearance_context_v1::setWindowTitlebarHeight>(),
        .get_window_titlebar_height = dispatch_member_function<
            &personalization_appearance_context_v1::sendWindowTitlebarHeight>(),
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
    if (m_radius == radius) {
        return;
    }

    m_radius = radius;

    Q_EMIT roundCornerRadiusChanged();
}

void personalization_appearance_context_v1::setIconTheme(const char *theme_name)
{
    if (m_iconTheme == theme_name) {
        return;
    }

    m_iconTheme = theme_name;

    Q_EMIT iconThemeChanged();
}

void personalization_appearance_context_v1::sendRoundCornerRadius() const
{
    treeland_personalization_appearance_context_v1_send_round_corner_radius(m_resource, m_radius);
}

void personalization_appearance_context_v1::sendIconTheme() const
{
    treeland_personalization_appearance_context_v1_send_icon_theme(m_resource,
                                                                   m_iconTheme.toUtf8());
}

void personalization_appearance_context_v1::setActiveColor(const char *color)
{
    if (m_activeColor == color) {
        return;
    }

    m_activeColor = color;

    Q_EMIT activeColorChanged();
}

void personalization_appearance_context_v1::sendActiveColor() const
{
    treeland_personalization_appearance_context_v1_send_active_color(m_resource,
                                                                     m_activeColor.toUtf8());
}

void personalization_appearance_context_v1::setWindowOpacity(uint32_t opacity)
{
    if (m_windowOpacity == opacity) {
        return;
    }

    m_windowOpacity = opacity;

    Q_EMIT windowOpacityChanged();
}

void personalization_appearance_context_v1::sendWindowOpacity() const
{
    treeland_personalization_appearance_context_v1_send_window_opacity(m_resource, m_windowOpacity);
}

void personalization_appearance_context_v1::setWindowThemeType(uint32_t type)
{
    if (m_windowThemeType == type) {
        return;
    }

    m_windowThemeType = static_cast<ThemeType>(type);

    Q_EMIT windowThemeTypeChanged();
}

void personalization_appearance_context_v1::sendWindowThemeType() const
{
    treeland_personalization_appearance_context_v1_send_window_theme_type(m_resource,
                                                                          m_windowThemeType);
}

void personalization_appearance_context_v1::setWindowTitlebarHeight(uint32_t height)
{
    if (m_titlebarHeight == height) {
        return;
    }

    m_titlebarHeight = height;

    Q_EMIT titlebarHeightChanged();
}

void personalization_appearance_context_v1::sendWindowTitlebarHeight() const
{
    treeland_personalization_appearance_context_v1_send_window_titlebar_height(m_resource,
                                                                               m_titlebarHeight);
}

void personalization_appearance_context_v1::sendState(
    std::function<void(struct wl_resource *)> func)
{
    struct wl_resource *resource;
    wl_resource_for_each(resource, &m_manager->resources)
    {
        if (wl_resource_instance_of(resource,
                                    &treeland_personalization_appearance_context_v1_interface,
                                    &personalization_appearance_context_impl)) {
            func(resource);
        }
    }
}
