// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "sessionprelaunch.h"
#include "session.h"
#include "core/waylandserver.h"
#include "core/rootsurfacecontainer.h"
#include "core/windowconfigstore.h"
#include "modules/prelaunch-splash/prelaunchsplash.h"
#include "modules/app-id-resolver/appidresolver.h"
#include "treelandconfig.hpp"
#include "treelanduserconfig.hpp"
#include "workspace/workspace.h"
#include <wlr_all.h>
#include <wtoplevelsurface.h>
#include <QTimer>
#include <algorithm>
#include <utility>

WAYLIB_SERVER_USE_NAMESPACE

SessionPrelaunch::SessionPrelaunch(Session *owner, WaylandServer *runtime)
    : SessionComponent(owner), m_runtime(runtime), m_store(new WindowConfigStore(this))
    , m_protocol(new PrelaunchSplash)
{
    owner->attachInterface(m_protocol);
    m_resolver = new AppIdResolverManager;
    owner->attachInterface(m_resolver);
    connect(m_protocol, &PrelaunchSplash::splashRequested, this, &SessionPrelaunch::request);
    connect(m_protocol, &PrelaunchSplash::splashCloseRequested, this, &SessionPrelaunch::close);
}

PrelaunchSplash *SessionPrelaunch::protocol() const { return m_protocol; }

bool SessionPrelaunch::resolvePidfd(int pidfd, std::function<void(const QString &)> callback)
{
    return m_resolver && m_resolver->resolvePidfd(pidfd, std::move(callback));
}

bool SessionPrelaunch::needsResolution() const
{
    return !m_pending.isEmpty() || !m_splashes.isEmpty() || !m_closed.isEmpty();
}

bool SessionPrelaunch::consumeClosed(const QString &appId)
{
    return !appId.isEmpty() && m_closed.remove(appId);
}

void SessionPrelaunch::request(const QString &appId, const QString &instanceId, wlr_buffer *buffer)
{
    // try_from_resource hands us a locked buffer. Both completed and cancelled
    // async requests release it, including when this Session is destroyed.
    const Icon icon(buffer, [](wlr_buffer *value) { if (value) wlr_buffer_unlock(value); });
    if (!m_runtime->globalConfig()->enablePrelaunchSplash() || appId.isEmpty()
        || m_pending.contains(appId)
        || std::any_of(m_splashes.cbegin(), m_splashes.cend(), [&appId](const Splash &splash) {
               return splash.wrapper && splash.wrapper->appId() == appId;
           }))
        return;
    const auto generation = ++m_generation;
    m_pending.insert(appId, {instanceId, generation});
    m_store->withSplashConfigFor(appId, this,
        [this, appId, instanceId, generation, icon](const QSize &size, const QString &dark,
                                                  const QString &light, qlonglong theme) {
            create(appId, instanceId, generation, icon, size, dark, light, theme);
        },
        [this, appId, generation] {
            const auto pending = m_pending.constFind(appId);
            if (pending != m_pending.cend() && pending->generation == generation)
                m_pending.remove(appId);
        });
}

void SessionPrelaunch::create(const QString &appId, const QString &instanceId, quint64 generation,
                              const Icon &icon, const QSize &size, const QString &darkPalette,
                              const QString &lightPalette, qlonglong theme)
{
    const auto pending = m_pending.constFind(appId);
    if (pending == m_pending.cend() || pending->generation != generation)
        return;
    m_pending.remove(appId);
    const bool dark = theme == 0 ? session()->config()->windowColorScheme() == 1 : theme == 2;
    auto *wrapper = new SurfaceWrapper(m_runtime->qmlEngine(), nullptr, size, appId,
                                       icon.get(), QColor(dark ? darkPalette : lightPalette));
    wrapper->setSession(session());
    m_splashes.append({wrapper, instanceId});
    session()->workspace()->addSurface(wrapper);
    Q_EMIT splashCreated(wrapper);
    connect(wrapper, &SurfaceWrapper::splashCloseRequested, this,
            [this, wrapper] { discard(wrapper, true); });
    const auto timeout = m_runtime->globalConfig()->prelaunchSplashTimeoutMs();
    if (timeout > 0) {
        QTimer::singleShot(static_cast<int>(qMin<qlonglong>(timeout, 60000)), this,
            [this, wrapper = QPointer(wrapper)] {
                if (wrapper)
                    discard(wrapper, false);
            });
    }
}

void SessionPrelaunch::discard(SurfaceWrapper *wrapper, bool closeWindow)
{
    auto it = std::find_if(m_splashes.begin(), m_splashes.end(), [wrapper](const Splash &splash) {
        return splash.wrapper == wrapper;
    });
    if (it == m_splashes.end())
        return; // Already matched to a real window; its old timeout must do nothing.
    m_splashes.erase(it);
    const auto appId = wrapper->appId();
    if (closeWindow)
        m_closed.insert(appId);
    m_runtime->rootSurfaceContainer()->destroyForSurface(wrapper);
    m_store->recordSplashMismatch(appId);
}

void SessionPrelaunch::close(const QString &appId, const QString &instanceId)
{
    const auto pending = m_pending.constFind(appId);
    if (pending != m_pending.cend() && pending->instanceId == instanceId)
        m_pending.remove(appId);
    for (const auto &splash : std::as_const(m_splashes)) {
        if (splash.wrapper && splash.wrapper->appId() == appId && splash.instanceId == instanceId) {
            discard(splash.wrapper, false);
            return;
        }
    }
}

SurfaceWrapper *SessionPrelaunch::takeSplash(const QString &appId, WToplevelSurface *surface,
                                            SurfaceWrapper::Type type)
{
    if (appId.isEmpty() || !surface || !session()->ownsClient(surface->waylandClient()))
        return nullptr;
    m_pending.remove(appId);
    for (auto it = m_splashes.begin(); it != m_splashes.end(); ++it) {
        auto *wrapper = it->wrapper.data();
        if (!wrapper || wrapper->appId() != appId)
            continue;
        m_splashes.erase(it);
        wrapper->convertToNormalSurface(surface, type);
        m_store->resetSplashMismatchCount(appId);
        return wrapper;
    }
    return nullptr;
}
