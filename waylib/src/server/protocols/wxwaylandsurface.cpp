// Copyright (C) 2023-2026 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxwaylandsurface.h"

#include "private/wtoplevelsurface_p.h"
#include "wsurface.h"
#include "wtools.h"
#include "wxwayland.h"
#include "wayliblogging.h"

#include <sys/syscall.h>

#include <climits>

#include <qwcompositor.h>
#include <qwxwaylandsurface.h>

#include <QPointer>

#include <unistd.h>

#include <xcb/xcb_icccm.h>

#define XCOORD_MAX 32767

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

namespace {

WXWaylandSurface::InputModel toInputModel(wlr_xwayland_icccm_input_model model)
{
    switch (model) {
    case WLR_ICCCM_INPUT_MODEL_NONE:
        return WXWaylandSurface::InputModelNone;
    case WLR_ICCCM_INPUT_MODEL_PASSIVE:
        return WXWaylandSurface::InputModelPassive;
    case WLR_ICCCM_INPUT_MODEL_LOCAL:
        return WXWaylandSurface::InputModelLocal;
    case WLR_ICCCM_INPUT_MODEL_GLOBAL:
        return WXWaylandSurface::InputModelGlobal;
    }
    return WXWaylandSurface::InputModelNone;
}

const char *inputModelName(WXWaylandSurface::InputModel model)
{
    switch (model) {
    case WXWaylandSurface::InputModelNone:
        return "none";
    case WXWaylandSurface::InputModelPassive:
        return "passive";
    case WXWaylandSurface::InputModelLocal:
        return "local";
    case WXWaylandSurface::InputModelGlobal:
        return "global";
    }
    return "unknown";
}

uint32_t xwaylandWindowId(const wlr_xwayland_surface *surface)
{
    return surface ? surface->window_id : 0;
}

bool protocolsContain(const wlr_xwayland_surface *surface, xcb_atom_t atom)
{
    if (!surface || atom == XCB_ATOM_NONE)
        return false;

    for (size_t i = 0; i < surface->protocols_len; ++i) {
        if (surface->protocols[i] == atom)
            return true;
    }

    return false;
}

int validChildCount(const QList<QPointer<WXWaylandSurface>> &children)
{
    int count = 0;
    for (const auto &child : children) {
        auto *raw = child.data();
        if (raw && !raw->isInvalidated())
            ++count;
    }
    return count;
}

bool childListEquals(const QList<QPointer<WXWaylandSurface>> &a,
                     const QList<QPointer<WXWaylandSurface>> &b)
{
    if (a.size() != b.size())
        return false;

    for (qsizetype i = 0; i < a.size(); ++i) {
        if (a.at(i).data() != b.at(i).data())
            return false;
    }

    return true;
}

int removeChildFromCache(QList<QPointer<WXWaylandSurface>> &children, WXWaylandSurface *surface)
{
    int removed = 0;
    for (auto it = children.begin(); it != children.end();) {
        auto *child = it->data();
        if (!child || child == surface) {
            if (child == surface)
                ++removed;
            it = children.erase(it);
            continue;
        }

        ++it;
    }

    return removed;
}

} // namespace

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

    QList<QPointer<WXWaylandSurface>> children;
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
    uint treeRelationsCleaned:1 = false;
};

