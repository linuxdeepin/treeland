// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxwaylandsurface.h"
#include "wsurface.h"
#include "wtools.h"
#include "wxwayland.h"
#include "private/wtoplevelsurface_p.h"

#include <qwxwaylandsurface.h>
#include <qwcompositor.h>

#define XCOORD_MAX 32767

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WXWaylandSurfacePrivate : public WToplevelSurfacePrivate
{
public:
    WXWaylandSurfacePrivate(WXWaylandSurface *qq, qw_xwayland_surface *handle, WXWayland *xwayland)
        : WToplevelSurfacePrivate(qq)
        , xwayland(xwayland)
        , maximized(false)
        , minimized(false)
        , fullscreen(false)
        , activated(false)
    {
        initHandle(handle);
    }

    WWRAP_HANDLE_FUNCTIONS(qw_xwayland_surface, wlr_xwayland_surface)

    inline bool isMaximized() const {
        return nativeHandle()->maximized_horz && nativeHandle()->maximized_vert;
    }

    wl_client *waylandClient() const override {
        return surface->handle()->handle()->resource->client;
    }

    void instantRelease() override;

    void init();
    void updateChildren();
    void updateParent();
    void updateWindowTypes();

    W_DECLARE_PUBLIC(WXWaylandSurface)

    WSurface *surface = nullptr;
    WXWayland *xwayland = nullptr;
    QList<WXWaylandSurface*> children;
    WXWaylandSurface *parent = nullptr;
    QRect lastRequestConfigureGeometry;
    WXWaylandSurface::ConfigureFlags lastRequestConfigureFlags = {0};
    WXWaylandSurface::WindowTypes windowTypes = {0};
    uint maximized:1;
    uint minimized:1;
    uint fullscreen:1;
    uint activated:1;
};

void WXWaylandSurfacePrivate::instantRelease()
{
    handle()->set_data(nullptr, nullptr);
    if (surface)
        surface->removeAttachedData<WXWaylandSurface>();
}

void WXWaylandSurfacePrivate::init()
{
    W_Q(WXWaylandSurface);
    handle()->set_data(this, q);

    QObject::connect(handle(), &qw_xwayland_surface::notify_associate, q, [this, q] {
        Q_ASSERT(!WSurface::fromHandle(nativeHandle()->surface));
        surface = new WSurface(qw_surface::from(nativeHandle()->surface), q);
        surface->setAttachedData<WXWaylandSurface>(q);
        Q_EMIT q->surfaceChanged();
    });
    QObject::connect(handle(), &qw_xwayland_surface::notify_dissociate, q, [this, q] {
        Q_ASSERT(surface);
        surface->safeDeleteLater();
        surface = nullptr;
        Q_EMIT q->surfaceChanged();
    });
    QObject::connect(handle(), &qw_xwayland_surface::notify_set_parent, q, [this] {
        updateParent();
    });
    QObject::connect(handle(), &qw_xwayland_surface::notify_request_activate, q, &WXWaylandSurface::requestActivate);
    QObject::connect(handle(), &qw_xwayland_surface::notify_request_configure,
                     q, [this, q] (wlr_xwayland_surface_configure_event *event) {
        lastRequestConfigureGeometry = QRect(event->x, event->y, event->width, event->height);
        lastRequestConfigureFlags = WXWaylandSurface::ConfigureFlags(event->mask);

        if (!surface || !surface->mapped()) {
            q->configure(lastRequestConfigureGeometry);
        } else {
            Q_EMIT q->requestConfigure(lastRequestConfigureGeometry, lastRequestConfigureFlags);
        }
    });
    QObject::connect(handle(), &qw_xwayland_surface::notify_request_fullscreen, q, [this, q] {
        if (nativeHandle()->fullscreen) {
            Q_EMIT q->requestFullscreen();
        } else {
            Q_EMIT q->requestCancelFullscreen();
        }
    });
    QObject::connect(handle(), &qw_xwayland_surface::notify_request_maximize, q, [this, q] {
        if (nativeHandle()->maximized_horz && nativeHandle()->maximized_vert) {
            Q_EMIT q->requestMaximize();
        } else {
            Q_EMIT q->requestCancelMaximize();
        }
    });
    QObject::connect(handle(), &qw_xwayland_surface::notify_request_minimize,
                     q, [this, q] (wlr_xwayland_minimize_event *event) {
        if (event->minimize) {
            Q_EMIT q->requestMinimize();
        } else {
            Q_EMIT q->requestCancelMinimize();
        }
    });
    QObject::connect(handle(), &qw_xwayland_surface::notify_request_move,
                     q, [this, q] {
        Q_EMIT q->requestMove(xwayland->seat(), 0);
    });
    QObject::connect(handle(), &qw_xwayland_surface::notify_request_resize,
                     q, [this, q] (wlr_xwayland_resize_event *event) {
        Q_EMIT q->requestResize(xwayland->seat(), WTools::toQtEdge(event->edges), 0);
    });
    QObject::connect(handle(), &qw_xwayland_surface::notify_set_override_redirect,
                     q, &WXWaylandSurface::bypassManagerChanged);
    QObject::connect(handle(), &qw_xwayland_surface::notify_set_geometry,
                     q, &WXWaylandSurface::geometryChanged);
    QObject::connect(handle(), &qw_xwayland_surface::notify_set_window_type,
                     q, [this] {
                         updateWindowTypes();
                     });
    QObject::connect(handle(), &qw_xwayland_surface::notify_set_decorations,
                     q, &WXWaylandSurface::decorationsTypeChanged);
    QObject::connect(handle(), &qw_xwayland_surface::notify_set_title,
                     q, &WXWaylandSurface::titleChanged);
    QObject::connect(handle(), &qw_xwayland_surface::notify_set_class,
                     q, &WXWaylandSurface::appIdChanged);
    updateChildren();
    updateParent();
    updateWindowTypes();
}

