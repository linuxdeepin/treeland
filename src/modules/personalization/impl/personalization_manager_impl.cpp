// Copyright (C) 2023 WenHao Peng <pengwenhao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "personalization_manager_impl.h"

#include "impl/appearance_impl.h"
#include "impl/font_impl.h"
#include "treeland-personalization-manager-protocol.h"

#include <cassert>

extern "C" {
#define static
#include <wlr/types/wlr_compositor.h>
#undef static
}

#define TREELAND_PERSONALIZATION_MANAGEMENT_V1_VERSION 1

using QW_NAMESPACE::qw_display;

template<typename T, typename... Args>
void createHandle(Args... args)
{
    new T(std::forward<decltype(args)>(args)...);
}

namespace Personalization {
namespace WindowContext {

static void set_background_type([[maybe_unused]] struct wl_client *client,
                                struct wl_resource *resource,
                                int32_t type);

static void set_round_corner_radius(struct wl_client *client,
                                    struct wl_resource *resource,
                                    int32_t radius);

static void set_shadow(struct wl_client *client,
                       struct wl_resource *resource,
                       int32_t radius,
                       int32_t offset_x,
                       int32_t offset_y,
                       int32_t r,
                       int32_t g,
                       int32_t b,
                       int32_t a);

static void set_border(struct wl_client *client,
                       struct wl_resource *resource,
                       int32_t width,
                       int32_t r,
                       int32_t g,
                       int32_t b,
                       int32_t a);

static void set_titlebar(struct wl_client *client, struct wl_resource *resource, int32_t mode);

static void on_destroy([[maybe_unused]] struct wl_client *client, struct wl_resource *resource)
{
    wl_resource_destroy(resource);
}
} // namespace WindowContext

} // namespace Personalization

static const struct treeland_personalization_window_context_v1_interface
    personalization_window_context_impl = {
        .set_blend_mode = Personalization::WindowContext::set_background_type,
        .set_round_corner_radius = Personalization::WindowContext::set_round_corner_radius,
        .set_shadow = Personalization::WindowContext::set_shadow,
        .set_border = Personalization::WindowContext::set_border,
        .set_titlebar = Personalization::WindowContext::set_titlebar,
        .destroy = Personalization::WindowContext::on_destroy,
    };

personalization_window_context_v1 *personalization_window_context_v1::from_resource(
    wl_resource *resource)
{
    assert(wl_resource_instance_of(resource,
                                   &treeland_personalization_window_context_v1_interface,
                                   &personalization_window_context_impl));
    return static_cast<struct personalization_window_context_v1 *>(
        wl_resource_get_user_data(resource));
}

personalization_window_context_v1::~personalization_window_context_v1()
{
    Q_EMIT before_destroy();
}

static void personalization_window_context_resource_destroy(struct wl_resource *resource)
{
    if (resource)
        return;

    wl_list_remove(wl_resource_get_link(resource));
    auto window_context = personalization_window_context_v1::from_resource(resource);
    if (!window_context) {
        return;
    }

    delete window_context;
}

namespace Personalization {
namespace WindowContext {

static void set_background_type([[maybe_unused]] struct wl_client *client,
                                struct wl_resource *resource,
                                int32_t type)

{
    if (auto *window = personalization_window_context_v1::from_resource(resource)) {
        window->background_type = type;
        Q_EMIT window->backgroundTypeChanged();
    }
}

static void set_round_corner_radius(struct wl_client *client,
                                    struct wl_resource *resource,
                                    int32_t radius)
{
    if (auto *window = personalization_window_context_v1::from_resource(resource)) {
        window->corner_radius = radius;
        Q_EMIT window->cornerRadiusChanged();
    }
}

static void set_shadow(struct wl_client *client,
                       struct wl_resource *resource,
                       int32_t radius,
                       int32_t offset_x,
                       int32_t offset_y,
                       int32_t r,
                       int32_t g,
                       int32_t b,
                       int32_t a)
{
    if (auto *window = personalization_window_context_v1::from_resource(resource)) {
        window->shadow = Shadow{ radius, QPoint{ offset_x, offset_y }, QColor{ r, g, b, a } };
        Q_EMIT window->shadowChanged();
    }
}

static void set_border(struct wl_client *client,
                       struct wl_resource *resource,
                       int32_t width,
                       int32_t r,
                       int32_t g,
                       int32_t b,
                       int32_t a)
{
    if (auto *window = personalization_window_context_v1::from_resource(resource)) {
        window->border = Border{ width, QColor{ r, g, b, a } };
        Q_EMIT window->borderChanged();
    }
}

static void set_titlebar(struct wl_client *client, struct wl_resource *resource, int32_t mode)
{
    if (auto *window = personalization_window_context_v1::from_resource(resource)) {
        window->states.setFlag(
            personalization_window_context_v1::WindowState::NoTitleBar,
            mode == TREELAND_PERSONALIZATION_WINDOW_CONTEXT_V1_ENABLE_MODE_DISABLE);
        Q_EMIT window->windowStateChanged();
    }
}

} // namespace WindowContext

} // namespace Personalization

