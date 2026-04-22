// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "winewindowstate.h"

#include "qwayland-server-treeland-wine-window-state-v1.h"

#include "core/rootsurfacecontainer.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"

#include <wxdgtoplevelsurface.h>

#include <qwdisplay.h>
#include <qwxdgshell.h>

#include <QHash>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(treelandWineWindowState, "treeland.wine.window.state", QtInfoMsg)

#define TREELAND_WINE_WINDOW_STATE_MANAGER_V1_VERSION 1

WAYLIB_SERVER_USE_NAMESPACE
QW_USE_NAMESPACE

class WineWindowState;

class WineWindowStateManagerPrivate : public QtWaylandServer::treeland_wine_window_state_manager_v1
{
public:
    explicit WineWindowStateManagerPrivate(WineWindowStateManager *q)
        : q(q)
    {
    }

    wl_global *globalHandle() const
    {
        return m_global;
    }

    void removeState(WineWindowState *state);

private:
    WineWindowStateManager *q = nullptr;
    QHash<WXdgToplevelSurface *, WineWindowState *> m_toplevelStates;
    QHash<WineWindowState *, WXdgToplevelSurface *> m_stateToplevels;

    void bindState(WXdgToplevelSurface *toplevel, WineWindowState *state)
    {
        m_toplevelStates.insert(toplevel, state);
        m_stateToplevels.insert(state, toplevel);
    }

protected:
    void destroy_global() override
    {
        qCDebug(treelandWineWindowState) << "WineWindowStateManager global destroyed";
    }

    void destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void get_window_state(Resource *resource,
                          uint32_t id,
                          struct ::wl_resource *toplevelResource) override;
};

class WineWindowState
    : public QObject
    , public QtWaylandServer::treeland_wine_window_state_v1
{
public:
    WineWindowState(QObject *parent,
                    WineWindowStateManagerPrivate *manager,
                    WXdgToplevelSurface *toplevel,
                    SurfaceWrapper *wrapper,
                    wl_client *client,
                    int version,
                    uint32_t id)
        : QObject(parent)
        , QtWaylandServer::treeland_wine_window_state_v1(client, id, version)
        , m_manager(manager)
        , m_toplevel(toplevel)
        , m_wrapper(wrapper)
    {
        if (m_toplevel) {
            m_toplevel->safeConnect(&WToplevelSurface::minimizeChanged, this, [this] {
                sendStateChanged();
            });
            connect(m_toplevel, &QObject::destroyed, this, [this] {
                m_alive = false;
                m_toplevel = nullptr;
                m_wrapper = nullptr;
            });
        }

        if (m_wrapper) {
            connect(m_wrapper, &QObject::destroyed, this, [this] {
                m_wrapper = nullptr;
            });
        }

        sendStateChanged();
    }

protected:
    void destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void destroy_resource([[maybe_unused]] Resource *resource) override
    {
        if (m_manager) {
            m_manager->removeState(this);
            m_manager = nullptr;
        }
        delete this;
    }

    void unminimize([[maybe_unused]] Resource *resource) override
    {
        if (!m_alive || !m_wrapper)
            return;
        if (m_wrapper->isMinimized())
            m_wrapper->requestCancelMinimize();
    }

    void activate([[maybe_unused]] Resource *resource,
                  uint32_t serial,
                  [[maybe_unused]] uint32_t reason,
                  [[maybe_unused]] struct ::wl_resource *seat) override
    {
        if (!m_alive || !m_wrapper || !m_wrapper->shellSurface() || !m_wrapper->surface()) {
            send_activate_denied(serial);
            return;
        }

        auto *helper = Helper::instance();
        if (!helper || helper->blockActivateSurface() || !m_wrapper->acceptKeyboardFocus()
            || !m_wrapper->hasActiveCapability()
            || !m_wrapper->shellSurface()->hasCapability(WToplevelSurface::Capability::Activate)) {
            send_activate_denied(serial);
            return;
        }

        helper->forceActivateSurface(m_wrapper);
    }

    void set_attention([[maybe_unused]] Resource *resource,
                       [[maybe_unused]] uint32_t count,
                       [[maybe_unused]] uint32_t timeoutMs) override
    {
        if (!m_alive)
            return;
        m_attention = true;
        sendStateChanged();
    }

    void clear_attention([[maybe_unused]] Resource *resource) override
    {
        if (!m_alive)
            return;
        m_attention = false;
        sendStateChanged();
    }

private:
    void sendStateChanged()
    {
        if (!m_alive)
            return;

        uint32_t state = 0;
        if (m_wrapper && m_wrapper->isMinimized())
            state |= TREELAND_WINE_WINDOW_STATE_V1_STATE_MINIMIZED;
        if (m_attention)
            state |= TREELAND_WINE_WINDOW_STATE_V1_STATE_ATTENTION;

        send_state_changed(state);
    }

private:
    WineWindowStateManagerPrivate *m_manager = nullptr;
    WXdgToplevelSurface *m_toplevel = nullptr;
    SurfaceWrapper *m_wrapper = nullptr;
    bool m_alive = true;
    bool m_attention = false;
};

void WineWindowStateManagerPrivate::removeState(WineWindowState *state)
{
    auto it = m_stateToplevels.find(state);
    if (it == m_stateToplevels.end())
        return;

    auto *toplevel = it.value();
    m_stateToplevels.erase(it);
    m_toplevelStates.remove(toplevel);
}

void WineWindowStateManagerPrivate::get_window_state(Resource *resource,
                                                      uint32_t id,
                                                      struct ::wl_resource *toplevelResource)
{
    auto *qXdgToplevel = qw_xdg_toplevel::from_resource(toplevelResource);
    auto *xdgToplevel = WXdgToplevelSurface::fromHandle(qXdgToplevel);
    auto *helper = Helper::instance();
    auto *root = helper ? helper->rootSurfaceContainer() : nullptr;
    auto *wrapper = (root && xdgToplevel) ? root->getSurface(xdgToplevel) : nullptr;

    if (!qXdgToplevel || !xdgToplevel || !wrapper) {
        wl_resource_post_error(resource->handle,
                               TREELAND_WINE_WINDOW_STATE_MANAGER_V1_ERROR_DEFUNCT_TOPLEVEL,
                               "invalid or defunct toplevel");
        return;
    }

    if (m_toplevelStates.contains(xdgToplevel)) {
        wl_resource_post_error(resource->handle,
                               TREELAND_WINE_WINDOW_STATE_MANAGER_V1_ERROR_TOPLEVEL_ALREADY_BOUND,
                               "toplevel already bound");
        return;
    }

    auto *state = new WineWindowState(q,
                                      this,
                                      xdgToplevel,
                                      wrapper,
                                      resource->client(),
                                      resource->version(),
                                      id);
    bindState(xdgToplevel, state);
}

WineWindowStateManager::WineWindowStateManager(QObject *parent)
    : QObject(parent)
    , d(new WineWindowStateManagerPrivate(this))
{
}

WineWindowStateManager::~WineWindowStateManager() = default;

void WineWindowStateManager::create(WServer *server)
{
    d->init(*server->handle(), TREELAND_WINE_WINDOW_STATE_MANAGER_V1_VERSION);
}

void WineWindowStateManager::destroy([[maybe_unused]] WServer *server)
{
    d->globalRemove();
}

wl_global *WineWindowStateManager::global() const
{
    return d->globalHandle();
}

QByteArrayView WineWindowStateManager::interfaceName() const
{
    return QtWaylandServer::treeland_wine_window_state_manager_v1::interfaceName();
}
