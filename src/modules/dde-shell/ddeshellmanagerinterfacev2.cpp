// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "ddeshellmanagerinterfacev2.h"

#include "qwayland-server-treeland-dde-shell-unstable-v2.h"

#include "helper.h"
#include "output.h"
#include "rootsurfacecontainer.h"

#include <wsurface.h>

#include <woutput.h>

#include <wayland-server.h>

#include <cstddef>
#include <cstring>

static QList<DDEShellSurfaceV2 *> s_shellSurfacesV2;

class DDEShellManagerInterfaceV2Private : public QtWaylandServer::treeland_dde_shell_manager_v2
{
public:
    explicit DDEShellManagerInterfaceV2Private(DDEShellManagerInterfaceV2 *_q);
    wl_global *global() const;

    DDEShellManagerInterfaceV2 *q;

protected:
    void destroy(Resource *resource) override;
    void get_shell_surface(Resource *resource, uint32_t id, struct ::wl_resource *surface) override;
};

DDEShellManagerInterfaceV2Private::DDEShellManagerInterfaceV2Private(DDEShellManagerInterfaceV2 *_q)
    : QtWaylandServer::treeland_dde_shell_manager_v2()
    , q(_q)
{
}

wl_global *DDEShellManagerInterfaceV2Private::global() const
{
    return m_global;
}