void get_metadata(struct wl_client *client, struct wl_resource *manager_resource);

void set_fd(struct wl_client *client,
            struct wl_resource *resource,
            int32_t fd,
            const char *metadata);

void set_identifier(struct wl_client *client, struct wl_resource *resource, const char *identifier);

void set_output(struct wl_client *client, struct wl_resource *resource, const char *output);

void set_on(struct wl_client *client, struct wl_resource *resource, uint32_t options);

void set_isdark(struct wl_client *client, struct wl_resource *resource, uint32_t dark);

void wallpaper_commit(struct wl_client *client, struct wl_resource *resource);

static void personalization_wallpaper_context_destroy([[maybe_unused]] struct wl_client *client,
                                                      struct wl_resource *resource)
{
    wl_resource_destroy(resource);
}

static const struct treeland_personalization_wallpaper_context_v1_interface
    personalization_wallpaper_context_impl = {
        .set_fd = set_fd,
        .set_identifier = set_identifier,
        .set_output = set_output,
        .set_on = set_on,
        .set_isdark = set_isdark,
        .commit = wallpaper_commit,
        .get_metadata = get_metadata,
        .destroy = personalization_wallpaper_context_destroy,
    };

personalization_wallpaper_context_v1 *personalization_wallpaper_context_v1::from_resource(
    wl_resource *resource)
{
    assert(wl_resource_instance_of(resource,
                                   &treeland_personalization_wallpaper_context_v1_interface,
                                   &personalization_wallpaper_context_impl));
    return static_cast<struct personalization_wallpaper_context_v1 *>(
        wl_resource_get_user_data(resource));
}

personalization_wallpaper_context_v1::~personalization_wallpaper_context_v1()
{
    Q_EMIT before_destroy();
}

static void personalization_wallpaper_context_resource_destroy(struct wl_resource *resource)
{
    if (resource)
        return;

    wl_list_remove(wl_resource_get_link(resource));
    auto wallpaper_context = personalization_wallpaper_context_v1::from_resource(resource);
    if (!wallpaper_context) {
        return;
    }

    delete wallpaper_context;
}

void set_cursor_theme(struct wl_client *client, struct wl_resource *resource, const char *name);

void get_cursor_theme(struct wl_client *client, struct wl_resource *resource);

void set_cursor_size(struct wl_client *client, struct wl_resource *resource, uint32_t size);

void get_cursor_size(struct wl_client *client, struct wl_resource *resource);

void cursor_commit(struct wl_client *client, struct wl_resource *resource);

static void personalization_cursor_context_destroy([[maybe_unused]] struct wl_client *client,
                                                   struct wl_resource *resource)
{
    wl_resource_destroy(resource);
}

static const struct treeland_personalization_cursor_context_v1_interface
    personalization_cursor_context_impl = {
        .set_theme = set_cursor_theme,
        .get_theme = get_cursor_theme,
        .set_size = set_cursor_size,
        .get_size = get_cursor_size,
        .commit = cursor_commit,
        .destroy = personalization_cursor_context_destroy,
    };

personalization_cursor_context_v1 *personalization_cursor_context_v1::from_resource(
    wl_resource *resource)
{
    assert(wl_resource_instance_of(resource,
                                   &treeland_personalization_cursor_context_v1_interface,
                                   &personalization_cursor_context_impl));
    return static_cast<struct personalization_cursor_context_v1 *>(
        wl_resource_get_user_data(resource));
}

