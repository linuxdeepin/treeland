// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "ddeshellmanagerv1impl.h"

#include <qwcompositor.h>
#include <qwdisplay.h>
#include <qwoutput.h>

extern "C" {
#define static
#include "wlr/types/wlr_compositor.h"

#include <wlr/types/wlr_seat.h>
#undef static
}

#define TREELAND_DDE_SHELL_MANAGER_V1_VERSION 1

Q_LOGGING_CATEGORY(ddeShellImpl, "treeland.protocols.ddeshell", QtDebugMsg);

static void handle_get_window_overlap_checker(struct wl_client *client,
                                              struct wl_resource *manager_resource,
                                              uint32_t id);

static void handle_get_shell_surface(struct wl_client *client,
                                     struct wl_resource *manager_resource,
                                     uint32_t id,
                                     struct wl_resource *surface_resource);

static void handle_get_treeland_dde_active(struct wl_client *client,
                                           struct wl_resource *manager_resource,
                                           uint32_t id,
                                           struct wl_resource *seat);

static void handle_get_treeland_multitaskview(struct wl_client *client,
                                              struct wl_resource *manager_resource,
                                              uint32_t id);

static const struct treeland_dde_shell_manager_v1_interface treeland_dde_shell_manager_v1_impl = {
    .get_window_overlap_checker = handle_get_window_overlap_checker,
    .get_shell_surface = handle_get_shell_surface,
    .get_treeland_dde_active = handle_get_treeland_dde_active,
    .get_treeland_multitaskview = handle_get_treeland_multitaskview
};

static treeland_dde_shell_manager_v1 *manager_from_resource(struct wl_resource *resource)
{
    assert(wl_resource_instance_of(resource,
                                   &treeland_dde_shell_manager_v1_interface,
                                   &treeland_dde_shell_manager_v1_impl));

    treeland_dde_shell_manager_v1 *manager =
        static_cast<treeland_dde_shell_manager_v1 *>(wl_resource_get_user_data(resource));
    assert(manager);
    return manager;
}

static void handle_treeland_window_overlap_checker_update(struct wl_client *client,
                                                          struct wl_resource *resource,
                                                          int32_t width,
                                                          int32_t height,
                                                          uint32_t anchor,
                                                          struct wl_resource *output)
{
    auto *handle =
        static_cast<treeland_window_overlap_checker *>(wl_resource_get_user_data(resource));

    handle->m_output = wlr_output_from_resource(output);
    handle->m_size = QSize(width, height);
    handle->m_anchor = static_cast<treeland_window_overlap_checker::Anchor>(anchor);

    Q_EMIT handle->refresh();
}

static const struct treeland_window_overlap_checker_interface
    treeland_window_overlap_checker_impl = {
        .update = handle_treeland_window_overlap_checker_update,
    };

static void treeland_window_overlap_checker_resource_destroy(struct wl_resource *resource)
{
    assert(wl_resource_instance_of(resource,
                                   &treeland_window_overlap_checker_interface,
                                   &treeland_window_overlap_checker_impl));

    auto *checker =
        static_cast<struct treeland_window_overlap_checker *>(wl_resource_get_user_data(resource));
    if (!checker) {
        return;
    }

    delete checker;
}

static void handle_get_window_overlap_checker(struct wl_client *client,
                                              struct wl_resource *manager_resource,
                                              uint32_t id)
{
    auto *manager = manager_from_resource(manager_resource);
    if (!manager) {
        qCCritical(ddeShellImpl) << "Failed to get treeland_dde_shell_manager_v1";
        return;
    }

    auto *checker = new treeland_window_overlap_checker;
    if (!checker) {
        wl_resource_post_no_memory(manager_resource);
        return;
    }

    uint32_t version = wl_resource_get_version(manager_resource);
    struct wl_resource *resource =
        wl_resource_create(client, &treeland_window_overlap_checker_interface, version, id);
    if (!resource) {
        delete checker;
        wl_resource_post_no_memory(manager_resource);
        return;
    }

    checker->m_manager = manager;
    checker->m_resource = resource;
    wl_resource_set_implementation(resource,
                                   &treeland_window_overlap_checker_impl,
                                   checker,
                                   treeland_window_overlap_checker_resource_destroy);
    wl_resource_set_user_data(resource, checker);
    manager->addWindowOverlapChecker(checker);
}

