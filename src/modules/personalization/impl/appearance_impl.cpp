// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "appearance_impl.h"

#include "personalization_manager_impl.h"
#include "treeland-personalization-manager-protocol.h"

#include <wayland-server-core.h>

template<typename T>
struct member_function;

template<typename R, typename C, typename... Args>
struct member_function<R (C::*)(Args...)>
{
    using return_type = R;
    using class_type = C;
    using arg_typelist = std::tuple<Args...>;
};

template<typename R, typename C, typename... Args>
struct member_function<R (C::*)(Args...) const>
{
    using return_type = R;
    using class_type = C;
    using arg_typelist = std::tuple<Args...>;
};

template<std::size_t N, auto mFunc, typename typeList, typename class_type, typename... Args>
constexpr auto make_lambda()
{
    if constexpr (N == 0) {
        auto tmp = [](struct wl_client *client, struct wl_resource *resource, Args... args) {
            auto obj = reinterpret_cast<class_type *>(
                personalization_appearance_context_v1::fromResource(resource));
            (obj->*mFunc)(args...);
        };
        return tmp;
    } else {
        return make_lambda<N - 1,
                           mFunc,
                           typeList,
                           class_type,
                           typename std::tuple_element<N - 1, typeList>::type,
                           Args...>();
    }
}

template<auto func>
constexpr auto dispatch_member_function()
{
    using typeList = typename member_function<decltype(func)>::arg_typelist;
    using class_type = typename member_function<decltype(func)>::class_type;
    const auto typeListLen = std::tuple_size_v<typeList>;
    return make_lambda<typeListLen, func, typeList, class_type>();
}

static const struct treeland_personalization_appearance_context_v1_interface
    personalization_appearance_context_impl = {
        .set_round_corner_radius = dispatch_member_function<
            &personalization_appearance_context_v1::setRoundCornerRadius>(),
        .set_font = dispatch_member_function<&personalization_appearance_context_v1::setFont>(),
        .set_monospace_font =
            dispatch_member_function<&personalization_appearance_context_v1::setMonospaceFont>(),
        .set_cursor_theme =
            dispatch_member_function<&personalization_appearance_context_v1::setCursorTheme>(),
        .set_icon_theme =
            dispatch_member_function<&personalization_appearance_context_v1::setIconTheme>(),
        .get_round_corner_radius = dispatch_member_function<
            &personalization_appearance_context_v1::sendRoundCornerRadius>(),
        .get_font = dispatch_member_function<&personalization_appearance_context_v1::sendFont>(),
        .get_monospace_font =
            dispatch_member_function<&personalization_appearance_context_v1::sendMonospaceFont>(),
        .get_cursor_theme =
            dispatch_member_function<&personalization_appearance_context_v1::sendCursorTheme>(),
        .get_icon_theme =
            dispatch_member_function<&personalization_appearance_context_v1::sendIconTheme>(),
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

    wl_resource_set_implementation(resource,
                                   &personalization_appearance_context_impl,
                                   this,
                                   [](struct wl_resource *resource) {
                                       wl_list_remove(wl_resource_get_link(resource));
                                   });

    wl_list_insert(&manager->resources, wl_resource_get_link(resource));

    Q_EMIT manager->appearanceContextCreated(this);
}

void personalization_appearance_context_v1::setRoundCornerRadius(int32_t radius)
{
    if (m_radius == radius) {
        return;
    }

    m_radius = radius;

    sendState([radius](struct wl_resource *resource) {
        treeland_personalization_appearance_context_v1_send_round_corner_radius(resource, radius);
    });

    Q_EMIT roundCornerRadiusChanged();
}

void personalization_appearance_context_v1::setFont(const char *font_name)
{
    if (m_fontName == font_name) {
        return;
    }

    m_fontName = font_name;

    sendState([font_name](struct wl_resource *resource) {
        treeland_personalization_appearance_context_v1_send_font(resource, font_name);
    });

    Q_EMIT fontChanged();
}

void personalization_appearance_context_v1::setMonospaceFont(const char *font_name)
{
    if (m_monoFontName == font_name) {
        return;
    }

    m_monoFontName = font_name;

    sendState([font_name](struct wl_resource *resource) {
        treeland_personalization_appearance_context_v1_send_monospace_font(resource, font_name);
    });

    Q_EMIT monoFontChanged();
}

void personalization_appearance_context_v1::setCursorTheme(const char *theme_name)
{
    if (m_cursorTheme == theme_name) {
        return;
    }

    m_cursorTheme = theme_name;

    sendState([theme_name](struct wl_resource *resource) {
        treeland_personalization_appearance_context_v1_send_cursor_theme(resource, theme_name);
    });

    Q_EMIT cursorThemeChanged();
}

void personalization_appearance_context_v1::setIconTheme(const char *theme_name)
{
    if (m_iconTheme == theme_name) {
        return;
    }

    m_iconTheme = theme_name;

    sendState([theme_name](struct wl_resource *resource) {
        treeland_personalization_appearance_context_v1_send_icon_theme(resource, theme_name);
    });

    Q_EMIT iconThemeChanged();
}

void personalization_appearance_context_v1::sendRoundCornerRadius() const
{
    treeland_personalization_appearance_context_v1_send_round_corner_radius(m_resource, m_radius);
}

void personalization_appearance_context_v1::sendFont() const
{
    treeland_personalization_appearance_context_v1_send_font(m_resource, m_fontName.toUtf8());
}

void personalization_appearance_context_v1::sendMonospaceFont() const
{
    treeland_personalization_appearance_context_v1_send_monospace_font(m_resource,
                                                                       m_monoFontName.toUtf8());
}

void personalization_appearance_context_v1::sendCursorTheme() const
{
    treeland_personalization_appearance_context_v1_send_cursor_theme(m_resource,
                                                                     m_cursorTheme.toUtf8());
}

void personalization_appearance_context_v1::sendIconTheme() const
{
    treeland_personalization_appearance_context_v1_send_icon_theme(m_resource,
                                                                   m_iconTheme.toUtf8());
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