personalization_cursor_context_v1::~personalization_cursor_context_v1()
{
    Q_EMIT before_destroy();
}

static void personalization_cursor_context_resource_destroy(struct wl_resource *resource)
{
    if (resource)
        return;

    wl_list_remove(wl_resource_get_link(resource));
    auto cursor_context = personalization_cursor_context_v1::from_resource(resource);
    if (!cursor_context) {
        return;
    }

    delete cursor_context;
}

void create_personalization_window_context_listener(struct wl_client *client,
                                                    struct wl_resource *manager_resource,
                                                    uint32_t id,
                                                    struct wl_resource *surface);

void create_personalization_wallpaper_context_listener(struct wl_client *client,
                                                       struct wl_resource *manager_resource,
                                                       uint32_t id);

void create_personalization_cursor_context_listener(struct wl_client *client,
                                                    struct wl_resource *manager_resource,
                                                    uint32_t id);

static const struct treeland_personalization_manager_v1_interface
    treeland_personalization_manager_impl = {
        .get_window_context = create_personalization_window_context_listener,
        .get_wallpaper_context = create_personalization_wallpaper_context_listener,
        .get_cursor_context = create_personalization_cursor_context_listener,
        .get_font_context = createHandle<personalization_font_context_v1>,
        .get_appearance_context = createHandle<personalization_appearance_context_v1>,
    };

treeland_personalization_manager_v1 *treeland_personalization_manager_v1::from_resource(
    struct wl_resource *resource)
{
    assert(wl_resource_instance_of(resource,
                                   &treeland_personalization_manager_v1_interface,
                                   &treeland_personalization_manager_impl));
    struct treeland_personalization_manager_v1 *manager =
        static_cast<treeland_personalization_manager_v1 *>(wl_resource_get_user_data(resource));
    assert(manager != NULL);
    return manager;
}

static void treeland_personalization_manager_resource_destroy(struct wl_resource *resource)
{
    if (resource)
        return;

    wl_list_remove(wl_resource_get_link(resource));
    auto manager = treeland_personalization_manager_v1::from_resource(resource);
    if (!manager) {
        return;
    }

    delete manager;
}

void create_personalization_window_context_listener(struct wl_client *client,
                                                    struct wl_resource *manager_resource,
                                                    uint32_t id,
                                                    struct wl_resource *surface)
{
    auto *manager = treeland_personalization_manager_v1::from_resource(manager_resource);
    if (!manager)
        return;

    auto *context = new personalization_window_context_v1;

    if (!context) {
        wl_resource_post_no_memory(manager_resource);
        return;
    }

    context->manager = manager;

    uint32_t version = wl_resource_get_version(manager_resource);
    struct wl_resource *resource =
        wl_resource_create(client,
                           &treeland_personalization_window_context_v1_interface,
                           version,
                           id);
    if (!resource) {
        delete context;
        wl_resource_post_no_memory(manager_resource);
        return;
    }

    context->surface = wlr_surface_from_resource(surface);

    wl_resource_set_implementation(resource,
                                   &personalization_window_context_impl,
                                   context,
                                   personalization_window_context_resource_destroy);

    wl_list_insert(&manager->resources, wl_resource_get_link(resource));
    Q_EMIT manager->windowContextCreated(context);
}

void create_personalization_wallpaper_context_listener(struct wl_client *client,
                                                       struct wl_resource *manager_resource,
                                                       uint32_t id)
{

    auto *manager = treeland_personalization_manager_v1::from_resource(manager_resource);
    if (!manager)
        return;

    auto *context = new personalization_wallpaper_context_v1;

    if (!context) {
        wl_resource_post_no_memory(manager_resource);
        return;
    }

    context->manager = manager;

    uint32_t version = wl_resource_get_version(manager_resource);
    struct wl_resource *resource =
        wl_resource_create(client,
                           &treeland_personalization_wallpaper_context_v1_interface,
                           version,
                           id);
    if (resource == NULL) {
        delete context;
        wl_resource_post_no_memory(manager_resource);
        return;
    }

    context->resource = resource;
    wl_client_get_credentials(client, nullptr, &context->uid, nullptr);

    wl_resource_set_implementation(resource,
                                   &personalization_wallpaper_context_impl,
                                   context,
                                   personalization_wallpaper_context_resource_destroy);

    wl_list_insert(&manager->resources, wl_resource_get_link(resource));
    Q_EMIT manager->wallpaperContextCreated(context);
}

