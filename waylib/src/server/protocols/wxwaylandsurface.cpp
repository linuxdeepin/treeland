// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxwaylandsurface.h"

#include "private/wxwaylandsurface_p.h"
#include "wsurface.h"
#include "wscoplistener.h"
#include "wtools.h"
#include "wxwayland.h"

#include <sys/syscall.h>

#include <climits>

#include <wlr_all.h>

#include <QPointer>

#include <unistd.h>

#include <xcb/xcb_icccm.h>

#define XCOORD_MAX 32767

WAYLIB_SERVER_BEGIN_NAMESPACE

WXWaylandSurfacePrivate::WXWaylandSurfacePrivate(WXWaylandSurface *qq, wlr_xwayland_surface *handle, WXWayland *xwayland)
    : WToplevelSurfacePrivate(qq)
    , xwayland(xwayland)
    , maximized(false)
    , minimized(false)
    , fullscreen(false)
    , activated(false)
{
    Q_ASSERT(handle);
    Q_ASSERT(!handle->data);
    handle->data = qq;
    m_handle = handle;
}

WXWaylandSurfacePrivate::~WXWaylandSurfacePrivate()
{
    if (pidFD >= 0)
        close(pidFD);
}

void WXWaylandSurfacePrivate::init()
{
    W_Q(WXWaylandSurface);

    q->listeners()->add(&m_handle->events.associate, this, [this, q] (void *) {
        Q_ASSERT(!WSurface::fromHandle(handle()->surface));
        surface = new WSurface(handle()->surface);
        surface->setAttachedData<WXWaylandSurface>(q);
        Q_EMIT q->surfaceChanged();
        Q_EMIT q->associated();
    });
    q->listeners()->add(&m_handle->events.dissociate, this, [this, q] (void *) {
        Q_ASSERT(surface);
        Q_EMIT q->aboutToDissociate();
        delete surface;
        surface = nullptr;
        Q_EMIT q->surfaceChanged();
    });
    q->listeners()->add(&m_handle->events.set_parent, this, &WXWaylandSurfacePrivate::updateParent);
    q->listeners()->add(&m_handle->events.request_activate, q, &WXWaylandSurface::requestActivate);
    q->listeners()->add(&m_handle->events.request_configure, this,
                     [this, q] (wlr_xwayland_surface_configure_event *event) {
        lastRequestConfigureGeometry = QRect(event->x, event->y, event->width, event->height);
        lastRequestConfigureFlags = WXWaylandSurface::ConfigureFlags(event->mask);

        if (!surface || !surface->mapped()) {
            q->configure(lastRequestConfigureGeometry);
        } else {
            Q_EMIT q->requestConfigure(lastRequestConfigureGeometry, lastRequestConfigureFlags);
        }
    });
    q->listeners()->add(&m_handle->events.request_fullscreen, this, [this, q] (void *) {
        if (handle()->fullscreen) {
            Q_EMIT q->requestFullscreen(nullptr);
        } else {
            Q_EMIT q->requestCancelFullscreen();
        }
    });
    q->listeners()->add(&m_handle->events.request_maximize, this, [this, q] (void *) {
        if (handle()->maximized_horz && handle()->maximized_vert) {
            Q_EMIT q->requestMaximize();
        } else {
            Q_EMIT q->requestCancelMaximize();
        }
    });
    q->listeners()->add(&m_handle->events.request_minimize, this,
                     [q] (wlr_xwayland_minimize_event *event) {
        if (event->minimize) {
            Q_EMIT q->requestMinimize();
        } else {
            Q_EMIT q->requestCancelMinimize();
        }
    });
    q->listeners()->add(&m_handle->events.request_move, this, [this, q] (void *) {
        Q_EMIT q->requestMove(xwayland->seat(), 0);
    });
    q->listeners()->add(&m_handle->events.request_resize, this,
                     [this, q] (wlr_xwayland_resize_event *event) {
        Q_EMIT q->requestResize(xwayland->seat(), WTools::toQtEdge(event->edges), 0);
    });
    q->listeners()->add(&m_handle->events.set_override_redirect, q, &WXWaylandSurface::bypassManagerChanged);
    q->listeners()->add(&m_handle->events.set_geometry, q, &WXWaylandSurface::geometryChanged);
    q->listeners()->add(&m_handle->events.set_hints, this, &WXWaylandSurfacePrivate::updateSizeHints);
    q->listeners()->add(&m_handle->events.set_window_type, this, &WXWaylandSurfacePrivate::updateWindowTypes);
    q->listeners()->add(&m_handle->events.set_decorations, q, &WXWaylandSurface::decorationsFlagsChanged);
    q->listeners()->add(&m_handle->events.set_title, q, &WXWaylandSurface::titleChanged);
    q->listeners()->add(&m_handle->events.set_class, q, &WXWaylandSurface::appIdChanged);
    updateChildren();
    updateParent();
    updateSizeHints();
    updateWindowTypes();
}

