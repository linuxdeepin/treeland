// Copyright (C) 2023-2026 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxwayland.h"

#include "private/wglobal_p.h"
#include "wseat.h"
#include "wsocket.h"
#include "wayliblogging.h"
#include "wxwaylandsurface.h"

#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include <qwcompositor.h>
#include <qwdisplay.h>
#include <qwseat.h>
#include <qwxwayland.h>
#include <qwxwaylandserver.h>
#include <qwxwaylandshellv1.h>
#include <qwxwaylandsurface.h>

#include <QCoreApplication>
#include <QTimer>

#include <utility>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WXWaylandPrivate : public WWrapObjectPrivate
{
public:
    WXWaylandPrivate(WXWayland *qq, qw_compositor *compositor, bool lazy)
        : WWrapObjectPrivate(qq)
        , compositor(compositor)
        , lazy(lazy)
    {

    }
    ~WXWaylandPrivate() {

    }

    void init();

    wl_client *waylandClient() const override {
        return q_func()->handle()->handle()->server->client;
    }

    // begin slot function
    void on_new_surface(wlr_xwayland_surface *xwl_surface);
    void on_surface_destroy(qw_xwayland_surface *xwl_surface);
    // end slot function

    // Async property reading
    struct PerWindowProps
    {
        QVector<WXWayland::AsyncPropRequest> requests;
        QMap<xcb_atom_t, QByteArray> results;
        QVector<xcb_get_property_cookie_t> cookies;
        std::function<void(xcb_window_t, const QMap<xcb_atom_t, QByteArray> &)> callback;
        QTimer *timer = nullptr;
        bool propNotifySeen = false;
    };

    QMap<xcb_window_t, PerWindowProps> asyncProps;

    void xcbPollReplies();
    void xcbAsyncTimeoutForWindow(xcb_window_t windowId);

    W_DECLARE_PUBLIC(WXWayland)

    xcb_screen_t *screen = nullptr;

    qw_compositor *compositor;
    bool lazy = true;
    QVector<WXWaylandSurface*> surfaceList;
    QVector<xcb_atom_t> atoms;
    QList<WXWaylandSurface*> toplevelSurfaces;

    WSocket *socket = nullptr;

protected:
    void instantRelease() override;
};

bool xwayland_user_event_handler(wlr_xwayland *xwayland, xcb_generic_event_t *event)
{
    if (!event)
        return false;

    const uint8_t response_type = event->response_type & ~0x80;
    if (response_type != XCB_PROPERTY_NOTIFY)
        return false;

    auto *pe = reinterpret_cast<const xcb_property_notify_event_t *>(event);
    auto *self = WXWayland::fromHandle(xwayland);

    if (!self)
        return false;

    auto *d = self->d_func();

    // Trigger async property reading infrastructure if this window is being tracked.
    if (!d->asyncProps.isEmpty()) {
        d->xcbPollReplies();
        auto it = d->asyncProps.find(pe->window);
        if (it != d->asyncProps.end()) {
            auto &props = it.value();
            props.propNotifySeen = true;
            // Resend requests on PROPNOTIFY to get the latest value after the change.
            xcb_connection_t *conn = self->xcbConnection();
            props.cookies.clear();
            for (const auto &req : std::as_const(props.requests)) {
                auto cookie = xcb_get_property_unchecked(conn,
                                                         false,
                                                         pe->window,
                                                         req.atom,
                                                         req.type,
                                                         0,
                                                         1024);
                props.cookies.append(cookie);
            }
            xcb_flush(conn);
        }
    }

    const auto &list = self->surfaceList();
    for (auto *surface : list) {
        QPointer<WXWaylandSurface> sp(surface);
        if (!sp)
            continue;
        if (sp->handle()->handle()->window_id == pe->window) {
            Q_EMIT self->windowPropertyChanged(sp, pe->atom);
            break;
        }
    }

    // Do not consume the event; let xcb and wlroots continue normal processing.
    return false;
}