static void handle_treeland_dde_shell_surface_v1_destroy(struct wl_client *client,
                                                         struct wl_resource *resource)
{
    wl_resource_destroy(resource);
}

static treeland_dde_shell_surface *treeland_dde_shell_surface_frome_resource(
    struct wl_resource *resource)
{
    auto *shell_surface =
        static_cast<treeland_dde_shell_surface *>(wl_resource_get_user_data(resource));
    if (!shell_surface) {
        qCCritical(ddeShellImpl) << "Failed to get treeland_dde_shell_surface";
        return nullptr;
    }

    return shell_surface;
}

static void handle_treeland_dde_shell_surface_v1_set_surface_position(struct wl_client *client,
                                                                      struct wl_resource *resource,
                                                                      int32_t x,
                                                                      int32_t y)
{
    auto *shell_surface = treeland_dde_shell_surface_frome_resource(resource);
    if (!shell_surface) {
        return;
    }

    QPoint globalPos(x, y);
    if (shell_surface->m_surfacePos == globalPos) {
        return;
    }

    shell_surface->m_surfacePos = globalPos;
    Q_EMIT shell_surface->positionChanged(globalPos);
}

static void handle_treeland_dde_shell_surface_v1_set_role(struct wl_client *client,
                                                          struct wl_resource *resource,
                                                          uint32_t layer)
{
    auto *shell_surface = treeland_dde_shell_surface_frome_resource(resource);
    if (!shell_surface) {
        return;
    }

    treeland_dde_shell_surface::Role r;
    switch (layer) {
    case TREELAND_DDE_SHELL_SURFACE_V1_ROLE_OVERLAY:
        r = treeland_dde_shell_surface::OVERLAY;
        break;
    default:
        break;
    }

    if (r == shell_surface->m_role) {
        return;
    }

    shell_surface->m_role = r;
    Q_EMIT shell_surface->roleChanged(r);
}

static void handle_treeland_dde_shell_surface_v1_set_auto_placement(struct wl_client *client,
                                                                    struct wl_resource *resource,
                                                                    uint32_t y_offset)
{
    auto *shell_surface = treeland_dde_shell_surface_frome_resource(resource);
    if (!shell_surface) {
        return;
    }

    if (y_offset == shell_surface->m_yOffset) {
        return;
    }

    shell_surface->m_yOffset = y_offset;
    Q_EMIT shell_surface->yOffsetChanged(y_offset);
}

static void handle_treeland_dde_shell_surface_v1_set_skip_switcher(struct wl_client *client,
                                                                   struct wl_resource *resource,
                                                                   uint32_t skip)
{
    auto *shell_surface = treeland_dde_shell_surface_frome_resource(resource);
    if (!shell_surface) {
        return;
    }

    if (skip == shell_surface->m_skipSwitcher) {
        return;
    }

    shell_surface->m_skipSwitcher = skip;
    Q_EMIT shell_surface->skipSwitcherChanged(skip);
}

static void handle_treeland_dde_shell_surface_v1_set_skip_dock_preview(struct wl_client *client,
                                                                       struct wl_resource *resource,
                                                                       uint32_t skip)
{
    auto *shell_surface = treeland_dde_shell_surface_frome_resource(resource);
    if (!shell_surface) {
        return;
    }

    if (skip == shell_surface->m_skipDockPreView) {
        return;
    }

    shell_surface->m_skipDockPreView = skip;
    Q_EMIT shell_surface->skipDockPreViewChanged(skip);
}