void create_personalization_cursor_context_listener(struct wl_client *client,
                                                    struct wl_resource *manager_resource,
                                                    uint32_t id)
{

    struct treeland_personalization_manager_v1 *manager =
        treeland_personalization_manager_v1::from_resource(manager_resource);
    if (!manager)
        return;

    auto *context = new personalization_cursor_context_v1;

    if (!context) {
        wl_resource_post_no_memory(manager_resource);
        return;
    }

    context->manager = manager;

    uint32_t version = wl_resource_get_version(manager_resource);
    struct wl_resource *resource =
        wl_resource_create(client,
                           &treeland_personalization_cursor_context_v1_interface,
                           version,
                           id);
    if (resource == NULL) {
        delete context;
        wl_resource_post_no_memory(manager_resource);
        return;
    }

    context->resource = resource;
    wl_resource_set_implementation(resource,
                                   &personalization_cursor_context_impl,
                                   context,
                                   personalization_cursor_context_resource_destroy);

    wl_list_insert(&manager->resources, wl_resource_get_link(resource));
    Q_EMIT manager->cursorContextCreated(context);
}

void create_personalization_appearance_context_listener(struct wl_client *client,
                                                        struct wl_resource *manager_resource,
                                                        uint32_t id)
{
    struct treeland_personalization_manager_v1 *manager =
        treeland_personalization_manager_v1::from_resource(manager_resource);
    if (!manager)
        return;

    auto *context = new personalization_cursor_context_v1;

    if (!context) {
        wl_resource_post_no_memory(manager_resource);
        return;
    }

    context->manager = manager;

    uint32_t version = wl_resource_get_version(manager_resource);
    struct wl_resource *resource =
        wl_resource_create(client,
                           &treeland_personalization_cursor_context_v1_interface,
                           version,
                           id);
    if (resource == NULL) {
        delete context;
        wl_resource_post_no_memory(manager_resource);
        return;
    }

    context->resource = resource;
    wl_resource_set_implementation(resource,
                                   &personalization_cursor_context_impl,
                                   context,
                                   personalization_cursor_context_resource_destroy);

    wl_list_insert(&manager->resources, wl_resource_get_link(resource));
}

void set_fd(struct wl_client *client [[maybe_unused]],
            struct wl_resource *resource,
            int32_t fd,
            const char *metadata)
{
    struct personalization_wallpaper_context_v1 *wallpaper =
        personalization_wallpaper_context_v1::from_resource(resource);
    if (wallpaper == NULL || fd == -1)
        return;

    wallpaper->fd = fd;
    wallpaper->meta_data = metadata;
}

void set_identifier(struct wl_client *client [[maybe_unused]],
                    struct wl_resource *resource,
                    const char *identifier)
{
    auto *wallpaper = personalization_wallpaper_context_v1::from_resource(resource);
    if (!wallpaper)
        return;

    wallpaper->identifier = identifier;
}

void set_output(struct wl_client *client, struct wl_resource *resource, const char *output)
{
    auto *wallpaper = personalization_wallpaper_context_v1::from_resource(resource);
    if (!wallpaper)
        return;

    wallpaper->output_name = QString::fromUtf8(output);
}

void set_on(struct wl_client *client [[maybe_unused]],
            struct wl_resource *resource,
            uint32_t options)
{
    auto *wallpaper = personalization_wallpaper_context_v1::from_resource(resource);
    if (!wallpaper)
        return;

    wallpaper->options = options;
}

void wallpaper_commit(struct wl_client *client [[maybe_unused]], struct wl_resource *resource)
{
    auto *wallpaper = personalization_wallpaper_context_v1::from_resource(resource);
    if (!wallpaper || wallpaper->fd == -1)
        return;

    Q_EMIT wallpaper->commit(wallpaper);
}