void WXWaylandPrivate::init()
{
    W_Q(WXWayland);

    auto screen_iterator = xcb_setup_roots_iterator(xcb_get_setup(q->xcbConnection()));
    screen = screen_iterator.data;

    xcb_intern_atom_cookie_t cookies[WXWayland::AtomCount];
    const auto atomEnum = QMetaEnum::fromType<WXWayland::XcbAtom>();
    for (int i = WXWayland::AtomNone + 1; i < WXWayland::AtomCount; ++i) {
        auto name = atomEnum.valueToKey(i);
        Q_ASSERT(name);
        cookies[i] = xcb_intern_atom(q->xcbConnection(), 0,
                                     strnlen(name, 50), name);
    }

    atoms.resize(WXWayland::AtomCount);
    for (int i = WXWayland::AtomNone + 1; i < WXWayland::AtomCount; ++i) {
        xcb_generic_error_t *error;
        xcb_intern_atom_reply_t *reply =
            xcb_intern_atom_reply(q->xcbConnection(), cookies[i], &error);
        if (reply && !error)
            atoms[i] = reply->atom;
        free(reply);

        if (error) {
            atoms[i] = XCB_ATOM_NONE;
            free(error);
            continue;
        }
    }
}

void WXWaylandPrivate::instantRelease() {
    delete handle<qw_xwayland>();
}

void WXWaylandPrivate::on_new_surface(wlr_xwayland_surface *xwl_surface)
{
    W_Q(WXWayland);

    auto server = q->server();
    qw_xwayland_surface *xwlSurface = qw_xwayland_surface::from(xwl_surface);
    auto surface = new WXWaylandSurface(xwlSurface, q, server);
    surface->setParent(server);
    Q_ASSERT(surface->parent() == server);
    surface->safeConnect(&qw_xwayland_surface::before_destroy,
                     q, [this, xwlSurface] {
        on_surface_destroy(xwlSurface);
    });

    surfaceList.append(surface);
    q->addSurface(surface);
}

void WXWaylandPrivate::on_surface_destroy(qw_xwayland_surface *xwl_surface)
{
    W_Q(WXWayland);

    auto surface = WXWaylandSurface::fromHandle(xwl_surface);
    Q_ASSERT(surface);
    surface->cleanupTreeRelationsBeforeDestroy();
    bool ok = surfaceList.removeOne(surface);
    Q_ASSERT(ok);
    q->removeSurface(surface);
    surface->safeDeleteLater();
}

WXWayland::WXWayland(qw_compositor *compositor, bool lazy)
    : WWrapObject(*new WXWaylandPrivate(this, compositor, lazy))
{
    W_D(WXWayland);
    // TODO: Add setFreezeClientWhenDisable in WSocket
    d->socket = new WSocket(false, this);
}

QByteArray WXWayland::displayName() const
{
    return isValid() ? QByteArray(std::as_const(handle()->handle()->display_name)) : QByteArray();
}

xcb_atom_t WXWayland::atom(XcbAtom type) const
{
    W_DC(WXWayland);
    return d->atoms.at(type);
}

xcb_atom_t WXWayland::atom(const QByteArray &name) const
{
    auto cookie = xcb_intern_atom(xcbConnection(), 0, name.size(), name.constData());
    xcb_generic_error_t *error;
    xcb_intern_atom_reply_t *reply =
        xcb_intern_atom_reply(xcbConnection(), cookie, &error);
    xcb_atom_t a = XCB_ATOM_NONE;
    if (reply && !error)
        a = reply->atom;
    free(reply);

    if (error)
        free(error);

    return a;
}

WXWayland::XcbAtom WXWayland::atomType(xcb_atom_t atom) const
{
    W_DC(WXWayland);
    for (int i = AtomNone; i < AtomCount; ++i) {
        if (d->atoms.at(i) == atom)
            return static_cast<XcbAtom>(i);
    }

    return AtomNone;
}

