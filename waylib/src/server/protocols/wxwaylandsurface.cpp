// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxwaylandsurface.h"

#include "private/wtoplevelsurface_p.h"
#include "wsurface.h"
#include "wtools.h"
#include "wxwayland.h"

#include <sys/syscall.h>

#include <climits>

extern "C" {
#include <wlr/types/wlr_compositor.h>
#define class _class
#include <wlr/xwayland/xwayland.h>
#undef class
}

#include <QPointer>
#include <QHash>

#include <memory>
#include <vector>
#include <unistd.h>

#include <xcb/xcb_icccm.h>

#define XCOORD_MAX 32767

WAYLIB_SERVER_BEGIN_NAMESPACE

using XWaylandSurfaceRegistry = QHash<const wlr_xwayland_surface *, WXWaylandSurface *>;
Q_GLOBAL_STATIC(XWaylandSurfaceRegistry, s_xwaylandSurfaces)

class Q_DECL_HIDDEN WXWaylandSurfacePrivate : public WToplevelSurfacePrivate
{
public:
    WXWaylandSurfacePrivate(WXWaylandSurface *qq, wlr_xwayland_surface *handle, WXWayland *xwayland)
        : WToplevelSurfacePrivate(qq)
        , surfaceHandle(handle)
        , xwayland(xwayland)
        , maximized(false)
        , minimized(false)
        , fullscreen(false)
        , activated(false)
    {
        Q_ASSERT(surfaceHandle);
        Q_ASSERT(!s_xwaylandSurfaces->contains(surfaceHandle));
        s_xwaylandSurfaces->insert(surfaceHandle, qq);
    }

    ~WXWaylandSurfacePrivate() {
        if (pidFD >= 0)
            close(pidFD);
    }

    inline wlr_xwayland_surface *nativeHandle() const { return surfaceHandle; }

    inline bool isMaximized() const {
        return nativeHandle()->maximized_horz && nativeHandle()->maximized_vert;
    }

    wl_client *waylandClient() const override {
        return surface ? wl_resource_get_client(surface->handle()->resource) : nullptr;
    }

    void instantRelease() override;

    void init();
    void updateChildren();
    void updateParent();
    void updateSizeHints();
    void updateWindowTypes();

    template<typename Callback>
    void listen(wl_signal *signal, Callback &&callback)
    {
        auto listener = std::make_unique<WNativeListener>();
        listener->connect(signal, std::forward<Callback>(callback));
        nativeListeners.push_back(std::move(listener));
    }

    W_DECLARE_PUBLIC(WXWaylandSurface)

    wlr_xwayland_surface *surfaceHandle = nullptr;
    std::vector<std::unique_ptr<WNativeListener>> nativeListeners;
    WSurface *surface = nullptr;
    WXWayland *xwayland = nullptr;
    mutable int pidFD = -1;

    QList<WXWaylandSurface*> children;
    QPointer<WXWaylandSurface> parent;
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
    nativeListeners.clear();
    if (surfaceHandle) {
        s_xwaylandSurfaces->remove(surfaceHandle);
        surfaceHandle = nullptr;
    }

    if (!surface)
        return;
    surface->safeDeleteLater();
    surface = nullptr;
}