void WXWaylandSurfacePrivate::updateSizeHints()
{
    W_Q(WXWaylandSurface);

    QSize minimumSize;
    QSize maximumSize(INT_MAX, INT_MAX);

    if (handle()->size_hints) {
        if (handle()->size_hints->flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE) {
            minimumSize = QSize(handle()->size_hints->min_width,
                                handle()->size_hints->min_height);
        }
        if (handle()->size_hints->flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE) {
            maximumSize = QSize(handle()->size_hints->max_width,
                                handle()->size_hints->max_height);
        }
    }

    if (this->minimumSize != minimumSize) {
        this->minimumSize = minimumSize;
        Q_EMIT q->minimumSizeChanged(this->minimumSize);
    }

    if (this->maximumSize != maximumSize) {
        this->maximumSize = maximumSize;
        Q_EMIT q->maximumSizeChanged(this->maximumSize);
    }
}

void WXWaylandSurfacePrivate::updateChildren()
{
    QList<WXWaylandSurface*> list;

    struct wlr_xwayland_surface *child, *next;
    wl_list_for_each_safe(child, next, &handle()->children, parent_link) {
        list << WXWaylandSurface::fromHandle(child);
    }

    if (children == list)
        return;

    const bool hasChildChanged = children.isEmpty() != list.isEmpty();
    children = list;

    W_Q(WXWaylandSurface);

    Q_EMIT q->childrenChanged();

    if (hasChildChanged)
        Q_EMIT q->hasChildChanged();
}

void WXWaylandSurfacePrivate::updateParent()
{
    auto newParent = WXWaylandSurface::fromHandle(handle()->parent);
    if (parent == newParent)
        return;

    const bool hasParentChanged = (parent == nullptr) != (newParent == nullptr);
    if (parent)
        parent->d_func()->updateChildren();
    parent = newParent;
    if (parent)
        parent->d_func()->updateChildren();

    W_Q(WXWaylandSurface);

    Q_EMIT q->parentXWaylandSurfaceChanged();

    if (hasParentChanged)
        Q_EMIT q->isToplevelChanged();
}