void DDEShellManagerInterfaceV2Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void DDEShellManagerInterfaceV2Private::get_shell_surface(Resource *resource,
                                                          uint32_t id,
                                                          wl_resource *surface)
{
    if (!surface) {
        wl_resource_post_error(resource->handle, 0, "surface resource is NULL!");
        return;
    }

    // wlr_surface_from_resource asserts on a non-wl_surface resource; validate
    // the class first so a hostile client gets a protocol error instead of
    // aborting the compositor.
    if (strcmp(wl_resource_get_class(surface), wl_surface_interface.name) != 0) {
        wl_resource_post_error(resource->handle, 0, "invalid wl_surface resource");
        return;
    }

    if (DDEShellSurfaceV2::getByWlrSurface(wlr_surface_from_resource(surface))) {
        ++q->m_alreadyShellSurfaceErrors;
        wl_resource_post_error(resource->handle,
                               error_already_shell_surface,
                               "the wl_surface already has a shell surface object");
        return;
    }

    wl_resource *shell_resource = wl_resource_create(resource->client(),
                                                     &treeland_dde_shell_surface_v2_interface,
                                                     resource->version(),
                                                     id);
    if (!shell_resource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto shellSurface = new DDEShellSurfaceV2(surface, shell_resource);
    s_shellSurfacesV2.append(shellSurface);

    QObject::connect(shellSurface, &QObject::destroyed, [shellSurface]() {
        s_shellSurfacesV2.removeOne(shellSurface);
    });

    Q_EMIT q->surfaceCreated(shellSurface);
}

DDEShellManagerInterfaceV2::DDEShellManagerInterfaceV2(QObject *parent)
    : QObject(parent)
    , d(new DDEShellManagerInterfaceV2Private(this))
{
}

DDEShellManagerInterfaceV2::~DDEShellManagerInterfaceV2() = default;

int DDEShellManagerInterfaceV2::alreadyShellSurfaceErrorCount() const
{
    return m_alreadyShellSurfaceErrors;
}

void DDEShellManagerInterfaceV2::create(WServer *server)
{
    d->init(server->handle(), InterfaceVersion);
}

void DDEShellManagerInterfaceV2::destroy([[maybe_unused]] WServer *server)
{
    d->globalRemove();
}

wl_global *DDEShellManagerInterfaceV2::global() const
{
    return d->global();
}

QByteArrayView DDEShellManagerInterfaceV2::interfaceName() const
{
    return d->interfaceName();
}

class DDEShellSurfaceV2Private : public QtWaylandServer::treeland_dde_shell_surface_v2
{
public:
    DDEShellSurfaceV2Private(DDEShellSurfaceV2 *_q,
                             wl_resource *surface,
                             wl_resource *resource);
    ~DDEShellSurfaceV2Private() override;

    DDEShellSurfaceV2 *q;
    wl_resource *surfaceResource{ nullptr };
    wl_listener surfaceDestroyListener;
    DDEShellSurfaceV2::Role role = DDEShellSurfaceV2::OVERLAY;
    // Exactly one of the two placement hints has a value: the most recently
    // sent request decides the placement mode.
    std::optional<QPoint> positionHint;
    std::optional<QPoint> cursorPlacementHint;
    uint32_t skipFlags = 0;
    bool acceptKeyboardFocus = true;

protected:
    void destroy_resource([[maybe_unused]] Resource *resource) override;
    void destroy([[maybe_unused]] Resource *resource) override;
    void set_role([[maybe_unused]] Resource *resource, uint32_t value) override;
    void set_position_hint([[maybe_unused]] Resource *resource,
                           struct ::wl_resource *output,
                           int32_t x,
                           int32_t y) override;
    void set_cursor_placement_hint([[maybe_unused]] Resource *resource,
                                   int32_t x_offset,
                                   int32_t y_offset) override;
    void set_skip_flags([[maybe_unused]] Resource *resource, uint32_t flags) override;
    void set_accept_keyboard_focus([[maybe_unused]] Resource *resource, uint32_t accept) override;

private:
    static void handleSurfaceDestroyed(wl_listener *listener, void *data);
};

DDEShellSurfaceV2Private::DDEShellSurfaceV2Private(DDEShellSurfaceV2 *_q,
                                                   wl_resource *surface,
                                                   wl_resource *resource)
    : QtWaylandServer::treeland_dde_shell_surface_v2(resource)
    , q(_q)
    , surfaceResource(surface)
{
    wl_list_init(&surfaceDestroyListener.link);
    // The v2 protocol promises the surface object is destroyed automatically
    // when the related wl_surface goes away. Listen on the native surface
    // destroy signal so the cleanup does not depend on the waylib wrapper.
    if (auto *wlrSurface = wlr_surface_from_resource(surface)) {
        surfaceDestroyListener.notify = &DDEShellSurfaceV2Private::handleSurfaceDestroyed;
        wl_signal_add(&wlrSurface->events.destroy, &surfaceDestroyListener);
    }
}

DDEShellSurfaceV2Private::~DDEShellSurfaceV2Private()
{
    if (!wl_list_empty(&surfaceDestroyListener.link))
        wl_list_remove(&surfaceDestroyListener.link);
}

void DDEShellSurfaceV2Private::handleSurfaceDestroyed(wl_listener *listener, void *data)
{
    auto *p = reinterpret_cast<DDEShellSurfaceV2Private *>(
        reinterpret_cast<char *>(listener)
        - offsetof(DDEShellSurfaceV2Private, surfaceDestroyListener));
    wl_list_remove(&p->surfaceDestroyListener.link);
    wl_list_init(&p->surfaceDestroyListener.link);
    if (p->resource())
        wl_resource_destroy(p->resource()->handle);
}

void DDEShellSurfaceV2Private::destroy_resource([[maybe_unused]] Resource *resource)
{
    delete q;
}

void DDEShellSurfaceV2Private::destroy([[maybe_unused]] Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void DDEShellSurfaceV2Private::set_role([[maybe_unused]] Resource *resource,
                                        uint32_t value)
{
    DDEShellSurfaceV2::Role newRole;
    switch (value) {
    case QtWaylandServer::treeland_dde_shell_surface_v2::role::role_overlay:
        newRole = DDEShellSurfaceV2::OVERLAY;
        break;
    default:
        wl_resource_post_error(resource->handle,
                               0,
                               "Invalid treeland_dde_shell_surface_v2::role: %u",
                               value);
        return;
    }

    if (role == newRole) {
        return;
    }

    role = newRole;
    Q_EMIT q->roleChanged(newRole);
}

void DDEShellSurfaceV2Private::set_position_hint([[maybe_unused]] Resource *resource,
                                                 wl_resource *output,
                                                 int32_t x,
                                                 int32_t y)
{
    QPoint anchor;
    if (output) {
        if (strcmp(wl_resource_get_class(output), wl_output_interface.name) != 0) {
            wl_resource_post_error(resource->handle, 0, "invalid wl_output resource");
            return;
        }
        auto *wOutput = WOutput::fromHandle(wlr_output_from_resource(output));
        if (!wOutput) {
            wl_resource_post_error(resource->handle, 0, "invalid wl_output resource");
            return;
        }
        anchor = wOutput->position();
    } else {
        // A null output anchors the coordinates at the primary output origin.
        if (auto *primary = Helper::instance()->rootContainer()->primaryOutput())
            anchor = primary->geometry().topLeft().toPoint();
    }

    const bool modeSwitched = cursorPlacementHint.has_value();
    cursorPlacementHint.reset();

    const QPoint globalPos = anchor + QPoint(x, y);
    if (positionHint == globalPos && !modeSwitched) {
        return;
    }

    positionHint = globalPos;
    Q_EMIT q->positionHintChanged(globalPos);
}

void DDEShellSurfaceV2Private::set_cursor_placement_hint([[maybe_unused]] Resource *resource,
                                                         int32_t x_offset,
                                                         int32_t y_offset)
{
    const bool modeSwitched = positionHint.has_value();
    positionHint.reset();

    const QPoint offset(x_offset, y_offset);
    if (cursorPlacementHint == offset && !modeSwitched) {
        return;
    }

    cursorPlacementHint = offset;
    Q_EMIT q->cursorPlacementHintChanged(offset);
}

void DDEShellSurfaceV2Private::set_skip_flags([[maybe_unused]] Resource *resource,
                                              uint32_t flags)
{
    if (skipFlags == flags) {
        return;
    }

    skipFlags = flags;
    Q_EMIT q->skipFlagsChanged(flags);
}

void DDEShellSurfaceV2Private::set_accept_keyboard_focus([[maybe_unused]] Resource *resource,
                                                         uint32_t accept)
{
    const bool newAccept = accept != 0;
    if (acceptKeyboardFocus == newAccept) {
        return;
    }

    acceptKeyboardFocus = newAccept;
    Q_EMIT q->acceptKeyboardFocusChanged(newAccept);
}

DDEShellSurfaceV2::DDEShellSurfaceV2(wl_resource *surface, wl_resource *resource)
    : d(new DDEShellSurfaceV2Private(this, surface, resource))
{
}

DDEShellSurfaceV2::~DDEShellSurfaceV2() = default;

WSurface *DDEShellSurfaceV2::wSurface() const
{
    return WSurface::fromHandle(wlr_surface_from_resource(d->surfaceResource));
}

DDEShellSurfaceV2::Role DDEShellSurfaceV2::role() const
{
    return d->role;
}

std::optional<QPoint> DDEShellSurfaceV2::positionHint() const
{
    return d->positionHint;
}

std::optional<QPoint> DDEShellSurfaceV2::cursorPlacementHint() const
{
    return d->cursorPlacementHint;
}

uint32_t DDEShellSurfaceV2::skipFlags() const
{
    return d->skipFlags;
}

bool DDEShellSurfaceV2::acceptKeyboardFocus() const
{
    return d->acceptKeyboardFocus;
}

DDEShellSurfaceV2 *DDEShellSurfaceV2::get(wl_resource *native)
{
    WSurface *surface = WSurface::fromHandle(wlr_surface_from_resource(native));
    if (surface) {
        return DDEShellSurfaceV2::get(surface);
    }

    return nullptr;
}

DDEShellSurfaceV2 *DDEShellSurfaceV2::get(WSurface *surface)
{
    if (!surface)
        return nullptr;
    return getByWlrSurface(surface->handle());
}

DDEShellSurfaceV2 *DDEShellSurfaceV2::getByWlrSurface(wlr_surface *handle)
{
    for (DDEShellSurfaceV2 *shellSurface : std::as_const(s_shellSurfacesV2)) {
        if (wlr_surface_from_resource(shellSurface->d->surfaceResource) == handle) {
            return shellSurface;
        }
    }

    return nullptr;
}