void WXWaylandSurfacePrivate::updateChildren()
{
    QList<WXWaylandSurface*> list;

    struct wlr_xwayland_surface *child, *next;
    wl_list_for_each_safe(child, next, &nativeHandle()->children, parent_link) {
        list << WXWaylandSurface::fromHandle(qw_xwayland_surface::from(child));
    }

    if (children == list)
        return;

    const bool hasChildChanged = children.isEmpty() || list.isEmpty();
    children = list;

    W_Q(WXWaylandSurface);

    Q_EMIT q->childrenChanged();

    if (hasChildChanged)
        Q_EMIT q->hasChildChanged();
}

void WXWaylandSurfacePrivate::updateParent()
{
    auto newParent = WXWaylandSurface::fromHandle(nativeHandle()->parent);
    if (parent == newParent)
        return;

    const bool hasParentChanged = parent == nullptr || newParent == nullptr;
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

    for (int i = 0; i < nativeHandle()->window_type_len; ++i) {
        switch (xwayland->atomType(nativeHandle()->window_type[i])) {
        case WXWayland::NET_WM_WINDOW_TYPE_NORMAL:
            types |= WXWaylandSurface::NET_WM_WINDOW_TYPE_NORMAL;
            break;
        case WXWayland::NET_WM_WINDOW_TYPE_UTILITY:
            types |= WXWaylandSurface::NET_WM_WINDOW_TYPE_UTILITY;
            break;
        case WXWayland::NET_WM_WINDOW_TYPE_TOOLTIP:
            types |= WXWaylandSurface::NET_WM_WINDOW_TYPE_TOOLTIP;
            break;
        case WXWayland::NET_WM_WINDOW_TYPE_DND:
            types |= WXWaylandSurface::NET_WM_WINDOW_TYPE_DND;
            break;
        case WXWayland::NET_WM_WINDOW_TYPE_DROPDOWN_MENU:
            types |= WXWaylandSurface::NET_WM_WINDOW_TYPE_DROPDOWN_MENU;
            break;
        case WXWayland::NET_WM_WINDOW_TYPE_POPUP_MENU:
            types |= WXWaylandSurface::NET_WM_WINDOW_TYPE_POPUP_MENU;
            break;
        case WXWayland::NET_WM_WINDOW_TYPE_COMBO:
            types |= WXWaylandSurface::NET_WM_WINDOW_TYPE_COMBO;
            break;
        case WXWayland::NET_WM_WINDOW_TYPE_MENU:
            types |= WXWaylandSurface::NET_WM_WINDOW_TYPE_MENU;
            break;
        case WXWayland::NET_WM_WINDOW_TYPE_NOTIFICATION:
            types |= WXWaylandSurface::NET_WM_WINDOW_TYPE_NOTIFICATION;
            break;
        case WXWayland::NET_WM_WINDOW_TYPE_SPLASH:
            types |= WXWaylandSurface::NET_WM_WINDOW_TYPE_SPLASH;
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

WXWaylandSurface::WXWaylandSurface(qw_xwayland_surface *handle, WXWayland *xwayland, QObject *parent)
    : WToplevelSurface(*new WXWaylandSurfacePrivate(this, handle, xwayland), parent)
{
    d_func()->init();
}

WXWaylandSurface::~WXWaylandSurface()
{

}

WXWaylandSurface *WXWaylandSurface::fromHandle(qw_xwayland_surface *handle)
{
    return handle->get_data<WXWaylandSurface>();
}

WXWaylandSurface *WXWaylandSurface::fromHandle(wlr_xwayland_surface *handle)
{
    if (auto surface = qw_xwayland_surface::get(handle))
        return fromHandle(surface);
    return nullptr;
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

qw_xwayland_surface *WXWaylandSurface::handle() const
{
    W_DC(WXWaylandSurface);

    return d->handle();
}

WXWaylandSurface *WXWaylandSurface::parentXWaylandSurface() const
{
    W_DC(WXWaylandSurface);

    return d->parent;
}

const QList<WXWaylandSurface*> &WXWaylandSurface::children() const
{
    W_DC(WXWaylandSurface);

    return d->children;
}

bool WXWaylandSurface::isToplevel() const
{
    W_DC(WXWaylandSurface);
    return !d->nativeHandle()->parent;
}

bool WXWaylandSurface::hasChild() const
{
    W_DC(WXWaylandSurface);
    return wl_list_empty(&d->nativeHandle()->children) == 0;
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

bool WXWaylandSurface::doesNotAcceptFocus() const
{
    W_DC(WXWaylandSurface);

    return !wlr_xwayland_or_surface_wants_focus(d->nativeHandle())
           || wlr_xwayland_icccm_input_model(d->nativeHandle()) == WLR_ICCCM_INPUT_MODEL_NONE;
}

QSize WXWaylandSurface::minSize() const
{
    W_DC(WXWaylandSurface);

    if (!d->nativeHandle()->size_hints)
        return QSize();

    if (!(d->nativeHandle()->size_hints->flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE))
        return QSize();

    return QSize(d->nativeHandle()->size_hints->min_width,
                 d->nativeHandle()->size_hints->min_height);
}

QSize WXWaylandSurface::maxSize() const
{
    W_DC(WXWaylandSurface);

    if (!d->nativeHandle()->size_hints)
        return QSize();

    if (!(d->nativeHandle()->size_hints->flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE))
        return QSize();

    return QSize(d->nativeHandle()->size_hints->max_width,
                 d->nativeHandle()->size_hints->max_height);
}

QRect WXWaylandSurface::geometry() const
{
    W_DC(WXWaylandSurface);

    QRect geometry = getContentGeometry();
    geometry.moveTopLeft(QPoint(d->nativeHandle()->x, d->nativeHandle()->y));

    return geometry;
}

QRect WXWaylandSurface::getContentGeometry() const
{
    W_DC(WXWaylandSurface);

    return QRect(0, 0, d->nativeHandle()->width, d->nativeHandle()->height);
}

QString WXWaylandSurface::title() const
{
    W_DC(WXWaylandSurface);

    return QString::fromUtf8(d->nativeHandle()->title);
}

QString WXWaylandSurface::appId() const
{
    W_DC(WXWaylandSurface);
    // https://www.x.org/releases/X11R7.7/doc/xproto/x11protocol.html#requests:InternAtom
    // The string should use the ISO Latin-1 encoding.
    return QString::fromLatin1(d->nativeHandle()->instance);
}

pid_t WXWaylandSurface::pid() const
{
    W_DC(WXWaylandSurface);

    return d->nativeHandle()->pid;
}

QRect WXWaylandSurface::requestConfigureGeometry() const
{
    W_DC(WXWaylandSurface);

    QRect rect = d->lastRequestConfigureGeometry;
    if (!d->lastRequestConfigureFlags.testFlag(XCB_CONFIG_WINDOW_X))
        rect.moveLeft(d->nativeHandle()->x);
    if (!d->lastRequestConfigureFlags.testFlag(XCB_CONFIG_WINDOW_Y))
        rect.moveTop(d->nativeHandle()->y);
    if (!d->lastRequestConfigureFlags.testFlag(XCB_CONFIG_WINDOW_WIDTH))
        rect.setWidth(d->nativeHandle()->width);
    if (!d->lastRequestConfigureFlags.testFlag(XCB_CONFIG_WINDOW_HEIGHT))
        rect.setHeight(d->nativeHandle()->height);

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
    return d->nativeHandle()->override_redirect;
}

WXWaylandSurface::WindowTypes WXWaylandSurface::windowTypes() const
{
    W_DC(WXWaylandSurface);
    return d->windowTypes;
}

WXWaylandSurface::DecorationsType WXWaylandSurface::decorationsType() const
{
    W_DC(WXWaylandSurface);
    return static_cast<DecorationsType>(d->nativeHandle()->decorations);
}

bool WXWaylandSurface::checkNewSize(const QSize &size)
{
    const QSize minSize = this->minSize();
    const QSize maxSize = this->maxSize();

    if (minSize.isValid()) {
        if (size.width() < minSize.width())
            return false;
        if (size.height() < minSize.height())
            return false;
    }

    if (maxSize.isValid()) {
        if (size.width() > maxSize.width())
            return false;
        if (size.height() > maxSize.height())
            return false;
    }

    return true;
}

void WXWaylandSurface::resize(const QSize &size)
{
    W_DC(WXWaylandSurface);
    handle()->configure(d->nativeHandle()->x, d->nativeHandle()->y, size.width(), size.height());
}

void WXWaylandSurface::configure(const QRect &geometry)
{
    handle()->configure(geometry.x(), geometry.y(), geometry.width(), geometry.height());
}

void WXWaylandSurface::setMaximize(bool on)
{
    W_D(WXWaylandSurface);

    if (d->maximized == on && d->isMaximized() == on)
        return;

    d->maximized = on;
    handle()->set_maximized(on);
    Q_EMIT maximizeChanged();
}

void WXWaylandSurface::setMinimize(bool on)
{
    W_D(WXWaylandSurface);

    if (d->minimized == on && d->nativeHandle()->minimized == on)
        return;

    d->minimized = on;
    handle()->set_minimized(on);
    Q_EMIT minimizeChanged();
}

void WXWaylandSurface::setFullScreen(bool on)
{
    W_D(WXWaylandSurface);

    if (d->fullscreen == on && d->nativeHandle()->fullscreen == on)
        return;

    d->fullscreen = on;
    handle()->set_fullscreen(on);
    Q_EMIT fullscreenChanged();
}

void WXWaylandSurface::setActivate(bool on)
{
    W_D(WXWaylandSurface);

    if (d->activated == on)
        return;

    d->activated = on;
    handle()->activate(on);
    Q_EMIT activateChanged();
}

void WXWaylandSurface::close()
{
    handle()->close();
}

void WXWaylandSurface::restack(WXWaylandSurface *sibling, StackMode mode)
{
    handle()->restack(*sibling->handle(), static_cast<xcb_stack_mode_t>(mode));
}

WAYLIB_SERVER_END_NAMESPACE