static void handle_treeland_dde_shell_surface_v1_set_skip_muti_task_view(
    struct wl_client *client,
    struct wl_resource *resource,
    uint32_t skip)
{
    auto *shell_surface = treeland_dde_shell_surface_frome_resource(resource);
    if (!shell_surface) {
        return;
    }

    if (skip == shell_surface->m_skipMutiTaskView) {
        return;
    }

    shell_surface->m_skipMutiTaskView = skip;
    Q_EMIT shell_surface->skipMutiTaskViewChanged(skip);
}

static const struct treeland_dde_shell_surface_v1_interface
    treeland_dde_shell_surface_v1_interface_impl = {
        .destroy = handle_treeland_dde_shell_surface_v1_destroy,
        .set_surface_position = handle_treeland_dde_shell_surface_v1_set_surface_position,
        .set_role = handle_treeland_dde_shell_surface_v1_set_role,
        .set_auto_placement = handle_treeland_dde_shell_surface_v1_set_auto_placement,
        .set_skip_switcher = handle_treeland_dde_shell_surface_v1_set_skip_switcher,
        .set_skip_dock_preview = handle_treeland_dde_shell_surface_v1_set_skip_dock_preview,
        .set_skip_muti_task_view = handle_treeland_dde_shell_surface_v1_set_skip_muti_task_view,
    };

static void treeland_dde_shell_surface_v1_resource_destroy(struct wl_resource *resource)
{
    assert(wl_resource_instance_of(resource,
                                   &treeland_dde_shell_surface_v1_interface,
                                   &treeland_dde_shell_surface_v1_interface_impl));

    auto *shell_surface = treeland_dde_shell_surface_frome_resource(resource);
    if (!shell_surface) {
        return;
    }

    delete shell_surface;
}

static void handle_get_shell_surface(struct wl_client *client,
                                     struct wl_resource *manager_resource,
                                     uint32_t id,
                                     struct wl_resource *surface_resource)
{
    auto *manager = manager_from_resource(manager_resource);
    if (!manager) {
        qCCritical(ddeShellImpl) << "Failed to get treeland_dde_shell_manager_v1";
        return;
    }

    if (!surface_resource) {
        qCCritical(ddeShellImpl) << "surface resource is NULL";
        return;
    }

    auto *shell_surface = new treeland_dde_shell_surface;
    if (!shell_surface) {
        wl_resource_post_no_memory(manager_resource);
        return;
    }

    shell_surface->m_manager = manager;
    uint32_t version = wl_resource_get_version(manager_resource);
    struct wl_resource *resource =
        wl_resource_create(client, &treeland_dde_shell_surface_v1_interface, version, id);
    if (!resource) {
        delete shell_surface;
        wl_resource_post_no_memory(manager_resource);
        return;
    }

    shell_surface->m_resource = resource;
    shell_surface->m_surface_resource = surface_resource;
    wl_resource_set_implementation(resource,
                                   &treeland_dde_shell_surface_v1_interface_impl,
                                   shell_surface,
                                   treeland_dde_shell_surface_v1_resource_destroy);
    wl_resource_set_user_data(resource, shell_surface);
    manager->addShellSurface(shell_surface);
}

static void handle_treeland_dde_active_v1_destroy(struct wl_client *client,
                                                  struct wl_resource *resource)
{
    wl_resource_destroy(resource);
}

static void handle_treeland_multitaskview_v1_destroy(struct wl_client *client,
                                                     struct wl_resource *resource)
{
    wl_resource_destroy(resource);
}

static treeland_multitaskview_v1 *multitaskview_from_resource(struct wl_resource *resource);

static void handle_treeland_multitaskview_v1_toggle(struct wl_client *client,
                                                    struct wl_resource *resource)
{
    auto multitaskview = multitaskview_from_resource(resource);
    Q_ASSERT(multitaskview);
    Q_EMIT multitaskview->toggle();
}

