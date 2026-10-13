// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "idlemanager.h"
#include "modules/screensaver/screensaverinterfacev2.h"
#include "common/treelandlogging.h"

#include <wserver.h>
#include <wsurface.h>
#include <wxdgtoplevelsurface.h>
#include <wseat.h>

#include <wlr_all.h>

#include <algorithm>
#include <utility>

IdleManager::IdleManager(QObject *parent)
    : QObject(parent)
{}

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
    if (m_screensaver->isInhibited()) {
        wlr_idle_notifier_v1_set_inhibited(m_idleNotifier, true);
        return;
    }

    for (const auto &entry : idleInhibitorEntries) {
        auto *inhibitor = entry.inhibitor;
        auto wsurface = WSurface::fromHandle(inhibitor->surface);
        if (!wsurface)
            continue;

        bool visible = wsurface->mapped();
        auto toplevel = WXdgToplevelSurface::fromSurface(wsurface);
        if (toplevel)
            visible &= !toplevel->isMinimized();

        if (visible) {
            wlr_idle_notifier_v1_set_inhibited(m_idleNotifier, true);
            return;
        }
    }
    wlr_idle_notifier_v1_set_inhibited(m_idleNotifier, false);
}