void WXWaylandSurfacePrivate::updateWindowTypes()
{
    WXWaylandSurface::WindowTypes types = {0};

    for (size_t i = 0; i < handle()->window_type_len; ++i) {
        auto atomType = xwayland->atomType(handle()->window_type[i]);
        
        switch (atomType) {
        case WXWayland::_NET_WM_WINDOW_TYPE_NORMAL:
            types |= WXWaylandSurface::NET_WM_WINDOW_TYPE_NORMAL;
            break;
        case WXWayland::_NET_WM_WINDOW_TYPE_UTILITY:
            types |= WXWaylandSurface::NET_WM_WINDOW_TYPE_UTILITY;
            break;
        case WXWayland::_NET_WM_WINDOW_TYPE_TOOLTIP:
            types |= WXWaylandSurface::NET_WM_WINDOW_TYPE_TOOLTIP;
            break;
        case WXWayland::_NET_WM_WINDOW_TYPE_DND:
            types |= WXWaylandSurface::NET_WM_WINDOW_TYPE_DND;
            break;
        case WXWayland::_NET_WM_WINDOW_TYPE_DROPDOWN_MENU:
            types |= WXWaylandSurface::NET_WM_WINDOW_TYPE_DROPDOWN_MENU;
            break;
        case WXWayland::_NET_WM_WINDOW_TYPE_POPUP_MENU:
            types |= WXWaylandSurface::NET_WM_WINDOW_TYPE_POPUP_MENU;
            break;
        case WXWayland::_NET_WM_WINDOW_TYPE_COMBO:
            types |= WXWaylandSurface::NET_WM_WINDOW_TYPE_COMBO;
            break;
        case WXWayland::_NET_WM_WINDOW_TYPE_MENU:
            types |= WXWaylandSurface::NET_WM_WINDOW_TYPE_MENU;
            break;
        case WXWayland::_NET_WM_WINDOW_TYPE_NOTIFICATION:
            types |= WXWaylandSurface::NET_WM_WINDOW_TYPE_NOTIFICATION;
            break;
        case WXWayland::_NET_WM_WINDOW_TYPE_SPLASH:
            types |= WXWaylandSurface::NET_WM_WINDOW_TYPE_SPLASH;
            break;
        case WXWayland::_NET_WM_WINDOW_TYPE_DIALOG:
            types |= WXWaylandSurface::NET_WM_WINDOW_TYPE_DIALOG;
            break;
        default:
            break;
        }
    }

    if (windowTypes == types)
        return;

    windowTypes = types;
    Q_EMIT q_func()->windowTypesChanged();
}

WXWaylandSurface::WXWaylandSurface(wlr_xwayland_surface *handle, WXWayland *xwayland)
    : WToplevelSurface(*new WXWaylandSurfacePrivate(this, handle, xwayland))
{
    d_func()->init();
}

WXWaylandSurface::~WXWaylandSurface()
{
    teardown();
    // Notify listeners while the object is still usable (its members are
    // alive during the destructor body).
    Q_EMIT beforeDestroy();
    // Owner rule: this object created the WSurface wrapper, release it.
    W_D(WXWaylandSurface);
    // Clear the reverse fromHandle() mapping while the native handle is
    // still alive (WXWayland deletes this wrapper from the destroy
    // callback, or the creator deletes it earlier).
    if (d->m_handle && d->m_handle->data == this)
        d->m_handle->data = nullptr;
    delete d->surface;
}

WXWaylandSurface *WXWaylandSurface::fromHandle(wlr_xwayland_surface *handle)
{
    if (!handle)
        return nullptr;
    return static_cast<WXWaylandSurface*>(handle->data);
}

WXWaylandSurface *WXWaylandSurface::fromSurface(WSurface *surface)
{
    return surface->getAttachedData<WXWaylandSurface>();
}

WSurface *WXWaylandSurface::surface() const
{
    W_DC(WXWaylandSurface);

    return d->surface;
}

wlr_xwayland_surface *WXWaylandSurface::handle() const
{
    W_DC(WXWaylandSurface);

    return d->handle();
}

WXWaylandSurface *WXWaylandSurface::parentXWaylandSurface() const
{
    W_DC(WXWaylandSurface);

    return d->parent;
}

WXWayland *WXWaylandSurface::xwayland() const
{
    W_DC(WXWaylandSurface);

    return d->xwayland;
}

void WXWaylandSurfacePrivate::handleParentDestroyed(WXWaylandSurface *parent)
{
    W_Q(WXWaylandSurface);
    if (this->parent != parent)
        return;
    // Mirrors what updateParent() would do once the native set_parent event
    // arrives (native parent is already NULL): the wrapper of the dying
    // parent is still alive here, so the QPointer comparison can proceed.
    this->parent = nullptr;
    Q_EMIT q->parentXWaylandSurfaceChanged();
    Q_EMIT q->isToplevelChanged();
}

const QList<WXWaylandSurface*> &WXWaylandSurface::children() const
{
    W_DC(WXWaylandSurface);

    return d->children;
}

bool WXWaylandSurface::isToplevel() const
{
    W_DC(WXWaylandSurface);
    return !d->handle()->parent;
}

bool WXWaylandSurface::hasChild() const
{
    W_DC(WXWaylandSurface);
    return wl_list_empty(&d->handle()->children) == 0;
}

bool WXWaylandSurface::isMaximized() const
{
    W_DC(WXWaylandSurface);
    return d->maximized;
}