QVarLengthArray<xcb_atom_t> WXWayland::supportedAtoms() const
{
    auto xcb_conn = xcbConnection();
    auto root = xcbScreen()->root;

    auto cookie = xcb_get_property(xcb_conn, 0, root, atom(_NET_SUPPORTED), XCB_ATOM_ATOM, 0, 4096);
    auto reply = xcb_get_property_reply(xcb_conn, cookie, nullptr);
    if (!reply) {
        return {};
    }

    xcb_atom_t *atoms = reinterpret_cast<xcb_atom_t*>(xcb_get_property_value(reply));
    size_t atoms_len = reply->value_len;

    QVarLengthArray<xcb_atom_t> atomList;
    atomList.append(atoms, atoms_len);

    return atomList;
}

void WXWayland::setSupportedAtoms(const QVarLengthArray<xcb_atom_t> &atoms)
{
    auto xcb_conn = xcbConnection();
    auto root = xcbScreen()->root;

    xcb_change_property(xcb_conn, XCB_PROP_MODE_REPLACE, root, atom(_NET_SUPPORTED),
                        XCB_ATOM_ATOM, 32, atoms.size(), atoms.constData());
    xcb_flush(xcb_conn);
}

void WXWayland::setAtomSupported(xcb_atom_t atom, bool supported)
{
    auto xcb_conn = xcbConnection();
    auto root = xcbScreen()->root;

    if (supported) {
        xcb_change_property(xcb_conn, XCB_PROP_MODE_APPEND, root,
                            this->atom(_NET_SUPPORTED),
                            XCB_ATOM_ATOM, 32, 1, &atom);
        xcb_flush(xcb_conn);
    } else {
        auto atoms = supportedAtoms();
        atoms.removeOne(atom);
        setSupportedAtoms(atoms);
    }
}

void WXWayland::setSeat(WSeat *seat)
{
    if (auto handle = this->handle())
        handle->set_seat(*seat->handle());
}

WSeat *WXWayland::seat() const
{
    if (!handle())
        return nullptr;
    if (!handle()->handle()->seat)
        return nullptr;
    auto seat = qw_seat::from(handle()->handle()->seat);
    return WSeat::fromHandle(seat);
}

xcb_connection_t *WXWayland::xcbConnection() const
{
    return handle()->get_xwm_connection();
}

xcb_screen_t *WXWayland::xcbScreen() const
{
    W_DC(WXWayland);
    return d->screen;
}

WXWayland *WXWayland::fromHandle(wlr_xwayland *handle)
{
    auto *qw = QW_NAMESPACE::qw_xwayland::from(handle);
    if (!qw)
        return nullptr;
    return qw->get_data<WXWayland>();
}

QVector<WXWaylandSurface*> WXWayland::surfaceList() const
{
    W_DC(WXWayland);
    return d->surfaceList;
}

WSocket *WXWayland::ownsSocket() const
{
    W_DC(WXWayland);
    return d->socket->parentSocket();
}

void WXWayland::setOwnsSocket(WSocket *socket)
{
    W_D(WXWayland);
    d->socket->setParentSocket(socket);
}

QByteArrayView WXWayland::interfaceName() const
{
    return "xwayland_shell_v1";
}

void WXWayland::addSurface(WXWaylandSurface *surface)
{
    surface->safeConnect(&WXWaylandSurface::isToplevelChanged,
                        this, &WXWayland::onIsToplevelChanged);

    if (surface->isToplevel())
        addToplevel(surface);
    Q_EMIT surfaceAdded(surface);
}

void WXWayland::removeSurface(WXWaylandSurface *surface)
{
    removeToplevel(surface);
    Q_EMIT surfaceRemoved(surface);
}


void WXWayland::addToplevel(WXWaylandSurface *surface)
{
    W_D(WXWayland);
    if (d->toplevelSurfaces.contains(surface))
        return;
    d->toplevelSurfaces.append(surface);
    Q_EMIT toplevelAdded(surface);
}

void WXWayland::removeToplevel(WXWaylandSurface *surface)
{
    W_D(WXWayland);
    if (d->toplevelSurfaces.removeOne(surface))
        Q_EMIT toplevelRemoved(surface);
}

