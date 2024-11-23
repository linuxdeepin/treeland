// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "ddeshellmanagerinterfacev1.h"

#include "qwayland-server-treeland-dde-shell-v1.h"

#include <woutput.h>

#include <qwcompositor.h>
#include <qwdisplay.h>
#include <qwoutput.h>
#include <qwseat.h>

#define TREELAND_DDE_SHELL_MANAGER_V1_VERSION 1

static QList<DDEShellSurfaceInterface *> s_shellSurfaces;
static QList<DDEActiveInterface *> s_ddeActives;
static QList<WindowOverlapCheckerInterface *> s_OverlapCheckers;
static QList<MultiTaskViewInterface *> s_multiTaskViews;
static QList<WindowPickerInterface *> s_windowPickers;
static QList<LockScreenInterface *> s_lockScreens;
static QMap<WindowOverlapCheckerInterface *, QRect> s_conflictList;

class DDEShellManagerInterfaceV1Private : public QtWaylandServer::treeland_dde_shell_manager_v1
{
public:
    DDEShellManagerInterfaceV1Private(DDEShellManagerInterfaceV1 *_q);
    wl_global *global() const;

    DDEShellManagerInterfaceV1 *q;

protected:
    void treeland_dde_shell_manager_v1_get_window_overlap_checker(Resource *resource,
                                                                  uint32_t id) override;
    void treeland_dde_shell_manager_v1_get_shell_surface(Resource *resource,
                                                         uint32_t id,
                                                         struct ::wl_resource *surface) override;
    void treeland_dde_shell_manager_v1_get_treeland_dde_active(Resource *resource,
                                                               uint32_t id,
                                                               struct ::wl_resource *seat) override;
    void treeland_dde_shell_manager_v1_get_treeland_multitaskview(Resource *resource,
                                                                  uint32_t id) override;
    void treeland_dde_shell_manager_v1_get_treeland_window_picker(Resource *resource,
                                                                  uint32_t id) override;
    void treeland_dde_shell_manager_v1_get_treeland_lockscreen(Resource *resource,
                                                               uint32_t id) override;
};