bool WXWaylandSurface::isMinimized() const
{
    W_DC(WXWaylandSurface);
    return d->minimized;
}

bool WXWaylandSurface::isFullScreen() const
{
    W_DC(WXWaylandSurface);
    return d->fullscreen;
}

bool WXWaylandSurface::isActivated() const
{
    W_DC(WXWaylandSurface);
    return d->activated;
}

bool WXWaylandSurface::hasCapability(Capability cap) const
{
    W_DC(WXWaylandSurface);
    switch (cap) {
        using enum Capability;
    case Resize:
        return !isBypassManager() && (minSize().width() < maxSize().width()
                                      || minSize().height() < maxSize().height());
    case Maximized:
        if (isBypassManager()) {
            return false;
        }
        return (minSize().width() < maxSize().width() && minSize().height() < maxSize().height())
            && !(d->windowTypes
                 & (NET_WM_WINDOW_TYPE_UTILITY | NET_WM_WINDOW_TYPE_TOOLTIP | NET_WM_WINDOW_TYPE_DND
                    | NET_WM_WINDOW_TYPE_DROPDOWN_MENU | NET_WM_WINDOW_TYPE_POPUP_MENU
                    | NET_WM_WINDOW_TYPE_COMBO | NET_WM_WINDOW_TYPE_MENU
                    | NET_WM_WINDOW_TYPE_NOTIFICATION | NET_WM_WINDOW_TYPE_SPLASH));
    // TODO(xwayland): To implement ICCCM focus model, refer to
    // https://github.com/labwc/labwc/blob/9ba3303246b1b1bf4dbf13793faa62ec74872e3b/src/xwayland.c#L108
    case Activate: [[fallthrough]];
    case Focus:
        return !isBypassManager()
            && wlr_xwayland_surface_override_redirect_wants_focus(d->handle())
            && wlr_xwayland_surface_icccm_input_model(d->handle()) != WLR_ICCCM_INPUT_MODEL_NONE;
    case FullScreen:
        return !isBypassManager();
    default:
        break;
    }
    Q_UNREACHABLE();
}

QSize WXWaylandSurface::minSize() const
{
    W_DC(WXWaylandSurface);
    return d->minimumSize;
}

QSize WXWaylandSurface::maxSize() const
{
    W_DC(WXWaylandSurface);
    return d->maximumSize;
}

QRect WXWaylandSurface::geometry() const
{
    W_DC(WXWaylandSurface);

    QRect geometry = getContentGeometry();
    geometry.moveTopLeft(QPoint(d->handle()->x, d->handle()->y));

    return geometry;
}

QRect WXWaylandSurface::getContentGeometry() const
{
    W_DC(WXWaylandSurface);

    return QRect(0, 0, d->handle()->width, d->handle()->height);
}

QString WXWaylandSurface::title() const
{
    W_DC(WXWaylandSurface);

    return QString::fromUtf8(d->handle()->title);
}

QString WXWaylandSurface::appId() const
{
    W_DC(WXWaylandSurface);
    // https://www.x.org/releases/X11R7.7/doc/xproto/x11protocol.html#requests:InternAtom
    // The string should use the ISO Latin-1 encoding.
    return QString::fromLatin1(d->handle()->instance);
}

pid_t WXWaylandSurface::pid() const
{
    W_DC(WXWaylandSurface);

    return d->handle()->pid;
}

int WXWaylandSurface::pidFD() const
{
    W_DC(WXWaylandSurface);

    if (d->pidFD == -1) {
        d->pidFD = syscall(SYS_pidfd_open, pid(), 0);
    }

    return d->pidFD;
}

QRect WXWaylandSurface::requestConfigureGeometry() const
{
    W_DC(WXWaylandSurface);

    QRect rect = d->lastRequestConfigureGeometry;
    if (!d->lastRequestConfigureFlags.testFlag(XCB_CONFIG_WINDOW_X))
        rect.moveLeft(d->handle()->x);
    if (!d->lastRequestConfigureFlags.testFlag(XCB_CONFIG_WINDOW_Y))
        rect.moveTop(d->handle()->y);
    if (!d->lastRequestConfigureFlags.testFlag(XCB_CONFIG_WINDOW_WIDTH))
        rect.setWidth(d->handle()->width);
    if (!d->lastRequestConfigureFlags.testFlag(XCB_CONFIG_WINDOW_HEIGHT))
        rect.setHeight(d->handle()->height);

    return rect;
}

