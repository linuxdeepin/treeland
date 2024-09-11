// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "ddeshellv1.h"

#include <qwcompositor.h>
#include <qwdisplay.h>
#include <qwoutput.h>

#define TREELAND_DDE_SHELL_MANAGER_V1_VERSION 1

template<typename T, typename... Args>
void createHandle(Args... args)
{
    new T(std::forward<decltype(args)>(args)...);
}

namespace Protocols {

struct treeland_dde_shell_manager_v1 *manager_from_resource(struct wl_resource *resource);

template<typename T>
concept window_overlap_checker_concept = std::same_as<T, treeland_window_overlap_checker>;

template<window_overlap_checker_concept T>
T *handle_from_resource(struct wl_resource *resource);

void bind(struct wl_client *client, void *data, uint32_t version, uint32_t id);
void window_overlap_check(struct wl_client *client,
                          struct wl_resource *resource,
                          struct wl_resource *surface,
                          uint32_t id);

void update(struct wl_client *client,
            struct wl_resource *resource,
            int32_t width,
            int32_t height,
            uint32_t anchor,
            struct wl_resource *output);
void destroy(struct wl_client *client, struct wl_resource *resource);

} // namespace Protocols

static const struct treeland_dde_shell_manager_v1_interface treeland_dde_shell_manager_impl = {
    .get_window_overlap_checker = createHandle<treeland_window_overlap_checker>,
};

static const struct treeland_window_overlap_checker_interface
    treeland_window_overlap_checker_impl = {
        .update = Protocols::update,
        .destroy = Protocols::destroy,
    };

namespace Protocols {

struct treeland_dde_shell_manager_v1 *manager_from_resource(struct wl_resource *resource)
{
    assert(wl_resource_instance_of(resource,
                                   &treeland_dde_shell_manager_v1_interface,
                                   &treeland_dde_shell_manager_impl));
    struct treeland_dde_shell_manager_v1 *manager =
        static_cast<treeland_dde_shell_manager_v1 *>(wl_resource_get_user_data(resource));
    assert(manager != NULL);
    return manager;
}

template<window_overlap_checker_concept T>
T *handle_from_resource(struct wl_resource *resource)
{
    assert(wl_resource_instance_of(resource,
                                   &treeland_window_overlap_checker_interface,
                                   &treeland_window_overlap_checker_impl));
    return static_cast<T *>(wl_resource_get_user_data(resource));
}

void bind(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
    auto *shell = static_cast<treeland_dde_shell_manager_v1 *>(data);
    struct wl_resource *resource =
        wl_resource_create(client, &treeland_dde_shell_manager_v1_interface, version, id);

    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }

    wl_resource_set_implementation(resource,
                                   &treeland_dde_shell_manager_impl,
                                   shell,
                                   [](struct wl_resource *resource) {
                                       wl_list_remove(wl_resource_get_link(resource));
                                   });

    wl_list_insert(&shell->resources, wl_resource_get_link(resource));
}

void update(struct wl_client *client,
            struct wl_resource *resource,
            int32_t width,
            int32_t height,
            uint32_t anchor,
            struct wl_resource *output)
{
    auto *handle = handle_from_resource<treeland_window_overlap_checker>(resource);

    handle->m_output = wlr_output_from_resource(output);
    handle->m_size = QSize(width, height);
    handle->m_anchor = static_cast<treeland_window_overlap_checker::Anchor>(anchor);

    Q_EMIT handle->refresh();
}

void destroy(struct wl_client *client, struct wl_resource *resource) { }
} // namespace Protocols

treeland_window_overlap_checker::treeland_window_overlap_checker(
    struct wl_client *client, struct wl_resource *manager_resource, uint32_t id)
{
    auto *shell = Protocols::manager_from_resource(manager_resource);
    if (!shell) {
        qWarning() << "Failed to get treeland_dde_shell_manager_v1";
        return;
    }

    m_manager = shell;

    uint32_t version = wl_resource_get_version(manager_resource);
    struct wl_resource *resource =
        wl_resource_create(client, &treeland_window_overlap_checker_interface, version, id);
    m_resource = resource;

    wl_resource_set_implementation(m_resource,
                                   &treeland_window_overlap_checker_impl,
                                   this,
                                   [](wl_resource *resource) {
                                       wl_list_remove(wl_resource_get_link(resource));
                                   });

    m_manager->addWindowOverlapChecker(this);
}

treeland_window_overlap_checker::~treeland_window_overlap_checker()
{
    Q_EMIT before_destroy();
}

void treeland_window_overlap_checker::sendOverlapped(bool overlapped)
{
    if (m_alreadySend && overlapped == m_overlapped) {
        m_alreadySend = true;
        return;
    }

    m_overlapped = overlapped;
    m_alreadySend = false;

    if (m_overlapped) {
        treeland_window_overlap_checker_send_enter(m_resource);
    } else {
        treeland_window_overlap_checker_send_leave(m_resource);
    }
}

treeland_dde_shell_manager_v1::treeland_dde_shell_manager_v1(QW_NAMESPACE::qw_display *display,
                                                             QObject *parent)
    : QObject(parent)
{

    eventLoop = wl_display_get_event_loop(display->handle());

    global = wl_global_create(display->handle(),
                              &treeland_dde_shell_manager_v1_interface,
                              TREELAND_DDE_SHELL_MANAGER_V1_VERSION,
                              this,
                              Protocols::bind);

    wl_list_init(&resources);

    connect(display,
            &QW_NAMESPACE::qw_display::before_destroy,
            this,
            &treeland_dde_shell_manager_v1::before_destroy);
    connect(display,
            &QW_NAMESPACE::qw_display::before_destroy,
            this,
            &treeland_dde_shell_manager_v1::deleteLater);
}

treeland_dde_shell_manager_v1::~treeland_dde_shell_manager_v1()
{
    Q_EMIT before_destroy();
}

void treeland_dde_shell_manager_v1::addWindowOverlapChecker(treeland_window_overlap_checker *handle)
{
    connect(handle, &treeland_window_overlap_checker::before_destroy, this, [this, handle] {
        m_checkHandles.removeOne(handle);
    });

    m_checkHandles.append(handle);
    wl_list_insert(&resources, wl_resource_get_link(handle->m_resource));

    Q_EMIT windowOverlapCheckerCreated(handle);
}

treeland_dde_shell_manager_v1 *treeland_dde_shell_manager_v1::create(
    QW_NAMESPACE::qw_display *display)
{
    return new treeland_dde_shell_manager_v1(display);
}