void DDEShellManagerInterfaceV1Private::treeland_dde_shell_manager_v1_get_treeland_lockscreen(Resource *resource, uint32_t id)
{
    wl_resource *lockscreen_resource = wl_resource_create(resource->client(),
                                                       &treeland_lockscreen_v1_interface, resource->version(), id);
    if (!lockscreen_resource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto lockScreen = new LockScreenInterface(lockscreen_resource);
    s_lockScreens.append(lockScreen);

    QObject::connect(lockScreen, &QObject::destroyed, [lockScreen]() {
        s_lockScreens.removeOne(lockScreen);
    });
    Q_EMIT q->lockScreenCreated(lockScreen);
}

DDEShellManagerInterfaceV1Private::DDEShellManagerInterfaceV1Private(DDEShellManagerInterfaceV1 *_q)
    : q(_q)
{
}

wl_global *DDEShellManagerInterfaceV1Private::global() const
{
    return m_global;
}

void DDEShellManagerInterfaceV1Private::treeland_dde_shell_manager_v1_get_window_overlap_checker(
    Resource *resource,
    uint32_t id)
{
    wl_resource *checker_resource = wl_resource_create(resource->client(),
                                                       &treeland_window_overlap_checker_interface,
                                                       resource->version(),
                                                       id);
    if (!checker_resource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto overlapChecker = new WindowOverlapCheckerInterface(checker_resource);
    s_OverlapCheckers.append(overlapChecker);

    QObject::connect(overlapChecker, &QObject::destroyed, [overlapChecker]() {
        s_OverlapCheckers.removeOne(overlapChecker);
    });

    Q_EMIT q->windowOverlapCheckerCreated(overlapChecker);
}

void DDEShellManagerInterfaceV1Private::treeland_dde_shell_manager_v1_get_shell_surface(
    Resource *resource,
    uint32_t id,
    wl_resource *surface)
{
    if (!surface) {
        wl_resource_post_error(resource->handle, 0, "surface resource is NULL!");
        return;
    }

    if (DDEShellSurfaceInterface::get(surface)) {
        wl_resource_post_error(resource->handle, 0, "treeland_dde_shell_surface_v1 already exists");
        return;
    }

    wl_resource *shell_resource = wl_resource_create(resource->client(),
                                                     &treeland_dde_shell_surface_v1_interface,
                                                     resource->version(),
                                                     id);
    if (!shell_resource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto shellSurface = new DDEShellSurfaceInterface(surface, shell_resource);
    s_shellSurfaces.append(shellSurface);

    QObject::connect(shellSurface, &QObject::destroyed, [shellSurface]() {
        s_shellSurfaces.removeOne(shellSurface);
    });

    Q_EMIT q->surfaceCreated(shellSurface);
}

void DDEShellManagerInterfaceV1Private::treeland_dde_shell_manager_v1_get_treeland_dde_active(
    Resource *resource,
    uint32_t id,
    struct ::wl_resource *seat)
{
    if (!seat) {
        wl_resource_post_error(resource->handle, 0, "seat resource is NULL!");
        return;
    }

    wl_resource *active_resource = wl_resource_create(resource->client(),
                                                      &treeland_dde_active_v1_interface,
                                                      resource->version(),
                                                      id);

    if (!active_resource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto active = new DDEActiveInterface(seat, active_resource);
    s_ddeActives.append(active);

    QObject::connect(active, &QObject::destroyed, [active]() {
        s_ddeActives.removeOne(active);
    });

    Q_EMIT q->activeCreated(active);
}

void DDEShellManagerInterfaceV1Private::treeland_dde_shell_manager_v1_get_treeland_multitaskview(
    Resource *resource,
    uint32_t id)
{
    wl_resource *multiTaskViewResource = wl_resource_create(resource->client(),
                                                            &treeland_multitaskview_v1_interface,
                                                            resource->version(),
                                                            id);

    if (!multiTaskViewResource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto multiTaskView = new MultiTaskViewInterface(multiTaskViewResource);
    s_multiTaskViews.append(multiTaskView);

    QObject::connect(multiTaskView, &QObject::destroyed, [multiTaskView]() {
        s_multiTaskViews.removeOne(multiTaskView);
    });
    QObject::connect(multiTaskView,
                     &MultiTaskViewInterface::toggle,
                     q,
                     &DDEShellManagerInterfaceV1::toggleMultitaskview);

    Q_EMIT q->multiTaskViewsCreated(multiTaskView);
}

void DDEShellManagerInterfaceV1Private::treeland_dde_shell_manager_v1_get_treeland_window_picker(
    Resource *resource,
    uint32_t id)
{
    wl_resource *windowPickerResource = wl_resource_create(resource->client(),
                                                           &treeland_window_picker_v1_interface,
                                                           resource->version(),
                                                           id);

    if (!windowPickerResource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto windowPicker = new WindowPickerInterface(windowPickerResource);
    s_windowPickers.append(windowPicker);

    QObject::connect(windowPicker, &QObject::destroyed, [windowPicker]() {
        s_windowPickers.removeOne(windowPicker);
    });
    QObject::connect(windowPicker, &WindowPickerInterface::pick, [windowPicker, this]() {
        Q_EMIT q->requestPickWindow(windowPicker);
    });

    Q_EMIT q->PickerCreated(windowPicker);
}

DDEShellManagerInterfaceV1::DDEShellManagerInterfaceV1(QObject *parent)
    : QObject(parent)
    , d(new DDEShellManagerInterfaceV1Private(this))
{
}

DDEShellManagerInterfaceV1::~DDEShellManagerInterfaceV1() = default;

void DDEShellManagerInterfaceV1::checkRegionalConflict(const QRegion &region) { }

void DDEShellManagerInterfaceV1::create(WServer *server)
{
    d->init(server->handle()->handle(), TREELAND_DDE_SHELL_MANAGER_V1_VERSION);
}

void DDEShellManagerInterfaceV1::destroy(WServer *server)
{
    d = nullptr;
}

wl_global *DDEShellManagerInterfaceV1::global() const
{
    return d->global();
}

QByteArrayView DDEShellManagerInterfaceV1::interfaceName() const
{
    return d->interfaceName();
}

class DDEShellSurfaceInterfacePrivate : public QtWaylandServer::treeland_dde_shell_surface_v1
{
public:
    DDEShellSurfaceInterfacePrivate(DDEShellSurfaceInterface *_q,
                                    wl_resource *surface,
                                    wl_resource *resource);

    DDEShellSurfaceInterface *q;

    wl_resource *surfaceResource{ nullptr };
    std::optional<QPoint> surfacePos;
    std::optional<DDEShellSurfaceInterface::Role> role;
    // if m_yOffset has_value, preventing surface from being displayed beyond
    // the edge of the output.
    std::optional<uint32_t> yOffset;
    std::optional<bool> skipSwitcher;
    std::optional<bool> skipDockPreView;
    std::optional<bool> skipMutiTaskView;

protected:
    void treeland_dde_shell_surface_v1_destroy_resource(Resource *resource) override;
    void treeland_dde_shell_surface_v1_destroy(Resource *resource) override;
    void treeland_dde_shell_surface_v1_set_surface_position(Resource *resource,
                                                            int32_t x,
                                                            int32_t y) override;
    void treeland_dde_shell_surface_v1_set_role(Resource *resource, uint32_t role) override;
    void treeland_dde_shell_surface_v1_set_auto_placement(Resource *resource,
                                                          uint32_t y_offset) override;
    void treeland_dde_shell_surface_v1_set_skip_switcher(Resource *resource,
                                                         uint32_t skip) override;
    void treeland_dde_shell_surface_v1_set_skip_dock_preview(Resource *resource,
                                                             uint32_t skip) override;
    void treeland_dde_shell_surface_v1_set_skip_muti_task_view(Resource *resource,
                                                               uint32_t skip) override;
};

DDEShellSurfaceInterfacePrivate::DDEShellSurfaceInterfacePrivate(DDEShellSurfaceInterface *_q,
                                                                 wl_resource *surface,
                                                                 wl_resource *resource)
    : QtWaylandServer::treeland_dde_shell_surface_v1(resource)
    , surfaceResource(surface)
    , q(_q)
{
}

void DDEShellSurfaceInterfacePrivate::treeland_dde_shell_surface_v1_destroy_resource(
    Resource *resource)
{
    delete q;
}

void DDEShellSurfaceInterfacePrivate::treeland_dde_shell_surface_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void DDEShellSurfaceInterfacePrivate::treeland_dde_shell_surface_v1_set_surface_position(
    Resource *resource,
    int32_t x,
    int32_t y)
{
    QPoint pos(x, y);

    if (surfacePos == pos) {
        return;
    }

    surfacePos = pos;
    Q_EMIT q->positionChanged(pos);
}

void DDEShellSurfaceInterfacePrivate::treeland_dde_shell_surface_v1_set_role(Resource *resource,
                                                                             uint32_t value)
{
    DDEShellSurfaceInterface::Role newRole;
    switch (value) {
    case QtWaylandServer::treeland_dde_shell_surface_v1::role::role_overlay:
        newRole = DDEShellSurfaceInterface::OVERLAY;
        break;
    default:
        wl_resource_post_error(resource->handle,
                               0,
                               "Invalid treeland_dde_shell_surface_v1::role: %u",
                               value);
        return;
    }

    if (role.has_value() && role.value() == newRole) {
        return;
    }

    role = newRole;
    Q_EMIT q->roleChanged(newRole);
}

void DDEShellSurfaceInterfacePrivate::treeland_dde_shell_surface_v1_set_auto_placement(
    Resource *resource,
    uint32_t y_offset)
{
    if (y_offset == yOffset) {
        return;
    }

    yOffset = y_offset;
    Q_EMIT q->yOffsetChanged(y_offset);
}

void DDEShellSurfaceInterfacePrivate::treeland_dde_shell_surface_v1_set_skip_switcher(
    Resource *resource,
    uint32_t skip)
{
    if (skip == skipSwitcher) {
        return;
    }

    skipSwitcher = skip;
    Q_EMIT q->skipSwitcherChanged(skip);
}

void DDEShellSurfaceInterfacePrivate::treeland_dde_shell_surface_v1_set_skip_dock_preview(
    Resource *resource,
    uint32_t skip)
{
    if (skip == skipDockPreView) {
        return;
    }

    skipDockPreView = skip;
    Q_EMIT q->skipDockPreViewChanged(skip);
}

void DDEShellSurfaceInterfacePrivate::treeland_dde_shell_surface_v1_set_skip_muti_task_view(
    Resource *resource,
    uint32_t skip)
{
    if (skip == skipMutiTaskView) {
        return;
    }

    skipMutiTaskView = skip;
    Q_EMIT q->skipMutiTaskViewChanged(skip);
}

DDEShellSurfaceInterface::DDEShellSurfaceInterface(wl_resource *surface, wl_resource *resource)
    : d(new DDEShellSurfaceInterfacePrivate(this, surface, resource))
{
}

DDEShellSurfaceInterface::~DDEShellSurfaceInterface() = default;

WSurface *DDEShellSurfaceInterface::wSurface() const
{
    return WSurface::fromHandle(qw_surface::from(wlr_surface_from_resource(d->surfaceResource)));
}

bool DDEShellSurfaceInterface::ddeShellSurfaceIsMappedToWsurface(const WSurface *surface)
{
    return wSurface() == surface;
}

std::optional<QPoint> DDEShellSurfaceInterface::surfacePos() const
{
    return d->surfacePos;
}

std::optional<DDEShellSurfaceInterface::Role> DDEShellSurfaceInterface::role() const
{
    return d->role;
}

std::optional<uint32_t> DDEShellSurfaceInterface::yOffset() const
{
    return d->yOffset;
}

std::optional<bool> DDEShellSurfaceInterface::skipSwitcher() const
{
    return d->skipSwitcher;
}

std::optional<bool> DDEShellSurfaceInterface::skipDockPreView() const
{
    return d->skipDockPreView;
}

std::optional<bool> DDEShellSurfaceInterface::skipMutiTaskView() const
{
    return d->skipMutiTaskView;
}

DDEShellSurfaceInterface *DDEShellSurfaceInterface::get(wl_resource *native)
{
    WSurface *surface = WSurface::fromHandle(wlr_surface_from_resource(native));
    if (surface) {
        return DDEShellSurfaceInterface::get(surface);
    }

    return nullptr;
}

DDEShellSurfaceInterface *DDEShellSurfaceInterface::get(WSurface *surface)
{
    for (DDEShellSurfaceInterface *shellSurface : std::as_const(s_shellSurfaces)) {
        if (shellSurface->wSurface() == surface) {
            return shellSurface;
        }
    }

    return nullptr;
}

class DDEActiveInterfacePrivate : public QtWaylandServer::treeland_dde_active_v1
{
public:
    DDEActiveInterfacePrivate(DDEActiveInterface *_q, wl_resource *seat, wl_resource *resource);

    DDEActiveInterface *q;
    wl_resource *seatResouce{ nullptr };

protected:
    void treeland_dde_active_v1_destroy_resource(Resource *resource) override;
    void treeland_dde_active_v1_destroy(Resource *resource) override;
};

DDEActiveInterfacePrivate::DDEActiveInterfacePrivate(DDEActiveInterface *_q,
                                                     wl_resource *_seat,
                                                     wl_resource *resource)
    : QtWaylandServer::treeland_dde_active_v1(resource)
    , seatResouce(_seat)
    , q(_q)
{
}

void DDEActiveInterfacePrivate::treeland_dde_active_v1_destroy_resource(Resource *resource)
{
    delete q;
}

void DDEActiveInterfacePrivate::treeland_dde_active_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

DDEActiveInterface::DDEActiveInterface(wl_resource *seat, wl_resource *resource)
    : d(new DDEActiveInterfacePrivate(this, seat, resource))
{
}

DDEActiveInterface::~DDEActiveInterface() = default;

WSeat *DDEActiveInterface::seat() const
{
    auto wlrSeat =
        static_cast<struct wlr_seat_client *>(wl_resource_get_user_data(d->seatResouce))->seat;
    return WSeat::fromHandle(qw_seat::from(wlrSeat));
}

void DDEActiveInterface::sendActiveIn(uint32_t reason)
{
    d->send_active_in(reason);
}

void DDEActiveInterface::sendActiveOut(uint32_t reason)
{
    d->send_active_out(reason);
}

void DDEActiveInterface::sendStartDrag()
{
    d->send_start_drag();
}

void DDEActiveInterface::sendDrop()
{
    d->send_drop();
}

void DDEActiveInterface::sendActiveIn(uint32_t reason, const WSeat *seat)
{
    for (auto interface : s_ddeActives) {
        if (interface->seat() == seat) {
            interface->sendActiveIn(reason);
        }
    }
}

void DDEActiveInterface::sendActiveOut(uint32_t reason, const WSeat *seat)
{
    for (auto interface : s_ddeActives) {
        if (interface->seat() == seat) {
            interface->sendActiveOut(reason);
        }
    }
}

void DDEActiveInterface::sendStartDrag(const WSeat *seat)
{
    for (auto interface : s_ddeActives) {
        if (interface->seat() == seat) {
            interface->sendStartDrag();
        }
    }
}

void DDEActiveInterface::sendDrop(const WSeat *seat)
{
    for (auto interface : s_ddeActives) {
        if (interface->seat() == seat) {
            interface->sendDrop();
        }
    }
}

class WindowOverlapCheckerInterfacePrivate : public QtWaylandServer::treeland_window_overlap_checker
{
public:
    WindowOverlapCheckerInterfacePrivate(WindowOverlapCheckerInterface *_q, wl_resource *resource);

    WindowOverlapCheckerInterface *q;

    struct wlr_output *output;
    QSize size;
    WindowOverlapCheckerInterface::Anchor anchor;
    bool alreadySend{ false };
    bool overlapped{ false };

protected:
    void treeland_window_overlap_checker_destroy_resource(Resource *resource) override;

    void treeland_window_overlap_checker_update(Resource *resource,
                                                int32_t width,
                                                int32_t height,
                                                uint32_t anchor,
                                                struct ::wl_resource *output) override;
    void treeland_window_overlap_checker_destroy(Resource *resource) override;
};

WindowOverlapCheckerInterface::WindowOverlapCheckerInterface(wl_resource *resource)
    : d(new WindowOverlapCheckerInterfacePrivate(this, resource))
{
}

WindowOverlapCheckerInterface::~WindowOverlapCheckerInterface() = default;

void WindowOverlapCheckerInterface::sendOverlapped(bool overlapped)
{
    if (d->alreadySend && overlapped == d->overlapped) {
        d->alreadySend = true;
        return;
    }

    d->overlapped = overlapped;
    d->alreadySend = false;

    if (d->overlapped) {
        d->send_enter();
    } else {
        d->send_leave();
    }
}

void WindowOverlapCheckerInterface::checkRegionalConflict(const QRegion &region)
{
    for (auto &&[interface, checkRect] : s_conflictList.asKeyValueRange()) {
        if (region.intersects(checkRect)) {
            interface->sendOverlapped(true);
            continue;
        } else {
            interface->sendOverlapped(false);
        }
    }
}

WindowOverlapCheckerInterfacePrivate::WindowOverlapCheckerInterfacePrivate(
    WindowOverlapCheckerInterface *_q,
    wl_resource *resource)
    : QtWaylandServer::treeland_window_overlap_checker(resource)
    , q(_q)
{
}

void WindowOverlapCheckerInterfacePrivate::treeland_window_overlap_checker_destroy_resource(
    Resource *resource)
{
    s_conflictList.remove(q);
    delete q;
}

void WindowOverlapCheckerInterfacePrivate::treeland_window_overlap_checker_update(
    Resource *resource,
    int32_t width,
    int32_t height,
    uint32_t anchor,
    wl_resource *o)
{
    output = wlr_output_from_resource(o);
    size = QSize(width, height);

    auto *wOutput = WOutput::fromHandle(qw_output::from(output));
    QRegion region(0, 0, wOutput->size().width(), wOutput->size().height());
    QRect checkRect;
    switch (anchor) {
    case WindowOverlapCheckerInterface::Anchor::TOP:
        checkRect = QRect(0, 0, wOutput->size().width(), size.height());
        break;
    case WindowOverlapCheckerInterface::Anchor::RIGHT:
        checkRect = QRect(wOutput->size().width() - size.width(),
                          0,
                          size.width(),
                          wOutput->size().height());
        break;
    case WindowOverlapCheckerInterface::Anchor::BOTTOM:
        checkRect = QRect(0,
                          wOutput->size().height() - size.height(),
                          wOutput->size().width(),
                          size.height());
        break;
    case WindowOverlapCheckerInterface::Anchor::LEFT:
        checkRect = QRect(0, 0, wOutput->size().width(), size.height());
        break;
    default:
        wl_resource_post_error(resource->handle,
                               0,
                               "Invalid treeland_window_overlap_checker::anchor: %u",
                               anchor);
        return;
    }

    s_conflictList.insert(q, checkRect);
    Q_EMIT q->refresh();
}

void WindowOverlapCheckerInterfacePrivate::treeland_window_overlap_checker_destroy(
    Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

class MultiTaskViewInterfacePrivate : public QtWaylandServer::treeland_multitaskview_v1
{
public:
    MultiTaskViewInterfacePrivate(MultiTaskViewInterface *_q, wl_resource *resource);

    MultiTaskViewInterface *q;

protected:
    void treeland_multitaskview_v1_destroy_resource(Resource *resource) override;
    void treeland_multitaskview_v1_destroy(Resource *resource) override;
    void treeland_multitaskview_v1_toggle(Resource *resource) override;
};

MultiTaskViewInterfacePrivate::MultiTaskViewInterfacePrivate(MultiTaskViewInterface *_q,
                                                             wl_resource *resource)
    : QtWaylandServer::treeland_multitaskview_v1(resource)
    , q(_q)
{
}

void MultiTaskViewInterfacePrivate::treeland_multitaskview_v1_destroy_resource(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void MultiTaskViewInterfacePrivate::treeland_multitaskview_v1_destroy(Resource *resource)
{
    delete q;
}

void MultiTaskViewInterfacePrivate::treeland_multitaskview_v1_toggle(Resource *resource)
{
    Q_EMIT q->toggle();
}

MultiTaskViewInterface::~MultiTaskViewInterface() { }

MultiTaskViewInterface::MultiTaskViewInterface(wl_resource *resource)
    : d(new MultiTaskViewInterfacePrivate(this, resource))
{
}

class WindowPickerInterfacePrivate : public QtWaylandServer::treeland_window_picker_v1
{
public:
    WindowPickerInterfacePrivate(WindowPickerInterface *_q, wl_resource *resource);

    WindowPickerInterface *q;

protected:
    void treeland_window_picker_v1_destroy_resource(Resource *resource) override;
    void treeland_window_picker_v1_destroy(Resource *resource) override;
    void treeland_window_picker_v1_pick(Resource *resource, const QString &hint) override;
};

WindowPickerInterfacePrivate::WindowPickerInterfacePrivate(WindowPickerInterface *_q,
                                                           wl_resource *resource)
    : QtWaylandServer::treeland_window_picker_v1(resource)
    , q(_q)
{
}

void WindowPickerInterfacePrivate::treeland_window_picker_v1_destroy_resource(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void WindowPickerInterfacePrivate::treeland_window_picker_v1_destroy(Resource *resource)
{
    Q_EMIT q->beforeDestroy();
    delete q;
}

void WindowPickerInterfacePrivate::treeland_window_picker_v1_pick(Resource *resource,
                                                                  const QString &hint)
{
    Q_EMIT q->pick(hint);
}

WindowPickerInterface::~WindowPickerInterface() { }

void WindowPickerInterface::sendWindowPid(qint32 pid)
{
    d->send_window(pid);
}

WindowPickerInterface::WindowPickerInterface(wl_resource *resource)
    : d(new WindowPickerInterfacePrivate(this, resource))
{
}

class LockScreenInterfacePrivate : public QtWaylandServer::treeland_lockscreen_v1
{
public:
    LockScreenInterfacePrivate(LockScreenInterface *_q, wl_resource *resource);

    LockScreenInterface *q;

protected:
    void treeland_lockscreen_v1_destroy_resource(Resource *resource) override;
    void treeland_lockscreen_v1_destroy(Resource *resource) override;
    virtual void treeland_lockscreen_v1_lock(Resource *resource) override;
    virtual void treeland_lockscreen_v1_shutdown(Resource *resource) override;
    virtual void treeland_lockscreen_v1_switch_user(Resource *resource) override;
};

void LockScreenInterfacePrivate::treeland_lockscreen_v1_lock(Resource *resource)
{
    Q_EMIT q->lock();
}

void LockScreenInterfacePrivate::treeland_lockscreen_v1_shutdown(Resource *resource)
{
    Q_EMIT q->shutdown();
}

void LockScreenInterfacePrivate::treeland_lockscreen_v1_switch_user(Resource *resource)
{
    Q_EMIT q->switchUser();
}

LockScreenInterfacePrivate::LockScreenInterfacePrivate(LockScreenInterface *_q, wl_resource *resource)
    : QtWaylandServer::treeland_lockscreen_v1(resource)
    , q(_q)
{
}

void LockScreenInterfacePrivate::treeland_lockscreen_v1_destroy_resource(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void LockScreenInterfacePrivate::treeland_lockscreen_v1_destroy(Resource *resource)
{
    delete q;
}

LockScreenInterface::~LockScreenInterface()
{

}

LockScreenInterface::LockScreenInterface(wl_resource *resource)
    : d(new LockScreenInterfacePrivate(this, resource))
{

}
