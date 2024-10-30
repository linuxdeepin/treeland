// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "font_impl.h"

#include "personalization_manager_impl.h"
#include "util.h"

static const struct treeland_personalization_font_context_v1_interface
    personalization_font_context_impl = {
        .set_font_size = dispatch_member_function<&personalization_font_context_v1::setFontSize>(),
        .get_font_size = dispatch_member_function<&personalization_font_context_v1::sendFontSize>(),
        .set_font = dispatch_member_function<&personalization_font_context_v1::setFont>(),
        .get_font = dispatch_member_function<&personalization_font_context_v1::sendFont>(),
        .set_monospace_font =
            dispatch_member_function<&personalization_font_context_v1::setMonospaceFont>(),
        .get_monospace_font =
            dispatch_member_function<&personalization_font_context_v1::sendMonospaceFont>(),
        .destroy =
            []([[maybe_unused]] struct wl_client *client, struct wl_resource *resource) {
                wl_resource_destroy(resource);
            }
    };

personalization_font_context_v1::personalization_font_context_v1(
    struct wl_client *client,
    struct wl_resource *manager_resource,
    uint32_t id)
{
    auto *manager = treeland_personalization_manager_v1::from_resource(manager_resource);
    Q_ASSERT(manager);

    m_manager = manager;

    uint32_t version = wl_resource_get_version(manager_resource);
    struct wl_resource *resource =
        wl_resource_create(client,
                           &treeland_personalization_font_context_v1_interface,
                           version,
                           id);
    if (resource == NULL) {
        wl_resource_post_no_memory(manager_resource);
    }

    m_resource = resource;

    wl_resource_set_implementation(resource,
                                   &personalization_font_context_impl,
                                   this,
                                   [](struct wl_resource *resource) {
                                       auto *p =
                                           personalization_font_context_v1::fromResource(resource);
                                       delete p;
                                   });

    wl_list_insert(&manager->resources, wl_resource_get_link(resource));

    Q_EMIT manager->fontContextCreated(this);
}

personalization_font_context_v1::~personalization_font_context_v1()
{
    wl_list_remove(wl_resource_get_link(m_resource));
}

personalization_font_context_v1 *personalization_font_context_v1::fromResource(
    struct wl_resource *resource)
{
    assert(wl_resource_instance_of(resource,
                                   &treeland_personalization_font_context_v1_interface,
                                   &personalization_font_context_impl));
    return static_cast<struct personalization_font_context_v1 *>(
        wl_resource_get_user_data(resource));
}

void personalization_font_context_v1::setFont(const char *font_name)
{
    if (m_fontName != font_name) {
        m_fontName = font_name;
        Q_EMIT fontChanged();
    }
}

void personalization_font_context_v1::setMonospaceFont(const char *font_name)
{
    if (m_monoFontName != font_name) {
        m_monoFontName = font_name;
        Q_EMIT monoFontChanged();
    }
}

void personalization_font_context_v1::setFontSize(uint32_t size)
{
    if (m_fontSize != size) {
        m_fontSize = size;
        Q_EMIT fontSizeChanged();
    }
}

void personalization_font_context_v1::sendFont() const
{
    treeland_personalization_font_context_v1_send_font(m_resource, m_fontName.toUtf8());
}

void personalization_font_context_v1::sendMonospaceFont() const
{
    treeland_personalization_font_context_v1_send_monospace_font(m_resource,
                                                                 m_monoFontName.toUtf8());
}

void personalization_font_context_v1::sendFontSize() const
{
    treeland_personalization_font_context_v1_send_font_size(m_resource, m_fontSize);
}