void WXWaylandSurfacePrivate::instantRelease()
{
    W_Q(WXWaylandSurface);
    q->cleanupTreeRelationsBeforeDestroy();
    handle()->set_data(nullptr, nullptr);
    handle()->disconnect(q);

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
    QObject::connect(handle(), &qw_xwayland_surface::notify_request_configure,
                     q, [this, q] (wlr_xwayland_surface_configure_event *event) {
        lastRequestConfigureGeometry = QRect(event->x, event->y, event->width, event->height);
        lastRequestConfigureFlags = WXWaylandSurface::ConfigureFlags(event->mask);
        qCDebug(lcWlXWayland) << "[XWL_REQUEST_CONFIGURE] XWayland request configure:"
                               << "window_id=" << xwaylandWindowId(nativeHandle())
                               << "mapped=" << (surface && surface->mapped())
                               << "request_flags=" << static_cast<int>(lastRequestConfigureFlags)
                               << "request_geometry=" << lastRequestConfigureGeometry
                               << "native_geometry=" << q->geometry();

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
                     q, [this, q] {
        const QRect actualGeometry = q->geometry();
        const QRect oldRequestGeometry = lastRequestConfigureGeometry;
        const auto oldRequestFlags = lastRequestConfigureFlags;
        const bool hadPendingRequest = oldRequestFlags != WXWaylandSurface::ConfigureFlags();

        lastRequestConfigureGeometry = actualGeometry;
        lastRequestConfigureFlags = WXWaylandSurface::ConfigureFlags();

        if (hadPendingRequest || oldRequestGeometry != actualGeometry) {
            qCDebug(lcWlXWayland) << "[XWL_SET_GEOMETRY_SYNC] XWayland geometry confirmed:"
                                   << "window_id=" << xwaylandWindowId(nativeHandle())
                                   << "old_request_flags=" << static_cast<int>(oldRequestFlags)
                                   << "old_request_geometry=" << oldRequestGeometry
                                   << "actual_geometry=" << actualGeometry
                                   << "request_flags_cleared=" << hadPendingRequest;
        }

        Q_EMIT q->geometryChanged();
    });
    QObject::connect(handle(), &qw_xwayland_surface::notify_set_hints, q, [this, q] {
        updateSizeHints();
        Q_EMIT q->inputModelChanged();
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
    QObject::connect(handle(), &qw_xwayland_surface::notify_focus_in, q, [this, q] {
        qCDebug(lcWlXWayland) << "[XWL_FOCUS_IN] XWayland focus accepted:"
                               << "window_id=" << xwaylandWindowId(nativeHandle())
                               << "surface=" << q_func();
        Q_EMIT q->focusIn();
    });
    QObject::connect(handle(), &qw_xwayland_surface::notify_grab_focus, q, [this, q] {
        qCDebug(lcWlXWayland) << "[XWL_GRAB_FOCUS] XWayland keyboard grab focus:"
                               << "window_id=" << xwaylandWindowId(nativeHandle())
                               << "surface=" << q_func();
        Q_EMIT q->grabFocus();
    });
    QObject::connect(handle(), &qw_xwayland_surface::notify_pointer_grab_focus, q, [this, q] {
        qCDebug(lcWlXWayland) << "[XWL_POINTER_GRAB_FOCUS] XWayland pointer grab focus:"
                               << "window_id=" << xwaylandWindowId(nativeHandle())
                               << "surface=" << q_func();
        Q_EMIT q->pointerGrabFocus();
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
    QList<QPointer<WXWaylandSurface>> list;

    struct wlr_xwayland_surface *child, *next;
    wl_list_for_each_safe(child, next, &nativeHandle()->children, parent_link) {
        if (auto *surface = WXWaylandSurface::fromHandle(qw_xwayland_surface::from(child)))
            list << surface;
    }

    if (childListEquals(children, list))
        return;

    const bool hasChildChanged = (validChildCount(children) == 0) != (validChildCount(list) == 0);
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
    cleanupTreeRelationsBeforeDestroy();
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

WXWayland *WXWaylandSurface::xwayland() const
{
    W_DC(WXWaylandSurface);

    return d->xwayland;
}

QList<WXWaylandSurface*> WXWaylandSurface::children() const
{
    W_DC(WXWaylandSurface);

    QList<WXWaylandSurface *> list;
    list.reserve(d->children.size());
    for (const auto &child : d->children) {
        auto *raw = child.data();
        if (raw && !raw->isInvalidated())
            list << raw;
    }

    return list;
}

bool WXWaylandSurface::isToplevel() const
{
    W_DC(WXWaylandSurface);
    return !d->nativeHandle()->parent;
}

bool WXWaylandSurface::hasChild() const
{
    return !children().isEmpty();
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
    {
        const auto model = inputModel();
        const bool acceptsInput = model != InputModelNone;
        const bool overrideRedirect = isBypassManager();
        const bool overrideRedirectWantsFocus = overrideRedirect ? d->handle()->wants_focus() : true;
        const bool hasFocus = acceptsInput && (!overrideRedirect || overrideRedirectWantsFocus);
        qCDebug(lcWlXWayland) << "[XWL_FOCUS_CAP] XWayland focus capability:"
                               << "window_id=" << xwaylandWindowId(d->nativeHandle())
                               << "surface=" << this
                               << "override_redirect=" << overrideRedirect
                               << "window_types=" << d->windowTypes
                               << "input_model=" << inputModelName(model)
                               << "override_redirect_wants_focus=" << overrideRedirectWantsFocus
                               << "has_focus_capability=" << hasFocus;
        return hasFocus;
    }
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

WXWaylandSurface::InputModel WXWaylandSurface::inputModel() const
{
    W_DC(WXWaylandSurface);
    return toInputModel(d->handle()->icccm_input_model());
}

bool WXWaylandSurface::supportsWmTakeFocus() const
{
    W_DC(WXWaylandSurface);

    if (!d->xwayland)
        return false;

    return protocolsContain(d->nativeHandle(), d->xwayland->atom("WM_TAKE_FOCUS"));
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

void WXWaylandSurface::forceActivate()
{
    W_D(WXWaylandSurface);

    const bool wasActivated = d->activated;
    d->activated = true;
    qCDebug(lcWlXWayland) << "[XWL_FORCE_ACTIVATE] Force XWayland native activation:"
                           << "window_id=" << xwaylandWindowId(d->nativeHandle())
                           << "surface=" << this
                           << "input_model=" << inputModelName(inputModel())
                           << "was_activated=" << wasActivated;
    handle()->activate(true);
    if (!wasActivated)
        Q_EMIT activateChanged();
}

void WXWaylandSurface::requestNativeFocus()
{
    W_D(WXWaylandSurface);

    qCDebug(lcWlXWayland) << "[XWL_NATIVE_FOCUS] Request XWayland native focus:"
                           << "window_id=" << xwaylandWindowId(d->nativeHandle())
                           << "surface=" << this
                           << "input_model=" << inputModelName(inputModel())
                           << "activated_state=" << d->activated;
    handle()->activate(true);
}

void WXWaylandSurface::forceNativeFocus()
{
    W_D(WXWaylandSurface);

    qCDebug(lcWlXWayland) << "[XWL_NATIVE_FOCUS_FORCE] Force XWayland native focus:"
                           << "window_id=" << xwaylandWindowId(d->nativeHandle())
                           << "surface=" << this
                           << "input_model=" << inputModelName(inputModel())
                           << "activated_state=" << d->activated;
    handle()->force_focus();
}

bool WXWaylandSurface::offerFocus()
{
    W_D(WXWaylandSurface);

    const auto inputModel = this->inputModel();
    const bool overrideRedirect = isBypassManager();
    const bool supportsTakeFocus = supportsWmTakeFocus();
    qCDebug(lcWlXWayland) << "[XWL_OFFER_FOCUS] XWayland offer focus:"
                           << "window_id=" << xwaylandWindowId(d->nativeHandle())
                           << "surface=" << this
                           << "input_model=" << inputModelName(inputModel)
                           << "override_redirect=" << overrideRedirect
                           << "supports_wm_take_focus=" << supportsTakeFocus
                           << "protocols_len=" << d->nativeHandle()->protocols_len;

    if (overrideRedirect || !supportsTakeFocus)
        return false;

    handle()->offer_focus();
    return true;
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

void WXWaylandSurface::cleanupTreeRelationsBeforeDestroy()
{
    W_D(WXWaylandSurface);

    if (d->treeRelationsCleaned)
        return;
    d->treeRelationsCleaned = true;

    const uint32_t windowId = !isInvalidated() ? xwaylandWindowId(d->nativeHandle()) : 0;
    auto *oldParent = d->parent.data();
    const int oldChildCount = validChildCount(d->children);

    if (oldParent) {
        auto *parentPrivate = oldParent->d_func();
        const int parentChildCountBefore = validChildCount(parentPrivate->children);
        const int removed = removeChildFromCache(parentPrivate->children, this);
        const int parentChildCountAfter = validChildCount(parentPrivate->children);

        if (removed > 0) {
            Q_EMIT oldParent->childrenChanged();
            if ((parentChildCountBefore == 0) != (parentChildCountAfter == 0))
                Q_EMIT oldParent->hasChildChanged();
        }
    }

    // This object is being destroyed. Keep the cached tree relation safe, but
    // do not report a real reparent-to-toplevel event to consumers.
    d->parent = nullptr;

    const auto children = d->children;
    d->children.clear();
    for (const auto &childPointer : children) {
        auto *child = childPointer.data();
        if (!child)
            continue;

        auto *childPrivate = child->d_func();
        if (childPrivate->parent != this)
            continue;

        childPrivate->parent = nullptr;
        Q_EMIT child->parentXWaylandSurfaceChanged();
        Q_EMIT child->isToplevelChanged();
    }

    if (oldChildCount > 0) {
        Q_EMIT childrenChanged();
        Q_EMIT hasChildChanged();
    }

    qCDebug(lcWlXWayland) << "[XWL_TREE_CLEANUP] Cleaned XWayland surface tree cache before destroy:"
                           << "window_id=" << windowId
                           << "surface=" << this
                           << "oldParent=" << oldParent
                           << "old_child_count=" << oldChildCount;
}

WAYLIB_SERVER_END_NAMESPACE
