// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxwaylandsurface.h"

#include "private/wtoplevelsurface_p.h"
#include "wsurface.h"
#include "wtools.h"
#include "wxwayland.h"

#include <sys/syscall.h>

#include <climits>

#include <qwcompositor.h>
#include <qwxwaylandsurface.h>

#include <QMetaObject>
#include <QPointer>
#include <QRect>

#include <unistd.h>

#include <xcb/xcb_icccm.h>

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

    ~WXWaylandSurfacePrivate() {
        if (pidFD >= 0)
            close(pidFD);
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
    void updateSizeHints();
    void updateWindowTypes();

    W_DECLARE_PUBLIC(WXWaylandSurface)

    WSurface *surface = nullptr;
    WXWayland *xwayland = nullptr;
    mutable int pidFD = -1;

    QList<WXWaylandSurface*> children;
    QPointer<WXWaylandSurface> parent;
    QMetaObject::Connection parentSurfaceChangedConnection;
    QRect lastRequestConfigureGeometry;
    WXWaylandSurface::ConfigureFlags lastRequestConfigureFlags = {0};
    WXWaylandSurface::WindowTypes windowTypes = {0};
    QSize minimumSize;
    QSize maximumSize = QSize(INT_MAX, INT_MAX);
    uint maximized:1;
    uint minimized:1;
    uint fullscreen:1;
    uint activated:1;
};

void WXWaylandSurfacePrivate::instantRelease()
{
    W_Q(WXWaylandSurface);
    handle()->set_data(nullptr, nullptr);
    handle()->disconnect(q);
    QObject::disconnect(parentSurfaceChangedConnection);
    parentSurfaceChangedConnection = {};

    if (!surface)
        return;
    surface->safeDeleteLater();
    surface = nullptr;
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
        Q_EMIT q->associated();
    });
    QObject::connect(handle(), &qw_xwayland_surface::notify_dissociate, q, [this, q] {
        Q_ASSERT(surface);
        Q_EMIT q->aboutToDissociate();
        surface->safeDeleteLater();
        surface = nullptr;
        Q_EMIT q->surfaceChanged();
    });
    QObject::connect(handle(), &qw_xwayland_surface::notify_set_parent, q, [this] {
        updateParent();
    });
    QObject::connect(handle(), &qw_xwayland_surface::notify_request_activate, q, &WXWaylandSurface::requestActivate);
    QObject::connect(handle(), &qw_xwayland_surface::notify_focus_in, q, [q] {
        Q_EMIT q->focusIn();
    });
    QObject::connect(handle(), &qw_xwayland_surface::notify_grab_focus, q, [q] {
        Q_EMIT q->grabFocus();
    });
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
                     q, [q] (wlr_xwayland_minimize_event *event) {
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
    QObject::connect(handle(), &qw_xwayland_surface::notify_set_hints, q, [this] {
        updateSizeHints();
    });
    QObject::connect(handle(), &qw_xwayland_surface::notify_set_window_type,
                     q, [this] {
                         updateWindowTypes();
                     });
    QObject::connect(handle(), &qw_xwayland_surface::notify_set_decorations,
                     q, &WXWaylandSurface::decorationsFlagsChanged);
    QObject::connect(handle(), &qw_xwayland_surface::notify_set_title,
                     q, &WXWaylandSurface::titleChanged);
    QObject::connect(handle(), &qw_xwayland_surface::notify_set_class,
                     q, &WXWaylandSurface::appIdChanged);
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

    if (nativeHandle()->size_hints) {
        if (nativeHandle()->size_hints->flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE) {
            minimumSize = QSize(nativeHandle()->size_hints->min_width,
                                nativeHandle()->size_hints->min_height);
        }
        if (nativeHandle()->size_hints->flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE) {
            maximumSize = QSize(nativeHandle()->size_hints->max_width,
                                nativeHandle()->size_hints->max_height);
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
    wl_list_for_each_safe(child, next, &nativeHandle()->children, parent_link) {
        list << WXWaylandSurface::fromHandle(qw_xwayland_surface::from(child));
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
    auto newParent = WXWaylandSurface::fromHandle(nativeHandle()->parent);
    if (parent == newParent)
        return;

    W_Q(WXWaylandSurface);

    const auto oldParentSurface = parent ? parent->surface() : nullptr;
    const bool hasParentChanged = (parent == nullptr) != (newParent == nullptr);
    // QPointer handles destroyed wrappers; isInvalidated() prevents accessing destroyed wlroots objects.
    if (parent && !parent->isInvalidated())
        parent->d_func()->updateChildren();
    QObject::disconnect(parentSurfaceChangedConnection);
    parentSurfaceChangedConnection = {};
    parent = newParent;
    if (parent) {
        parent->d_func()->updateChildren();
        parentSurfaceChangedConnection = QObject::connect(parent,
                                                          &WToplevelSurface::surfaceChanged,
                                                          q,
                                                          &WToplevelSurface::parentSurfaceChanged);
    }

    const auto newParentSurface = parent ? parent->surface() : nullptr;

    Q_EMIT q->parentXWaylandSurfaceChanged();
    if (oldParentSurface != newParentSurface)
        Q_EMIT q->parentSurfaceChanged();

    if (hasParentChanged)
        Q_EMIT q->isToplevelChanged();
}

void WXWaylandSurfacePrivate::updateWindowTypes()
{
    WXWaylandSurface::WindowTypes types = {0};

    for (size_t i = 0; i < nativeHandle()->window_type_len; ++i) {
        auto atomType = xwayland->atomType(nativeHandle()->window_type[i]);
        
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

WSurface *WXWaylandSurface::parentSurface() const
{
    auto parent = parentXWaylandSurface();
    return parent ? parent->surface() : nullptr;
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

WXWayland *WXWaylandSurface::xwayland() const
{
    W_DC(WXWaylandSurface);

    return d->xwayland;
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
    case Focus:
        return wlr_xwayland_surface_override_redirect_wants_focus(d->nativeHandle())
            && wlr_xwayland_surface_icccm_input_model(d->nativeHandle()) != WLR_ICCCM_INPUT_MODEL_NONE;
    case Activate:
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

WXWaylandSurface::DecorationsFlags WXWaylandSurface::decorationsFlags() const
{
    W_DC(WXWaylandSurface);
    return WXWaylandSurface::DecorationsFlags::fromInt(d->nativeHandle()->decorations);
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
    handle()->set_maximized(on, on);
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
    if (sibling) {
        handle()->restack(*sibling->handle(), static_cast<xcb_stack_mode_t>(mode));
        return;
    }

    handle()->restack(nullptr, static_cast<xcb_stack_mode_t>(mode));
}

WAYLIB_SERVER_END_NAMESPACE