static const struct treeland_dde_active_v1_interface treeland_dde_active_v1_interface_impl = {
    .destroy = handle_treeland_dde_active_v1_destroy,
};

static const struct treeland_multitaskview_v1_interface treeland_multitaskview_v1_interface_impl = {
    .destroy = handle_treeland_multitaskview_v1_destroy,
    .toggle = handle_treeland_multitaskview_v1_toggle
};

static treeland_multitaskview_v1 *multitaskview_from_resource(struct wl_resource *resource)
{
    assert(wl_resource_instance_of(resource,
                                   &treeland_multitaskview_v1_interface,
                                   &treeland_multitaskview_v1_interface_impl));

    treeland_multitaskview_v1 *multitaskview =
        static_cast<treeland_multitaskview_v1 *>(wl_resource_get_user_data(resource));
    assert(multitaskview);
    return multitaskview;
}

static void treeland_dde_active_v1_resource_destroy(struct wl_resource *resource)
{
    assert(wl_resource_instance_of(resource,
                                   &treeland_dde_active_v1_interface,
                                   &treeland_dde_active_v1_interface_impl));

    auto *dde_active = static_cast<treeland_dde_active *>(wl_resource_get_user_data(resource));
    if (!dde_active) {
        return;
    }

    delete dde_active;
}

static void treeland_multitaskview_v1_resource_destroy(struct wl_resource *resource)
{
    assert(wl_resource_instance_of(resource,
                                   &treeland_multitaskview_v1_interface,
                                   &treeland_multitaskview_v1_interface_impl));

    auto *multitaskview =
        static_cast<treeland_multitaskview_v1 *>(wl_resource_get_user_data(resource));
    if (!multitaskview) {
        return;
    }
    Q_EMIT multitaskview->before_destroy();
    delete multitaskview;
}

static void handle_get_treeland_dde_active(struct wl_client *client,
                                           struct wl_resource *manager_resource,
                                           uint32_t id,
                                           struct wl_resource *seat)
{
    auto *manager = manager_from_resource(manager_resource);
    if (!manager) {
        qCCritical(ddeShellImpl) << "Failed to get treeland_dde_shell_manager_v1";
        return;
    }

    if (!seat) {
        qCCritical(ddeShellImpl) << "wl_seat resource is NULL";
        return;
    }

    auto *dde_active = new treeland_dde_active;
    if (!dde_active) {
        wl_resource_post_no_memory(manager_resource);
        return;
    }

    uint32_t version = wl_resource_get_version(manager_resource);
    struct wl_resource *resource =
        wl_resource_create(client, &treeland_dde_active_v1_interface, version, id);
    if (!resource) {
        delete dde_active;
        wl_resource_post_no_memory(manager_resource);
        return;
    }

    dde_active->m_resource = resource;
    dde_active->m_seat_resource = seat;
    wl_resource_set_implementation(resource,
                                   &treeland_dde_active_v1_interface_impl,
                                   dde_active,
                                   treeland_dde_active_v1_resource_destroy);
    wl_resource_set_user_data(resource, dde_active);
    manager->addDdeActive(dde_active);
}

static void handle_get_treeland_multitaskview(struct wl_client *client,
                                              struct wl_resource *manager_resource,
                                              uint32_t id)
{
    auto *manager = manager_from_resource(manager_resource);
    if (!manager) {
        qCCritical(ddeShellImpl) << "Failed to get treeland_dde_shell_manager_v1";
        return;
    }
    auto multitaskview = new treeland_multitaskview_v1;
    if (!multitaskview) {
        wl_resource_post_no_memory(manager_resource);
        return;
    }

    uint32_t version = wl_resource_get_version(manager_resource);
    struct wl_resource *resource =
        wl_resource_create(client, &treeland_multitaskview_v1_interface, version, id);
    if (!resource) {
        delete multitaskview;
        wl_resource_post_no_memory(manager_resource);
        return;
    }

    multitaskview->m_resource = resource;
    wl_resource_set_implementation(resource,
                                   &treeland_multitaskview_v1_interface_impl,
                                   multitaskview,
                                   treeland_multitaskview_v1_resource_destroy);
    wl_resource_set_user_data(resource, multitaskview);
    manager->addMultitaskview(multitaskview);
}

