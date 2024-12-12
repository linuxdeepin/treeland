// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "font_impl.h"

#include "modules/personalization/impl/personalization_manager_impl.h"

static const struct treeland_personalization_font_context_v1_interface
    personalization_font_context_impl = {
        .set_font_size =
            []([[maybe_unused]] struct wl_client *client,
               struct wl_resource *resource,
               uint32_t size) {
                Q_EMIT personalization_font_context_v1::fromResource(resource)->fontSizeChanged(
                    size);
            },
        .get_font_size =
            []([[maybe_unused]] struct wl_client *client, struct wl_resource *resource) {
                Q_EMIT personalization_font_context_v1::fromResource(resource)->requestFontSize();
            },
        .set_font =
            []([[maybe_unused]] struct wl_client *client,
               struct wl_resource *resource,
               const char *font) {
                Q_EMIT personalization_font_context_v1::fromResource(resource)->fontChanged(font);
            },
        .get_font =
            []([[maybe_unused]] struct wl_client *client, struct wl_resource *resource) {
                Q_EMIT personalization_font_context_v1::fromResource(resource)->requestFont();
            },
        .set_monospace_font =
            []([[maybe_unused]] struct wl_client *client,
               struct wl_resource *resource,
               const char *font) {
                Q_EMIT personalization_font_context_v1::fromResource(resource)->monoFontChanged(
                    font);
            },
        .get_monospace_font =
            []([[maybe_unused]] struct wl_client *client, struct wl_resource *resource) {
                Q_EMIT personalization_font_context_v1::fromResource(resource)->requestMonoFont();
            },
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
                                       Q_EMIT p->beforeDestroy();
                                       delete p;
                                       wl_list_remove(wl_resource_get_link(resource));
                                   });

    wl_list_insert(&manager->resources, wl_resource_get_link(resource));

    Q_EMIT manager->fontContextCreated(this);
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

void personalization_font_context_v1::sendFont(const QString &font) const
{
    treeland_personalization_font_context_v1_send_font(m_resource, font.toUtf8());
}

void personalization_font_context_v1::sendMonospaceFont(const QString &font) const
{
    treeland_personalization_font_context_v1_send_monospace_font(m_resource, font.toUtf8());
}

void personalization_font_context_v1::sendFontSize(uint32_t size) const
{
    treeland_personalization_font_context_v1_send_font_size(m_resource, size);
}