WXWaylandSurface::ConfigureFlags WXWaylandSurface::requestConfigureFlags() const
{
    W_DC(WXWaylandSurface);
    return d->lastRequestConfigureFlags;
}

bool WXWaylandSurface::isBypassManager() const
{
    W_DC(WXWaylandSurface);
    return d->handle()->override_redirect;
}

WXWaylandSurface::WindowTypes WXWaylandSurface::windowTypes() const
{
    W_DC(WXWaylandSurface);
    return d->windowTypes;
}

WXWaylandSurface::DecorationsFlags WXWaylandSurface::decorationsFlags() const
{
    W_DC(WXWaylandSurface);
    return WXWaylandSurface::DecorationsFlags::fromInt(d->handle()->decorations);
}

bool WXWaylandSurface::checkNewSize(const QSize &size, QSize *clipedSize)
{
    const QSize minSize = this->minSize();
    const QSize maxSize = this->maxSize();

    bool ok = true;
    if (clipedSize)
        *clipedSize = size;

    if (size.width() < minSize.width()) {
        if (clipedSize)
            clipedSize->setWidth(minSize.width());
        ok = false;
    }
    if (size.height() < minSize.height()) {
        if (clipedSize)
            clipedSize->setHeight(minSize.height());
        ok = false;
    }
    if (size.width() > maxSize.width() && maxSize.width() > 0) {
        if (clipedSize)
            clipedSize->setWidth(maxSize.width());
        ok = false;
    }
    if (size.height() > maxSize.height() && maxSize.height() > 0) {
        if (clipedSize)
            clipedSize->setHeight(maxSize.height());
        ok = false;
    }

    return ok;
}

void WXWaylandSurface::resize(const QSize &size)
{
    W_DC(WXWaylandSurface);
    wlr_xwayland_surface_configure(handle(), d->handle()->x, d->handle()->y, size.width(), size.height());
}

void WXWaylandSurface::configure(const QRect &geometry)
{
    wlr_xwayland_surface_configure(handle(), geometry.x(), geometry.y(), geometry.width(), geometry.height());
}

void WXWaylandSurface::setMaximize(bool on)
{
    W_D(WXWaylandSurface);

    if (d->maximized == on && d->isMaximized() == on)
        return;

    d->maximized = on;
    wlr_xwayland_surface_set_maximized(handle(), on, on);
    Q_EMIT maximizeChanged();
}

void WXWaylandSurface::setMinimize(bool on)
{
    W_D(WXWaylandSurface);

    if (d->minimized == on && d->handle()->minimized == on)
        return;

    d->minimized = on;
    wlr_xwayland_surface_set_minimized(handle(), on);
    Q_EMIT minimizeChanged();
}

void WXWaylandSurface::setFullScreen(bool on)
{
    W_D(WXWaylandSurface);

    if (d->fullscreen == on && d->handle()->fullscreen == on)
        return;

    d->fullscreen = on;
    wlr_xwayland_surface_set_fullscreen(handle(), on);
    Q_EMIT fullscreenChanged();
}

void WXWaylandSurface::setActivate(bool on)
{
    W_D(WXWaylandSurface);

    if (d->activated == on)
        return;

    d->activated = on;
    wlr_xwayland_surface_activate(handle(), on);
    Q_EMIT activateChanged();
}

void WXWaylandSurface::close()
{
    wlr_xwayland_surface_close(handle());
}

void WXWaylandSurface::restack(WXWaylandSurface *sibling, StackMode mode)
{
    if (sibling) {
        wlr_xwayland_surface_restack(handle(), sibling->handle(), static_cast<xcb_stack_mode_t>(mode));
        return;
    }

    wlr_xwayland_surface_restack(handle(), nullptr, static_cast<xcb_stack_mode_t>(mode));
}

WAYLIB_SERVER_END_NAMESPACE