static void treeland_dde_shell_manager_v1_bind(struct wl_client *client,
                                               void *data,
                                               uint32_t version,
                                               uint32_t id)
{
    auto *shell = static_cast<treeland_dde_shell_manager_v1 *>(data);
    struct wl_resource *resource =
        wl_resource_create(client, &treeland_dde_shell_manager_v1_interface, version, id);

    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }

    wl_resource_set_implementation(resource,
                                   &treeland_dde_shell_manager_v1_impl,
                                   shell,
                                   [](struct wl_resource *resource) {
                                       wl_list_remove(wl_resource_get_link(resource));
                                   });

    wl_list_insert(&shell->resources, wl_resource_get_link(resource));
}

treeland_dde_shell_surface::~treeland_dde_shell_surface()
{
    Q_EMIT before_destroy();
}

bool treeland_dde_shell_surface::treeland_dde_shell_surface_is_mapped_to_wsurface(WSurface *surface)
{
    return surface->handle()->handle() == wlr_surface_from_resource(m_surface_resource);
}

void treeland_dde_shell_surface::destroy()
{
    wl_resource_destroy(m_resource);
}

treeland_dde_active::~treeland_dde_active()
{
    Q_EMIT before_destroy();
}

void treeland_dde_active::send_active_in(uint32_t reason)
{
    treeland_dde_active_v1_send_active_in(m_resource, reason);
}

void treeland_dde_active::send_active_out(uint32_t reason)
{
    treeland_dde_active_v1_send_active_out(m_resource, reason);
}

void treeland_dde_active::send_start_drag()
{
    treeland_dde_active_v1_send_start_drag(m_resource);
}

bool treeland_dde_active::treeland_dde_active_is_mapped_to_wseat(WSeat *seat)
{
    if (!m_seat_resource)
        return false;

    return seat->nativeHandle()
        == static_cast<struct wlr_seat_client *>(wl_resource_get_user_data(m_seat_resource))->seat;
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
                              treeland_dde_shell_manager_v1_bind);

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

    wl_list_remove(&resources);
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

void treeland_dde_shell_manager_v1::addShellSurface(treeland_dde_shell_surface *handle)
{
    connect(handle, &treeland_dde_shell_surface::before_destroy, this, [this, handle] {
        m_surfaceHandles.removeOne(handle);
    });

    m_surfaceHandles.append(handle);
    wl_list_insert(&resources, wl_resource_get_link(handle->m_resource));

    Q_EMIT shellSurfaceCreated(handle);
}

void treeland_dde_shell_manager_v1::addDdeActive(treeland_dde_active *handle)
{
    connect(handle, &treeland_dde_active::before_destroy, this, [this, handle] {
        m_ddeActiveHandles.removeOne(handle);
    });

    m_ddeActiveHandles.append(handle);
    wl_list_insert(&resources, wl_resource_get_link(handle->m_resource));

    Q_EMIT ddeActiveCreated(handle);
}

void treeland_dde_shell_manager_v1::addMultitaskview(treeland_multitaskview_v1 *handle)
{
    connect(handle, &treeland_multitaskview_v1::before_destroy, this, [this, handle] {
        m_multitaskviewHandles.removeOne(handle);
    });
    m_multitaskviewHandles.append(handle);
    wl_list_insert(&resources, wl_resource_get_link(handle->m_resource));

    Q_EMIT multitaskviewCreated(handle);
}

treeland_dde_shell_manager_v1 *treeland_dde_shell_manager_v1::create(
    QW_NAMESPACE::qw_display *display)
{
    return new treeland_dde_shell_manager_v1(display);
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