void get_metadata(struct wl_client *client [[maybe_unused]], struct wl_resource *resource)
{
    auto *wallpaper = personalization_wallpaper_context_v1::from_resource(resource);
    if (!wallpaper)
        return;

    Q_EMIT wallpaper->getWallpapers(wallpaper);
}

void set_isdark(struct wl_client *client [[maybe_unused]],
                struct wl_resource *resource,
                uint32_t isdark)
{
    auto *wallpaper = personalization_wallpaper_context_v1::from_resource(resource);
    if (!wallpaper)
        return;

    wallpaper->isdark = isdark;
}

void set_cursor_theme(struct wl_client *client, struct wl_resource *resource, const char *name)
{
    auto *cursor = personalization_cursor_context_v1::from_resource(resource);
    if (!cursor)
        return;

    cursor->theme = QString::fromUtf8(name);
}

void get_cursor_theme(struct wl_client *client, struct wl_resource *resource)
{
    auto *cursor = personalization_cursor_context_v1::from_resource(resource);
    if (!cursor)
        return;

    Q_EMIT cursor->get_theme(cursor);
}

void set_cursor_size(struct wl_client *client, struct wl_resource *resource, uint32_t size)
{
    auto *cursor = personalization_cursor_context_v1::from_resource(resource);
    if (!cursor)
        return;

    cursor->size = size;
}

void get_cursor_size(struct wl_client *client, struct wl_resource *resource)
{
    auto *cursor = personalization_cursor_context_v1::from_resource(resource);
    if (!cursor)
        return;

    Q_EMIT cursor->get_size(cursor);
}

void cursor_commit(struct wl_client *client, struct wl_resource *resource)
{
    auto *cursor = personalization_cursor_context_v1::from_resource(resource);
    if (!cursor)
        return;

    Q_EMIT cursor->commit(cursor);
}

static void treeland_personalization_manager_bind(struct wl_client *client,
                                                  void *data,
                                                  uint32_t version,
                                                  uint32_t id)
{
    auto *manager = static_cast<struct treeland_personalization_manager_v1 *>(data);
    struct wl_resource *resource =
        wl_resource_create(client, &treeland_personalization_manager_v1_interface, version, id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource,
                                   &treeland_personalization_manager_impl,
                                   manager,
                                   treeland_personalization_manager_resource_destroy);

    wl_list_insert(&manager->resources, wl_resource_get_link(resource));
}

treeland_personalization_manager_v1::treeland_personalization_manager_v1()
{
    wl_list_init(&resources);
}

treeland_personalization_manager_v1::~treeland_personalization_manager_v1()
{
    Q_EMIT before_destroy();
    if (global)
        wl_global_destroy(global);

    wl_list_remove(&resources);
}

treeland_personalization_manager_v1 *treeland_personalization_manager_v1::create(
    qw_display *display)
{
    auto *manager = new treeland_personalization_manager_v1;

    manager->event_loop = wl_display_get_event_loop(display->handle());
    manager->global = wl_global_create(display->handle(),
                                       &treeland_personalization_manager_v1_interface,
                                       TREELAND_PERSONALIZATION_MANAGEMENT_V1_VERSION,
                                       manager,
                                       treeland_personalization_manager_bind);
    if (!manager->global) {
        delete manager;
        return nullptr;
    }

    connect(display, &qw_display::before_destroy, manager, [manager]() {
        delete manager;
    });

    return manager;
}

void personalization_wallpaper_context_v1::set_meta_data(const QString &data)
{
    meta_data = data;

    treeland_personalization_wallpaper_context_v1_send_metadata(resource, meta_data.toLocal8Bit());
}

void personalization_cursor_context_v1::set_theme(const QString &theme)
{
    if (this->theme == theme)
        return;
    this->theme = theme;
    treeland_personalization_cursor_context_v1_send_theme(resource, theme.toLocal8Bit());
}

void personalization_cursor_context_v1::set_size(uint32_t size)
{
    if (this->size == size)
        return;
    this->size = size;
    treeland_personalization_cursor_context_v1_send_size(resource, size);
}

void personalization_cursor_context_v1::verfity(bool verfityed)
{
    treeland_personalization_cursor_context_v1_send_verfity(resource, verfityed);
}