void WXWaylandSurfacePrivate::init()
{
    W_Q(WXWaylandSurface);
    listen(&nativeHandle()->events.associate, [this, q](void *) {
        Q_ASSERT(!WSurface::fromHandle(nativeHandle()->surface));
        surface = new WSurface(nativeHandle()->surface, q);
        surface->setAttachedData<WXWaylandSurface>(q);
        Q_EMIT q->surfaceChanged();
        Q_EMIT q->associated();
    });
    listen(&nativeHandle()->events.dissociate, [this, q](void *) {
        Q_ASSERT(surface);
        Q_EMIT q->aboutToDissociate();
        surface->safeDeleteLater();
        surface = nullptr;
        Q_EMIT q->surfaceChanged();
    });
    listen(&nativeHandle()->events.set_parent, [this](void *) {
        updateParent();
    });
    listen(&nativeHandle()->events.request_activate, [q](void *) {
        Q_EMIT q->requestActivate();
    });
    listen(&nativeHandle()->events.request_configure, [this, q](void *data) {
        auto *event = static_cast<wlr_xwayland_surface_configure_event *>(data);
        lastRequestConfigureGeometry = QRect(event->x, event->y, event->width, event->height);
        lastRequestConfigureFlags = WXWaylandSurface::ConfigureFlags(event->mask);

        if (!surface || !surface->mapped()) {
            q->configure(lastRequestConfigureGeometry);
        } else {
            Q_EMIT q->requestConfigure(lastRequestConfigureGeometry, lastRequestConfigureFlags);
        }
    });
    listen(&nativeHandle()->events.request_fullscreen, [this, q](void *) {
        if (nativeHandle()->fullscreen) {
            Q_EMIT q->requestFullscreen();
        } else {
            Q_EMIT q->requestCancelFullscreen();
        }
    });
    listen(&nativeHandle()->events.request_maximize, [this, q](void *) {
        if (nativeHandle()->maximized_horz && nativeHandle()->maximized_vert) {
            Q_EMIT q->requestMaximize();
        } else {
            Q_EMIT q->requestCancelMaximize();
        }
    });
    listen(&nativeHandle()->events.request_minimize, [q](void *data) {
        auto *event = static_cast<wlr_xwayland_minimize_event *>(data);
        if (event->minimize) {
            Q_EMIT q->requestMinimize();
        } else {
            Q_EMIT q->requestCancelMinimize();
        }
    });
    listen(&nativeHandle()->events.request_move, [this, q](void *) {
        Q_EMIT q->requestMove(xwayland->seat(), 0);
    });
    listen(&nativeHandle()->events.request_resize, [this, q](void *data) {
        auto *event = static_cast<wlr_xwayland_resize_event *>(data);
        Q_EMIT q->requestResize(xwayland->seat(), WTools::toQtEdge(event->edges), 0);
    });
    listen(&nativeHandle()->events.set_override_redirect, [q](void *) {
        Q_EMIT q->bypassManagerChanged();
    });
    listen(&nativeHandle()->events.set_geometry, [q](void *) {
        Q_EMIT q->geometryChanged();
    });
    listen(&nativeHandle()->events.set_hints, [this](void *) {
        updateSizeHints();
    });
    listen(&nativeHandle()->events.set_window_type, [this](void *) {
        updateWindowTypes();
    });
    listen(&nativeHandle()->events.set_decorations, [q](void *) {
        Q_EMIT q->decorationsFlagsChanged();
    });
    listen(&nativeHandle()->events.set_title, [q](void *) {
        Q_EMIT q->titleChanged();
    });
    listen(&nativeHandle()->events.set_class, [q](void *) {
        Q_EMIT q->appIdChanged();
    });
    listen(&nativeHandle()->events.destroy, [q](void *) {
        q->safeDeleteLater();
    });
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
    auto newParent = WXWaylandSurface::fromHandle(nativeHandle()->parent);
    if (parent == newParent)
        return;

    const bool hasParentChanged = (parent == nullptr) != (newParent == nullptr);
    // QPointer handles destroyed wrappers; isInvalidated() prevents accessing destroyed wlroots objects.
    if (parent && !parent->isInvalidated())
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

WXWaylandSurface::WXWaylandSurface(wlr_xwayland_surface *handle, WXWayland *xwayland, QObject *parent)
    : WToplevelSurface(*new WXWaylandSurfacePrivate(this, handle, xwayland), parent)
{
    d_func()->init();
}

WXWaylandSurface::~WXWaylandSurface()
{

}

WXWaylandSurface *WXWaylandSurface::fromHandle(wlr_xwayland_surface *handle)
{
    return s_xwaylandSurfaces->value(handle);
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
    return d_func()->nativeHandle();
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
    // TODO(xwayland): To implement ICCCM focus model, refer to
    // https://github.com/labwc/labwc/blob/9ba3303246b1b1bf4dbf13793faa62ec74872e3b/src/xwayland.c#L108
    case Activate: [[fallthrough]];
    case Focus:
        return !isBypassManager()
            && wlr_xwayland_surface_override_redirect_wants_focus(d->nativeHandle())
            && wlr_xwayland_surface_icccm_input_model(d->nativeHandle()) != WLR_ICCCM_INPUT_MODEL_NONE;
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
    wlr_xwayland_surface_configure(
        handle(), d->nativeHandle()->x, d->nativeHandle()->y, size.width(), size.height());
}

void WXWaylandSurface::configure(const QRect &geometry)
{
    wlr_xwayland_surface_configure(
        handle(), geometry.x(), geometry.y(), geometry.width(), geometry.height());
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

    if (d->minimized == on && d->nativeHandle()->minimized == on)
        return;

    d->minimized = on;
    wlr_xwayland_surface_set_minimized(handle(), on);
    Q_EMIT minimizeChanged();
}

void WXWaylandSurface::setFullScreen(bool on)
{
    W_D(WXWaylandSurface);

    if (d->fullscreen == on && d->nativeHandle()->fullscreen == on)
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
        wlr_xwayland_surface_restack(
            handle(), sibling->handle(), static_cast<xcb_stack_mode_t>(mode));
        return;
    }

    wlr_xwayland_surface_restack(handle(), nullptr, static_cast<xcb_stack_mode_t>(mode));
}

WAYLIB_SERVER_END_NAMESPACE