void WXWayland::onIsToplevelChanged()
{
    auto surface = qobject_cast<WXWaylandSurface*>(sender());
    Q_ASSERT(surface);

    if (!surface->surface())
        return;

    if (surface->isToplevel()) {
        addToplevel(surface);
    } else {
        removeToplevel(surface);
    }
}

void WXWayland::create(WServer *server)
{
    W_D(WXWayland);
    // free follow display

    auto handle = qw_xwayland::create(*server->handle(), *d->compositor, d->lazy);
    initHandle(handle);
    m_handle = handle;
    d->socket->bind(handle->handle()->server->x_fd[1]);

    handle->set_data(this, this);
    handle->handle()->user_event_handler = xwayland_user_event_handler;

    QObject::connect(handle, &qw_xwayland::notify_new_surface, this, [d] (wlr_xwayland_surface *surface) {
        d->on_new_surface(surface);
    });

    QObject::connect(handle, &qw_xwayland::notify_ready, this, [this, d] {
        d->init();
        Q_EMIT ready();
    });

    auto s = qw_xwayland_server::from(handle->handle()->server);
    QObject::connect(s, &qw_xwayland_server::notify_start, this, [d] {
        d->socket->addClient(d->waylandClient(), false);
    });
}

void WXWayland::destroy([[maybe_unused]] WServer *server)
{
    W_D(WXWayland);

    if (auto handle = this->handle()) {
        handle->set_data(nullptr, nullptr);
        handle->handle()->user_event_handler = nullptr;
    }

    auto list = d->surfaceList;
    d->surfaceList.clear();
    d->screen = nullptr;

    for (auto surface : std::as_const(list)) {
        // disconnect from on_surface_destroy
        disconnect(surface->handle(), &qw_xwayland_surface::before_destroy,
                   this, nullptr);
        surface->cleanupTreeRelationsBeforeDestroy();
        removeSurface(surface);
        surface->safeDeleteLater();
    }
}

wl_global *WXWayland::global() const
{
    return handle()->handle()->shell_v1->global;
}

void WXWayland::readAsyncProperties(
    xcb_window_t windowId,
    const QVector<AsyncPropRequest> &requests,
    int timeoutMs,
    std::function<void(xcb_window_t, const QMap<xcb_atom_t, QByteArray> &)> callback)
{
    W_D(WXWayland);

    xcb_connection_t *conn = xcbConnection();
    if (!conn || requests.isEmpty()) {
        callback(windowId, QMap<xcb_atom_t, QByteArray>{});
        return;
    }

    WXWaylandPrivate::PerWindowProps props;
    props.requests = requests;
    props.callback = std::move(callback);

    props.cookies.reserve(requests.size());
    for (const auto &req : std::as_const(requests)) {
        auto cookie =
            xcb_get_property_unchecked(conn, false, windowId, req.atom, req.type, 0, 1024);
        props.cookies.append(cookie);
    }
    xcb_flush(conn);

    // Create per-window timer.
    props.timer = new QTimer(this);
    props.timer->setSingleShot(true);
    connect(props.timer, &QTimer::timeout, this, [d, windowId]() {
        d->xcbAsyncTimeoutForWindow(windowId);
    });
    props.timer->start(timeoutMs);

    auto oldIt = d->asyncProps.find(windowId);
    if (oldIt != d->asyncProps.end()) {
        if (oldIt->timer) {
            oldIt->timer->stop();
            delete oldIt->timer;
        }
        qCDebug(lcWlXWayland) << "[XWL_ASYNC_PROPS_REPLACE] Replacing pending async properties:"
                               << "window_id=" << windowId
                               << "request_count=" << requests.size();
        d->asyncProps.erase(oldIt);
    }

    d->asyncProps.insert(windowId, std::move(props));
}

