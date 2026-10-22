// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "idlemanager.h"
#include "modules/screensaver/screensaverinterfacev2.h"
#include "common/treelandlogging.h"

#include <wserver.h>
#include <wsurface.h>
#include <wxwaylandsurface.h>
#include <wxdgtoplevelsurface.h>
#include <wseat.h>
#include <wsocket.h>

#include <wlr_all.h>

#include <algorithm>
#include <utility>

IdleManager::IdleManager(QObject *parent)
    : QObject(parent)
{
    m_sinceActivity.start();
}

IdleManager::~IdleManager()
{
    teardown();
}

void IdleManager::initialize(WServer *server)
{
    m_idleNotifier = wlr_idle_notifier_v1_create(server->handle());
    m_idleInhibitManager = wlr_idle_inhibit_v1_create(server->handle());
    listeners()->add(&m_idleInhibitManager->events.new_inhibitor,
                     this, &IdleManager::onNewIdleInhibitor);
}

void IdleManager::setScreensaver(ScreensaverInterfaceV2 *screensaver)
{
    m_screensaver = screensaver;
}

void IdleManager::notifyActivity(WSeat *seat)
{
    wlr_idle_notifier_v1_notify_activity(m_idleNotifier, seat->handle());
    m_sinceActivity.restart();
}

bool IdleManager::isScreensaverInhibited() const
{
    return m_screensaver && m_screensaver->isInhibited();
}

qint64 IdleManager::idleMs() const
{
    return m_sinceActivity.isValid() ? m_sinceActivity.elapsed() : 0;
}

// A zwp_idle_inhibit surface only counts while it is mapped and, for an xdg
// toplevel, not minimized.
static bool inhibitorIsActive(wlr_idle_inhibitor_v1 *inhibitor)
{
    if (!inhibitor->surface)
        return false;

    auto *wsurface = WSurface::fromHandle(inhibitor->surface);
    if (!wsurface)
        return false;

    bool active = wsurface->mapped();
    auto *toplevel = WXdgToplevelSurface::fromSurface(wsurface);
    if (toplevel)
        active &= !toplevel->isMinimized();
    return active;
}

QList<IdleManager::Inhibitor> IdleManager::inhibitors() const
{
    QList<Inhibitor> result;

    for (const auto &entry : idleInhibitorEntries) {
        auto *inhibitor = entry.inhibitor;
        // wlroots destroys the inhibitor together with its surface, so this
        // should never be null; keep the same defence as onNewIdleInhibitor().
        if (!inhibitor->surface)
            continue;

        auto *wsurface = WSurface::fromHandle(inhibitor->surface);

        Inhibitor info;
        info.kind = QStringLiteral("idle-inhibit");
        info.surface = inhibitor->surface;
        info.active = inhibitorIsActive(inhibitor);
        if (wsurface) {
            if (auto *toplevel = WXdgToplevelSurface::fromSurface(wsurface))
                info.title = toplevel->title();
            else if (auto *xwayland = WXWaylandSurface::fromSurface(wsurface))
                info.title = xwayland->title();
        }
        if (auto *client = inhibitor->resource ? wl_resource_get_client(inhibitor->resource)
                                               : nullptr) {
            if (auto *wclient = WClient::get(client))
                info.appId = QString::fromUtf8(wclient->appId());
            if (auto creds = WClient::getCredentials(client))
                info.pid = creds->pid;
        }
        info.heldMs = static_cast<quint64>(entry.age.isValid() ? entry.age.elapsed() : 0);
        result.append(info);
    }

    if (m_screensaver) {
        for (const auto &inhibit : m_screensaver->inhibits()) {
            Inhibitor info;
            info.kind = QStringLiteral("screensaver");
            info.appId = inhibit.appId;
            info.pid = inhibit.pid;
            info.appName = inhibit.applicationName;
            info.reason = inhibit.reason;
            info.active = true;
            info.heldMs = inhibit.heldMs;
            result.append(info);
        }
    }

    return result;
}

void IdleManager::onNewIdleInhibitor(wlr_idle_inhibitor_v1 *wlr_inhibitor)
{
    if (!wlr_inhibitor->surface) {
        qCInfo(lcTlCore) << "Ignoring idle inhibitor with null surface";
        return;
    }

    auto wsurface = WSurface::fromHandle(wlr_inhibitor->surface);
    if (!wsurface) {
        qCWarning(lcTlCore) << "No WSurface found for idle inhibitor surface"
                                << wlr_inhibitor->surface;
        return;
    }

    IdleInhibitorEntry entry;
    entry.inhibitor = wlr_inhibitor;
    entry.age.start();
    // Single destroy signal: keep a WScopedListener on the entry itself
    // (equivalent to the old QObject context / QMetaObject::Connection).
    // Erasing the entry disconnects the listener via RAII.
    entry.destroyListener.init(&wlr_inhibitor->events.destroy, [this, wlr_inhibitor] (void *) {
        auto it = std::find_if(idleInhibitorEntries.begin(), idleInhibitorEntries.end(),
                               [wlr_inhibitor](const auto &e) {
            return e.inhibitor == wlr_inhibitor;
        });
        if (it != idleInhibitorEntries.end()) {
            auto connections = std::move(it->connections);
            idleInhibitorEntries.erase(it);
            for (const auto &connection : connections)
                QObject::disconnect(connection);
        }
        updateIdleInhibitor();
    });

    QList<QMetaObject::Connection> connections;
    connections << connect(wsurface, &WSurface::mappedChanged, this, &IdleManager::updateIdleInhibitor);

    auto toplevel = WXdgToplevelSurface::fromSurface(wsurface);
    if (toplevel) {
        connections << connect(toplevel, &WXdgToplevelSurface::minimizeChanged, this, &IdleManager::updateIdleInhibitor);
    }

    entry.connections = std::move(connections);
    idleInhibitorEntries.push_back(std::move(entry));

    updateIdleInhibitor();
}

void IdleManager::updateIdleInhibitor()
{
    m_inhibited = isScreensaverInhibited();

    if (!m_inhibited) {
        for (const auto &entry : idleInhibitorEntries) {
            if (inhibitorIsActive(entry.inhibitor)) {
                m_inhibited = true;
                break;
            }
        }
    }

    wlr_idle_notifier_v1_set_inhibited(m_idleNotifier, m_inhibited);
}