void WXWaylandPrivate::xcbPollReplies()
{
    W_Q(WXWayland);

    if (asyncProps.isEmpty())
        return;

    xcb_connection_t *conn = q->xcbConnection();

    QList<xcb_window_t> toRemove;

    for (auto it = asyncProps.begin(); it != asyncProps.end(); ++it) {
        xcb_window_t windowId = it.key();
        auto &props = it.value();

        bool windowDone = true;
        for (int i = 0; i < props.cookies.size(); ++i) {
            if (props.results.contains(props.requests[i].atom))
                continue; // already got this one

            void *replyPtr = nullptr;
            xcb_generic_error_t *err = nullptr;
            int ret = xcb_poll_for_reply64(conn, props.cookies[i].sequence, &replyPtr, &err);

            if (ret == 0) {
                windowDone = false;
                continue;
            }

            if (ret == 1 && replyPtr) {
                auto *reply = static_cast<xcb_get_property_reply_t *>(replyPtr);
                if (reply->type != 0 && reply->value_len > 0) {
                    QByteArray data(static_cast<const char *>(xcb_get_property_value(reply)),
                                    xcb_get_property_value_length(reply));
                    props.results.insert(props.requests[i].atom, data);
                }
                free(reply);
            } else if (err) {
                free(err);
            }
        }

        // Fire callback when all replies received AND propNotifySeen.
        if (windowDone && props.propNotifySeen) {
            toRemove.append(windowId);
        }
    }

    struct CompletedAsyncProperties
    {
        xcb_window_t windowId = XCB_WINDOW_NONE;
        QMap<xcb_atom_t, QByteArray> result;
        std::function<void(xcb_window_t, const QMap<xcb_atom_t, QByteArray> &)> callback;
    };

    QList<CompletedAsyncProperties> completed;

    // Cleanup completed windows.
    for (auto windowId : std::as_const(toRemove)) {
        auto it = asyncProps.find(windowId);
        if (it != asyncProps.end()) {
            CompletedAsyncProperties item;
            item.windowId = windowId;
            item.result = it->results;
            item.callback = std::move(it->callback);
            if (it->timer) {
                it->timer->stop();
                delete it->timer;
            }
            asyncProps.erase(it);
            completed.append(std::move(item));
        }
    }

    for (auto &item : completed) {
        if (item.callback)
            item.callback(item.windowId, item.result);
    }
}

void WXWaylandPrivate::xcbAsyncTimeoutForWindow(xcb_window_t windowId)
{
    W_Q(WXWayland);

    auto it = asyncProps.find(windowId);
    if (it == asyncProps.end())
        return;

    auto &props = it.value();

    xcb_connection_t *conn = q->xcbConnection();
    if (conn) {
        // Drain remaining replies.
        for (int i = 0; i < props.cookies.size(); ++i) {
            if (props.results.contains(props.requests[i].atom))
                continue;
            void *replyPtr = nullptr;
            xcb_generic_error_t *err = nullptr;
            int ret = xcb_poll_for_reply64(conn, props.cookies[i].sequence, &replyPtr, &err);
            if (ret == 1 && replyPtr) {
                auto *reply = static_cast<xcb_get_property_reply_t *>(replyPtr);
                if (reply->type != 0 && reply->value_len > 0) {
                    QByteArray data(static_cast<const char *>(xcb_get_property_value(reply)),
                                    xcb_get_property_value_length(reply));
                    props.results.insert(props.requests[i].atom, data);
                }
                free(reply);
            } else {
                if (replyPtr)
                    free(replyPtr);
                if (err)
                    free(err);
            }
        }
    }

    auto resultCopy = props.results;
    auto callback = std::move(props.callback);

    // Cleanup.
    if (props.timer) {
        props.timer->stop();
        props.timer->deleteLater();
    }
    asyncProps.erase(it);

    if (callback)
        callback(windowId, resultCopy);
}

void WXWayland::cancelAsyncProperties(xcb_window_t windowId)
{
    W_D(WXWayland);
    auto it = d->asyncProps.find(windowId);
    if (it == d->asyncProps.end())
        return;

    if (it->timer) {
        delete it->timer;
    }
    d->asyncProps.erase(it);
}

bool WXWayland::event(QEvent *ev)
{
    return QObject::event(ev);
}

WAYLIB_SERVER_END_NAMESPACE
