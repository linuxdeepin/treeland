// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "shellhandler.h"

#include "common/treelandlogging.h"
#include "core/imcandidatepanelmanager.h"
#include "core/qmlengine.h"
#include "core/windowconfigstore.h"
#include "layersurfacecontainer.h"
#include "modules/app-id-resolver/appidresolver.h"
#include "modules/dde-shell/ddeshellmanagerinterfacev1.h"
#include "modules/foreign-toplevel/foreigntoplevelmanagerv1.h"
#include "modules/prelaunch-splash/prelaunchsplash.h"
#include "modules/wine-window-management/winewindowmanagement.h"
#include "modules/wine-window-state/winewindowstate.h"
#include "output/output.h"
#include "rootsurfacecontainer.h"
#include "seat/helper.h"
#include "seat/seatsmanager.h"
#include "session/session.h"
#include "surface/seatsurfacemanager.h"
#include "surface/surfacewrapper.h"
#include "treelandconfig.hpp"
#include "treelanduserconfig.hpp"
#include "wallpapershellinterfacev1.h"
#include "workspace/workspace.h"

#include <xcb/xcb.h>

#include <winputmethodhelper.h>
#include <winputpopupsurface.h>
#include <wlayershell.h>
#include <wlayersurface.h>
#include <woutputrenderwindow.h>
#include <wserver.h>
#include <wxdgpopupsurface.h>
#include <wxdgshell.h>
#include <wxdgtoplevelsurface.h>
#include <wxwayland.h>
#include <wxwaylandsurface.h>
#include <wxwaylandsurfaceitem.h>

#include <qwbuffer.h>
#include <qwcompositor.h>
#include <qwxwaylandsurface.h>

#include <QColor>
#include <QPointer>
#include <QTimer>

#include <algorithm>
#include <functional>
#include <optional>

QW_USE_NAMESPACE
WAYLIB_SERVER_USE_NAMESPACE

#define TREELAND_XDG_SHELL_VERSION 5

namespace {
bool isValidRestoreSize(const QSize &size)
{
    return size.isValid() && size.width() > 0 && size.height() > 0;
}

QSize restoreSizeForPrelaunchWrapper(const SurfaceWrapper *wrapper)
{
    QSize size = wrapper->normalGeometry().size().toSize();
    if (!isValidRestoreSize(size))
        size = QSize(qRound(wrapper->implicitWidth()), qRound(wrapper->implicitHeight()));
    return isValidRestoreSize(size) ? size : QSize();
}
} // namespace

ShellHandler::ShellHandler(RootSurfaceContainer *rootContainer, WServer *server)
    : m_rootSurfaceContainer(rootContainer)
    , m_backgroundContainer(new LayerSurfaceContainer(rootContainer))
    , m_bottomContainer(new LayerSurfaceContainer(rootContainer))
    , m_workspace(new Workspace(rootContainer))
    , m_topContainer(new LayerSurfaceContainer(rootContainer))
    , m_overlayContainer(new LayerSurfaceContainer(rootContainer))
    , m_popupContainer(new SurfaceContainer(rootContainer))
    , m_windowConfigStore(new WindowConfigStore(this))
{
    m_treelandForeignToplevel = server->attach<ForeignToplevelManagerInterfaceV1>();
    Q_ASSERT(m_treelandForeignToplevel);
    qmlRegisterSingletonInstance<ForeignToplevelManagerInterfaceV1>(
        "Treeland.Protocols",
        1,
        0,
        "ForeignToplevelManagerInterfaceV1",
        m_treelandForeignToplevel);
    qRegisterMetaType<ForeignToplevelManagerInterfaceV1::PreviewDirection>();

    m_backgroundContainer->setZ(RootSurfaceContainer::BackgroundZOrder);
    m_backgroundContainer->setObjectName(QStringLiteral("BackgroundContainer"));
    m_bottomContainer->setZ(RootSurfaceContainer::BottomZOrder);
    m_bottomContainer->setObjectName(QStringLiteral("BottomContainer"));
    m_workspace->setZ(RootSurfaceContainer::NormalZOrder);
    m_workspace->setObjectName(QStringLiteral("WorkspaceContainer"));
    m_topContainer->setZ(RootSurfaceContainer::TopZOrder);
    m_topContainer->setObjectName(QStringLiteral("TopContainer"));
    m_overlayContainer->setZ(RootSurfaceContainer::OverlayZOrder);
    m_overlayContainer->setObjectName(QStringLiteral("OverlayContainer"));
    m_popupContainer->setZ(RootSurfaceContainer::PopupZOrder);
    m_popupContainer->setObjectName(QStringLiteral("PopupContainer"));
}

void ShellHandler::updateWrapperContainer(SurfaceWrapper *wrapper, WSurface *parentSurface)
{
    if (wrapper->parentSurface())
        wrapper->parentSurface()->removeSubSurface(wrapper);

    auto oldContainer = wrapper->container();
    if (parentSurface) {
        auto parentWrapper = m_rootSurfaceContainer->getSurface(parentSurface);
        auto parentContainer = qobject_cast<SurfaceContainer *>(parentWrapper->container());
        parentWrapper->addSubSurface(wrapper);
        if (oldContainer != parentContainer) {
            if (oldContainer)
                oldContainer->removeSurface(wrapper);
            if (auto ws = qobject_cast<Workspace *>(parentContainer))
                ws->addSurface(wrapper, parentWrapper->workspaceId());
            else
                parentContainer->addSurface(wrapper);
        }
    } else {
        if (oldContainer) {
            if (qobject_cast<Workspace *>(oldContainer) == nullptr) {
                oldContainer->removeSurface(wrapper);
                m_workspace->addSurface(wrapper);
            }
            // else do nothing, already in workspace
        } else {
            m_workspace->addSurface(wrapper);
        }
    }
}

// Prelaunch splash request: create a SurfaceWrapper that is not yet bound to a shellSurface
void ShellHandler::handlePrelaunchSplashRequested(const QString &appId,
                                                  const QString &instanceId,
                                                  QW_NAMESPACE::qw_buffer *iconBuffer)
{
    m_unmatchedPrelaunchAppIds.remove(appId);

    auto skipSplash = [this, appId, iconBuffer] {
        if (iconBuffer) {
            iconBuffer->unlock();
        }
        m_pendingPrelaunchAppIds.remove(appId);
    };

    Q_UNUSED(instanceId); // TODO: will be provided by AM DBus in future

    if (!Helper::instance()->globalConfig()->enablePrelaunchSplash() || !m_appIdResolverManager
        || appId.isEmpty()
        // If a prelaunch wrapper with the same appId already exists, skip creating a duplicate.
        || std::any_of(m_prelaunchWrappers.cbegin(),
                       m_prelaunchWrappers.cend(),
                       [&appId](SurfaceWrapper *w) {
                           return w && w->appId() == appId;
                       })
        // Flow: add appId to pending list -> wait for dconfig init ->
        // if pending still exists, create splash; otherwise a real window appeared and cleared it.
        || m_pendingPrelaunchAppIds.contains(appId)) {
        skipSplash();
        return;
    }

    m_pendingPrelaunchAppIds.insert(appId);

    m_windowConfigStore->withSplashConfigFor(appId,
                                             this,
                                             std::bind(&ShellHandler::createPrelaunchSplash,
                                                       this,
                                                       appId,
                                                       instanceId,
                                                       iconBuffer,
                                                       std::placeholders::_1,
                                                       std::placeholders::_2,
                                                       std::placeholders::_3,
                                                       std::placeholders::_4),
                                             skipSplash);
}

void ShellHandler::createPrelaunchSplash(const QString &appId,
                                         const QString &instanceId,
                                         QW_NAMESPACE::qw_buffer *iconBuffer,
                                         const QSize &lastSize,
                                         const QString &darkPalette,
                                         const QString &lightPalette,
                                         qlonglong splashThemeType)
{
    Q_UNUSED(instanceId); // TODO: will be provided by AM DBus in future

    if (!m_pendingPrelaunchAppIds.contains(appId)) {
        // A real window may have consumed the pending identity while DConfig was still loading.
        // Preserve the now-available restore size for that wrapper instead of creating a late
        // splash.
        updateUnmatchedPrelaunchLastSize(appId, lastSize);
        if (iconBuffer) {
            iconBuffer->unlock();
        }
        return; // app window already created while waiting for dconfig
    }
    m_pendingPrelaunchAppIds.remove(appId);

    const qlonglong effectiveType =
        splashThemeType == 0 ? Helper::instance()->config()->windowThemeType() : splashThemeType;
    const QColor splashColor = effectiveType == 1 ? QColor(lightPalette) : QColor(darkPalette);

    auto *wrapper = new SurfaceWrapper(Helper::instance()->qmlEngine(),
                                       nullptr,
                                       lastSize,
                                       appId,
                                       iconBuffer,
                                       splashColor);
    if (iconBuffer) {
        iconBuffer->unlock();
    }
    m_prelaunchWrappers.append(wrapper);
    m_workspace->addSurface(wrapper);
    setupSurfaceActiveWatcher(wrapper);
    registerSurfaceToForeignToplevel(wrapper);

    // After configurable timeout, if still unmatched (not converted and still in the list),
    // destroy the splash wrapper
    qlonglong timeoutMs = Helper::instance()->globalConfig()->prelaunchSplashTimeoutMs();
    // Bounds: <=0 disables auto-destroy, >60000 clamps to 60000
    if (timeoutMs > 60000) {
        qCWarning(lcTlShell)
            << "Prelaunch splash timeout too long, clamping to 60000ms, requested:" << timeoutMs;
        timeoutMs = 60000;
    }

    // Listen for splash close request
    connect(wrapper, &SurfaceWrapper::splashCloseRequested, this, [this, wrapper]() {
        const QString appId = wrapper->appId();
        qCInfo(lcTlShell) << "Splash close requested for appId=" << appId;

        // Add to closed splash list
        if (!appId.isEmpty()) {
            m_closedSplashAppIds.insert(appId);
        }

        // Remove from prelaunch wrappers list
        m_prelaunchWrappers.removeOne(wrapper);

        // Destroy the splash wrapper
        m_rootSurfaceContainer->destroyForSurface(wrapper);
    });

    if (timeoutMs > 0) {
        QTimer::singleShot(static_cast<int>(timeoutMs),
                           wrapper,
                           [this, wrapper = QPointer<SurfaceWrapper>(wrapper)] {
                               if (!wrapper) {
                                   return; // Wrapper already destroyed elsewhere
                               }
                               int idx = m_prelaunchWrappers.indexOf(wrapper);
                               if (idx < 0) {
                                   return; // Already matched or removed earlier
                               }
                               qCDebug(lcTlShell)
                                   << "Prelaunch splash timeout, destroy wrapper appId="
                                   << wrapper->appId();
                               rememberUnmatchedPrelaunchAppId(
                                   wrapper->appId(),
                                   restoreSizeForPrelaunchWrapper(wrapper));
                               m_prelaunchWrappers.removeAt(idx);
                               m_rootSurfaceContainer->destroyForSurface(wrapper);
                           });
    }
}

void ShellHandler::handlePrelaunchSplashClosed(const QString &appId, const QString &instanceId)
{
    Q_UNUSED(instanceId); // TODO: will be provided by AM DBus in future

    // Remove pending prelaunch request if it hasn't created a wrapper yet.
    const bool removedPendingRequest = m_pendingPrelaunchAppIds.remove(appId);

    // Find and destroy any existing prelaunch wrapper with the matching appId
    for (int i = 0; i < m_prelaunchWrappers.size(); ++i) {
        auto *wrapper = m_prelaunchWrappers[i];
        if (wrapper->appId() == appId) {
            qCDebug(lcTlShell)
                << "Client requested close_splash, destroy wrapper appId=" << appId;
            rememberUnmatchedPrelaunchAppId(appId, restoreSizeForPrelaunchWrapper(wrapper));
            m_prelaunchWrappers.removeAt(i);
            m_rootSurfaceContainer->destroyForSurface(wrapper);
            return;
        }
    }

    if (removedPendingRequest)
        rememberUnmatchedPrelaunchAppId(appId);
}

bool ShellHandler::hasPrelaunchAppIdCandidates() const
{
    return !m_prelaunchWrappers.isEmpty() || !m_pendingPrelaunchAppIds.isEmpty()
        || !m_closedSplashAppIds.isEmpty() || !m_unmatchedPrelaunchAppIds.isEmpty();
}

void ShellHandler::rememberUnmatchedPrelaunchAppId(const QString &appId,
                                                   const QSize &lastNormalSize)
{
    if (appId.isEmpty() || m_closedSplashAppIds.contains(appId))
        return;

    const quint64 generation = ++m_prelaunchAppIdGeneration;
    UnmatchedPrelaunchInfo info;
    info.generation = generation;
    if (isValidRestoreSize(lastNormalSize))
        info.lastNormalSize = lastNormalSize;
    m_unmatchedPrelaunchAppIds.insert(appId, info);

    const qlonglong configuredTimeout =
        Helper::instance()->globalConfig()->prelaunchSplashTimeoutMs();
    const qlonglong requestedRetention =
        configuredTimeout > 0 ? configuredTimeout : qlonglong(5000);
    const int retentionMs = static_cast<int>(
        qBound(qlonglong(5000), requestedRetention, qlonglong(60000)));
    qCDebug(lcTlShell) << "Retaining unmatched prelaunch appId" << appId << "normal size"
                       << info.lastNormalSize << "for" << retentionMs << "ms";

    QTimer::singleShot(retentionMs, this, [this, appId, generation] {
        const auto it = m_unmatchedPrelaunchAppIds.constFind(appId);
        if (it != m_unmatchedPrelaunchAppIds.cend() && it->generation == generation)
            m_unmatchedPrelaunchAppIds.remove(appId);
    });
}

void ShellHandler::updateUnmatchedPrelaunchLastSize(const QString &appId,
                                                    const QSize &lastNormalSize)
{
    if (!isValidRestoreSize(lastNormalSize))
        return;

    auto it = m_unmatchedPrelaunchAppIds.find(appId);
    if (it == m_unmatchedPrelaunchAppIds.end())
        return;

    it->lastNormalSize = lastNormalSize;
    for (const QPointer<SurfaceWrapper> &wrapper : std::as_const(it->waitingWrappers)) {
        if (wrapper)
            wrapper->setRestoredNormalSize(lastNormalSize);
    }
    it->waitingWrappers.clear();
    qCDebug(lcTlShell) << "Updated unmatched prelaunch normal size for" << appId << "to"
                       << lastNormalSize;
}

void ShellHandler::seedUnmatchedPrelaunchLastSize(const QString &appId,
                                                 SurfaceWrapper *wrapper)
{
    if (appId.isEmpty() || !wrapper)
        return;

    auto it = m_unmatchedPrelaunchAppIds.find(appId);
    if (it == m_unmatchedPrelaunchAppIds.end())
        return;

    if (isValidRestoreSize(it->lastNormalSize)) {
        wrapper->setRestoredNormalSize(it->lastNormalSize);
        qCDebug(lcTlShell) << "Seeded unmatched prelaunch normal size for" << appId << "as"
                           << it->lastNormalSize;
        return;
    }

    if (!it->waitingWrappers.contains(wrapper))
        it->waitingWrappers.append(wrapper);
}

Workspace *ShellHandler::workspace() const
{
    return m_workspace;
}

SurfaceContainer *ShellHandler::popupContainer() const
{
    return m_popupContainer;
}

RootSurfaceContainer *ShellHandler::rootSurfaceContainer() const
{
    return m_rootSurfaceContainer;
}

ForeignToplevelManagerInterfaceV1 *ShellHandler::foreignToplevel() const
{
    return m_treelandForeignToplevel;
}

void ShellHandler::createComponent(QmlEngine *engine, QQuickItem *parentItem)
{
    m_windowMenu = engine->createWindowMenu(Helper::instance());
    QObject::connect(m_windowMenu, SIGNAL(closed()), this, SLOT(onWindowMenuClosed()));
    m_dockPreview = engine->createDockPreview(parentItem);
    setupDockPreview();
}

void ShellHandler::init(WServer *server, WSeat *seat)
{
    Q_ASSERT_X(server, Q_FUNC_INFO, "server must not be null");
    Q_ASSERT_X(seat, Q_FUNC_INFO, "seat must not be null");
    Q_ASSERT_X(!m_prelaunchSplash, Q_FUNC_INFO, "Only init once!");
    Q_ASSERT_X(!m_appIdResolverManager, Q_FUNC_INFO, "Only init once!");
    Q_ASSERT_X(!m_wineWindowStateManager, Q_FUNC_INFO, "Only init once!");
    Q_ASSERT_X(!m_wineWindowManager, Q_FUNC_INFO, "Only init once!");
    Q_ASSERT_X(!m_xdgShell, Q_FUNC_INFO, "Only init once!");
    Q_ASSERT_X(!m_layerShell, Q_FUNC_INFO, "Only init once!");
    Q_ASSERT_X(!m_wallpaperShell, Q_FUNC_INFO, "Only init once!");
    Q_ASSERT_X(!m_inputMethodHelper, Q_FUNC_INFO, "Only init once!");

    m_prelaunchSplash = server->attach<PrelaunchSplash>();
    connect(m_prelaunchSplash,
            &PrelaunchSplash::splashRequested,
            this,
            &ShellHandler::handlePrelaunchSplashRequested);
    connect(m_prelaunchSplash,
            &PrelaunchSplash::splashCloseRequested,
            this,
            &ShellHandler::handlePrelaunchSplashClosed);

    m_appIdResolverManager = server->attach<AppIdResolverManager>();
    m_wineWindowStateManager = server->attach<WineWindowStateManager>();
    m_wineWindowManager = server->attach<WineWindowManager>();

    m_xdgShell = server->attach<WXdgShell>(TREELAND_XDG_SHELL_VERSION);
    connect(m_xdgShell,
            &WXdgShell::toplevelSurfaceAdded,
            this,
            &ShellHandler::onXdgToplevelSurfaceAdded);
    connect(m_xdgShell,
            &WXdgShell::toplevelSurfaceRemoved,
            this,
            &::ShellHandler::onXdgToplevelSurfaceRemoved);
    connect(m_xdgShell, &WXdgShell::popupSurfaceAdded, this, &ShellHandler::onXdgPopupSurfaceAdded);
    connect(m_xdgShell,
            &WXdgShell::popupSurfaceRemoved,
            this,
            &ShellHandler::onXdgPopupSurfaceRemoved);

    m_layerShell = server->attach<WLayerShell>(m_xdgShell);
    connect(m_layerShell, &WLayerShell::surfaceAdded, this, &ShellHandler::onLayerSurfaceAdded);
    connect(m_layerShell, &WLayerShell::surfaceRemoved, this, &ShellHandler::onLayerSurfaceRemoved);

    m_wallpaperShell = server->attach<TreelandWallpaperShellInterfaceV1>(m_wallpaperShell);
    if (Helper::instance()->isDDMDisplay()) {
        m_wallpaperShell->setFilter([](WClient *client) {
            return Helper::instance()->sessionManager()->isDDEUserClient(client);
        });
    }

    m_inputMethodHelper = new WInputMethodHelper(server, seat);
    m_inputMethodHelper->setParent(this);

    m_imCandidatePanelManager = new IMCandidatePanelManager(this, m_inputMethodHelper, this);

    connect(m_inputMethodHelper,
            &WInputMethodHelper::inputPopupSurfaceV2Added,
            this,
            &ShellHandler::onInputPopupSurfaceV2Added);
    connect(m_inputMethodHelper,
            &WInputMethodHelper::inputPopupSurfaceV2Removed,
            this,
            &ShellHandler::onInputPopupSurfaceV2Removed);
}

WXWayland *ShellHandler::createXWayland(WServer *server,
                                        WSeat *seat,
                                        qw_compositor *compositor,
                                        [[maybe_unused]] bool lazy)
{
    auto *xwayland = server->attach<WXWayland>(compositor, false);
    m_xwaylands.append(xwayland);
    xwayland->setSeat(seat);
    connect(xwayland, &WXWayland::surfaceAdded, this, &ShellHandler::onXWaylandSurfaceAdded);
    connect(xwayland, &WXWayland::ready, xwayland, [this, xwayland] {
        auto atomPid = xwayland->atom("_NET_WM_PID");
        xwayland->setAtomSupported(atomPid, true);
        auto atomNoTitlebar = xwayland->atom("_DEEPIN_NO_TITLEBAR");
        xwayland->setAtomSupported(atomNoTitlebar, true);

        if (m_imCandidatePanelManager)
            m_imCandidatePanelManager->setupXWayland(xwayland);
    });
    return xwayland;
}

void ShellHandler::removeXWayland(WXWayland *xwayland)
{
    m_xwaylands.removeOne(xwayland);
    // Ensure xwayland is removed from interfaceList
    if (auto server = WServer::from(xwayland))
        server->detach(xwayland);
    delete xwayland;
}

void ShellHandler::onXdgToplevelSurfaceAdded(WXdgToplevelSurface *surface)
{
    surface->safeConnect(
        &WXdgToplevelSurface::initialConfigureRequested,
        this,
        [this, surface] {
            if (surface->isMaximizeRequested())
                configureInitialXdgMaximize(surface);
        },
        Qt::DirectConnection);
    surface->safeConnect(&WToplevelSurface::requestMaximize, this, [this, surface] {
        if (surface->isInitialized() && surface->surface()
            && (!surface->surface()->mapped()
                || !m_rootSurfaceContainer->getSurface(surface))) {
            configureInitialXdgMaximize(surface);
        }
    });
    surface->safeConnect(&WToplevelSurface::requestCancelMaximize, this, [this, surface] {
        cancelPendingInitialXdgMaximize(surface);
    });

    // If there are prelaunch identities to match and the resolver is available
    // -> attempt async resolve; remaining logic continues in the callback on success
    if (hasPrelaunchAppIdCandidates() && m_appIdResolverManager) {
        int pidfd = surface->pidFD();
        if (pidfd >= 0) {
            // Register pending before starting async resolve (unified list)
            m_pendingAppIdResolveToplevels.append(surface);

            bool started = m_appIdResolverManager->resolvePidfd(
                pidfd,
                [this, surface = QPointer<WXdgToplevelSurface>(surface)](const QString &appId) {
                    auto raw = surface.data();
                    if (!raw)
                        return; // surface was deleted before callback
                    int idx = m_pendingAppIdResolveToplevels.indexOf(raw);
                    if (idx < 0)
                        return; // removed before callback
                    ensureXdgWrapper(raw, appId);
                    m_pendingAppIdResolveToplevels.removeAt(idx);
                });
            if (started) {
                qCDebug(lcTlShell) << "AppIdResolver request sent (callback) pidfd=" << pidfd;
                return; // async path handles creation
            } else {
                int idx = m_pendingAppIdResolveToplevels.indexOf(surface);
                if (idx >= 0)
                    m_pendingAppIdResolveToplevels.removeAt(idx);
                qCDebug(lcTlShell)
                    << "AppIdResolverManager present but requestResolve failed pidfd=" << pidfd;
            }
        }
    }
    // Async resolve not started or failed -> directly match or create
    ensureXdgWrapper(surface, QString());
}

bool ShellHandler::configureInitialXdgMaximize(WXdgToplevelSurface *surface)
{
    if (!surface || !surface->isInitialized() || !surface->isMaximizeRequested()
        || !surface->hasCapability(WToplevelSurface::Capability::Maximized)) {
        return false;
    }

    SurfaceWrapper *wrapper = m_rootSurfaceContainer->getSurface(surface);
    Output *output = wrapper ? wrapper->ownsOutput() : nullptr;
    if (!output) {
        if (auto *parentSurface = surface->parentSurface()) {
            if (auto *parentWrapper = m_rootSurfaceContainer->getSurface(parentSurface))
                output = parentWrapper->ownsOutput();
        }
    }
    if (!output)
        output = m_rootSurfaceContainer->primaryOutput();
    if (!output)
        return false;

    QRectF targetGeometry = output->validGeometry();
    if (!targetGeometry.isValid() || targetGeometry.isEmpty())
        return false;

    QSize configureSize = targetGeometry.size().toSize();
    QSize clippedSize;
    if (!surface->checkNewSize(configureSize, &clippedSize))
        configureSize = clippedSize;
    if (!configureSize.isValid() || configureSize.isEmpty())
        return false;
    targetGeometry.setSize(configureSize);

    // Waylib has already scheduled the 0x0 fallback on this event-loop turn. wlroots keeps one
    // idle configure, so these size and state updates replace that fallback atomically.
    bool configured = true;
    if (wrapper)
        configured = wrapper->resize(targetGeometry.size());
    else
        surface->resize(configureSize);
    if (!configured)
        return false;

    surface->setMaximize(true);
    if (wrapper) {
        wrapper->adoptInitialXdgMaximize(targetGeometry);
    } else {
        m_pendingInitialXdgMaximizeGeometries.insert(surface, targetGeometry);
    }

    qCDebug(lcTlShell) << "Configured initial XDG maximize for" << surface->appId() << "target"
                       << targetGeometry << "wrapperReady" << bool(wrapper);
    return true;
}

void ShellHandler::cancelPendingInitialXdgMaximize(WXdgToplevelSurface *surface)
{
    const auto it = m_pendingInitialXdgMaximizeGeometries.find(surface);
    if (it == m_pendingInitialXdgMaximizeGeometries.end())
        return;

    m_pendingInitialXdgMaximizeGeometries.erase(it);
    if (surface->isInitialized()) {
        surface->resize(QSize());
        surface->setMaximize(false);
    }
    qCDebug(lcTlShell) << "Cancelled pending initial XDG maximize for" << surface->appId();
}

void ShellHandler::ensureXdgWrapper(WXdgToplevelSurface *surface, const QString &targetAppId)
{
    const QRectF initialMaximizedGeometry =
        m_pendingInitialXdgMaximizeGeometries.take(surface);

    if (!targetAppId.isEmpty()) {
        const bool wasPending = m_pendingPrelaunchAppIds.remove(targetAppId);
        if (wasPending && !m_unmatchedPrelaunchAppIds.contains(targetAppId))
            rememberUnmatchedPrelaunchAppId(targetAppId);
    }

    // Check if this matches a closed splash screen
    if (!targetAppId.isEmpty() && m_closedSplashAppIds.contains(targetAppId)) {
        qCInfo(lcTlShell) << "XDG surface matches closed splash, closing immediately: appId="
                              << targetAppId;
        m_closedSplashAppIds.remove(targetAppId);
        surface->close();
        return;
    }

    SurfaceWrapper *wrapper = nullptr;
    bool isNewWrapper = true;

    if (!targetAppId.isEmpty()) {
        for (int i = 0; i < m_prelaunchWrappers.size(); ++i) {
            auto *candidate = m_prelaunchWrappers[i];
            if (candidate->appId() == targetAppId) {
                qCDebug(lcTlShell) << "match prelaunch xdg" << targetAppId;
                m_prelaunchWrappers.removeAt(i);
                candidate->convertToNormalSurface(surface,
                                                  SurfaceWrapper::Type::XdgToplevel,
                                                  initialMaximizedGeometry);
                wrapper = candidate;
                isNewWrapper = false; // matched from prelaunch, not newly created
                break;
            }
        }
    }

    if (!wrapper) {
        wrapper = new SurfaceWrapper(Helper::instance()->qmlEngine(),
                                     surface,
                                     SurfaceWrapper::Type::XdgToplevel,
                                     targetAppId,
                                     nullptr,
                                     initialMaximizedGeometry);
        if (!surface->parentSurface())
            seedUnmatchedPrelaunchLastSize(targetAppId, wrapper);
        m_workspace->addSurface(wrapper);
        isNewWrapper = true; // newly created
    }

    // Initialize wrapper
    if (DDEShellSurfaceInterface::get(surface->surface())) {
        handleDdeShellSurfaceAdded(surface->surface(), wrapper);
    }
    auto updateSurfaceWithParentContainer = [this, wrapper, surface] {
        updateWrapperContainer(wrapper, surface->parentSurface());
    };

    surface->safeConnect(&WXdgToplevelSurface::parentXdgSurfaceChanged,
                         this,
                         updateSurfaceWithParentContainer);
    updateSurfaceWithParentContainer();
    Q_ASSERT(wrapper->parentItem());
    setupSurfaceWindowMenu(wrapper);
    // Only setup active watcher for newly created wrappers;
    // prelaunch splash wrappers already have it set up in createPrelaunchSplash
    if (isNewWrapper) {
        setupSurfaceActiveWatcher(wrapper);
        registerSurfaceToForeignToplevel(wrapper);
    }
    Q_EMIT surfaceWrapperAdded(wrapper);

    // IM candidate panel detection via xdg-toplevel-tag
    if (m_imCandidatePanelManager) {
        QPointer<SurfaceWrapper> wrapperPtr(wrapper);
        surface->safeConnect(&WXdgToplevelSurface::tagChanged, this, [this, surface, wrapperPtr]() {
            if (wrapperPtr)
                m_imCandidatePanelManager->checkAndApplyIMCandidatePanel(wrapperPtr, surface);
        });
    }
}

void ShellHandler::onXdgToplevelSurfaceRemoved(WXdgToplevelSurface *surface)
{
    m_pendingInitialXdgMaximizeGeometries.remove(surface);
    auto wrapper = m_rootSurfaceContainer->getSurface(surface);
    // If async resolve still pending, cancel it. If wrapper never created, just return: compositor
    // never exposed this surface (from treeland's perspective).
    if (!wrapper) {
        if (!m_pendingAppIdResolveToplevels.removeOne(surface)) {
            qCWarning(lcTlShell)
                << "onXdgToplevelSurfaceRemoved for unknown surface" << surface;
        }
        return;
    }
    auto interface = DDEShellSurfaceInterface::get(surface->surface());
    if (interface) {
        delete interface;
    }
    // Persist only geometry observed from the real client. A prelaunch splash or a maximized
    // presentation geometry is not a valid restore size.
    if (m_windowConfigStore && !wrapper->appId().isEmpty()
        && wrapper->hasReliableNormalGeometry()) {
        const QSize s = wrapper->normalGeometry().size().toSize();
        if (s.isValid() && s.width() > 0 && s.height() > 0) {
            m_windowConfigStore->saveLastSize(wrapper->appId(), s);
        }
    }
    Q_EMIT surfaceWrapperAboutToRemove(wrapper);
    m_rootSurfaceContainer->destroyForSurface(wrapper);
}

void ShellHandler::onXdgPopupSurfaceAdded(WXdgPopupSurface *surface)
{
    auto wrapper = new SurfaceWrapper(Helper::instance()->qmlEngine(),
                                      surface,
                                      SurfaceWrapper::Type::XdgPopup);

    auto parent = surface->parentSurface();
    auto parentWrapper = m_rootSurfaceContainer->getSurface(parent);
    parentWrapper->addSubSurface(wrapper);

    // When the `parent` is in a specific `container`. for example, when the parent's
    // 'container' has a higher z-index than the `popup`, it is necessary to add the `popup`
    // to the parent's `container` to ensure that the `popup` displays above it's `parent`.
    QQuickItem *ancestorContainer = parentWrapper->container();
    while (ancestorContainer && ancestorContainer->parentItem() != m_popupContainer->parentItem()) {
        ancestorContainer = ancestorContainer->parentItem();
    }
    if (ancestorContainer && ancestorContainer->z() > m_popupContainer->z()) {
        // Set the z-index of the popup itself to ensure it is higher
        // than other types of windows within the same container
        wrapper->setZ(5);
        parentWrapper->container()->addSurface(wrapper);
    } else {
        m_popupContainer->addSurface(wrapper);
    }
    // m_popupContainer is a Simple `SurfaceContainer`
    // Need to call `setHasInitializeContainer` manually
    wrapper->setHasInitializeContainer(true);
    wrapper->setOwnsOutput(parentWrapper->ownsOutput());
    setupSurfaceActiveWatcher(wrapper);

    Q_ASSERT(wrapper->parentItem());
    Q_EMIT surfaceWrapperAdded(wrapper);
}

void ShellHandler::onXdgPopupSurfaceRemoved(WXdgPopupSurface *surface)
{
    auto wrapper = m_rootSurfaceContainer->getSurface(surface);
    Q_EMIT surfaceWrapperAboutToRemove(wrapper);
    wrapper->setHasInitializeContainer(false);
    m_rootSurfaceContainer->destroyForSurface(wrapper);
}

void ShellHandler::onXWaylandSurfaceAdded(WXWaylandSurface *surface)
{
    surface->safeConnect(&WXWaylandSurface::associated,
                         this,
                         [this, surface = QPointer<WXWaylandSurface>(surface)] {
                             auto raw = surface.data();
                             if (!raw)
                                 return; // surface destroyed before callback
                             // If prelaunch identities exist and the resolver is available,
                             // attempt async resolve; if started, remaining logic is handled in
                             // the callback.
                             if (hasPrelaunchAppIdCandidates() && m_appIdResolverManager) {
                                 int pidfd = raw->pidFD();
                                 if (pidfd >= 0) {
                                     m_pendingAppIdResolveToplevels.append(raw);
                                     bool started = m_appIdResolverManager->resolvePidfd(
                                         pidfd,
                                         [self = QPointer<ShellHandler>(this),
                                          surface](const QString &appId) {
                                             auto raw = surface.data();
                                             if (!raw || !self)
                                                 return; // surface destroyed before callback
                                             int idx =
                                                 self->m_pendingAppIdResolveToplevels.indexOf(raw);
                                             if (idx < 0)
                                                 return; // removed before callback
                                             self->fetchInitialProperties(raw, appId);
                                             self->m_pendingAppIdResolveToplevels.removeAt(idx);
                                         });
                                     if (started) {
                                         qCDebug(lcTlShell)
                                             << "(XWayland) AppIdResolver request sent (callback)";
                                         return; // async path
                                     } else {
                                         int idx = m_pendingAppIdResolveToplevels.indexOf(raw);
                                         if (idx >= 0)
                                             m_pendingAppIdResolveToplevels.removeAt(idx);
                                         qCDebug(lcTlShell)
                                             << "(XWayland) requestResolve failed pidfd=" << pidfd;
                                     }
                                 }
                             }
                             // Async path not taken: directly fetch properties then match/create
                             fetchInitialProperties(raw, QString());
                         });
    surface->safeConnect(&WXWaylandSurface::aboutToDissociate, this, [this, surface] {
        auto wrapper = m_rootSurfaceContainer->getSurface(surface);
        qCDebug(lcTlShell) << "WXWayland::aboutToDissociate" << surface << wrapper;

        // Cancel pending async property fetch for this surface.
        auto *xwayland = surface->xwayland();
        if (xwayland) {
            auto windowId = surface->handle()->handle()->window_id;
            xwayland->cancelAsyncProperties(windowId);
        }

        // Cancel pending async resolve if still present. If wrapper never created, return.
        if (!wrapper) {
            if (!m_pendingAppIdResolveToplevels.removeOne(surface)) {
                qCWarning(lcTlShell)
                    << "WXWayland::aboutToDissociate for unknown surface" << surface;
            }
            return; // never created
        }
        // Only task-level XWayland windows may update the per-app restore size. Utility and child
        // windows frequently share the same appId and must not overwrite the main window.
        if (m_windowConfigStore && !wrapper->appId().isEmpty() && surface->isToplevel()
            && !wrapper->skipDockPreView() && wrapper->hasReliableNormalGeometry()) {
            const QSize s = wrapper->normalGeometry().size().toSize();
            if (s.isValid() && s.width() > 0 && s.height() > 0) {
                m_windowConfigStore->saveLastSize(wrapper->appId(), s);
            }
        }
        Q_EMIT surfaceWrapperAboutToRemove(wrapper);
        m_rootSurfaceContainer->destroyForSurface(wrapper);
    });
}

void ShellHandler::fetchInitialProperties(WXWaylandSurface *surface, const QString &appId)
{
    auto *xwayland = surface->xwayland();
    if (!xwayland) {
        ensureXwaylandWrapper(surface, appId);
        return;
    }

    auto windowId = surface->handle()->handle()->window_id;
    QVector<WXWayland::AsyncPropRequest> requests;
    if (m_imCandidatePanelManager) {
        requests.append({ m_imCandidatePanelManager->imCandidatePanelAtom(), XCB_ATOM_CARDINAL });
    }

    if (requests.isEmpty()) {
        ensureXwaylandWrapper(surface, appId);
        return;
    }

    xwayland->readAsyncProperties(
        windowId,
        requests,
        50,
        [self = QPointer<ShellHandler>(this),
         surface = QPointer<WXWaylandSurface>(surface),
         appId](xcb_window_t, const QMap<xcb_atom_t, QByteArray> &result) {
            auto *raw = surface.data();
            if (!raw || !self)
                return;
            self->onInitialPropertiesReady(raw, appId, result);
        });
}

void ShellHandler::onInitialPropertiesReady(WXWaylandSurface *surface,
                                            const QString &appId,
                                            const QMap<xcb_atom_t, QByteArray> &result)
{
    if (m_imCandidatePanelManager) {
        bool value = IMCandidatePanelManager::parseIMCandidatePanelProperty(
            result,
            m_imCandidatePanelManager->imCandidatePanelAtom());
        surface->setProperty("imCandidatePanel", value);
    }
    ensureXwaylandWrapper(surface, appId);
}

void ShellHandler::ensureXwaylandWrapper(WXWaylandSurface *surface, const QString &targetAppId)
{
    if (!targetAppId.isEmpty()) {
        const bool wasPending = m_pendingPrelaunchAppIds.remove(targetAppId);
        if (wasPending && !m_unmatchedPrelaunchAppIds.contains(targetAppId))
            rememberUnmatchedPrelaunchAppId(targetAppId);
    }

    // Check if this matches a closed splash screen
    if (!targetAppId.isEmpty() && m_closedSplashAppIds.contains(targetAppId)) {
        qCDebug(lcTlShell)
            << "XWayland surface matches closed splash, closing immediately: appId=" << targetAppId;
        m_closedSplashAppIds.remove(targetAppId);
        surface->close();
        return;
    }

    SurfaceWrapper *wrapper = nullptr;
    bool isNewWrapper = true;

    if (!targetAppId.isEmpty()) {
        for (int i = 0; i < m_prelaunchWrappers.size(); ++i) {
            auto *candidate = m_prelaunchWrappers[i];
            if (candidate->appId() == targetAppId) {
                qCDebug(lcTlShell) << "match prelaunch xwayland" << targetAppId;
                m_prelaunchWrappers.removeAt(i);
                candidate->convertToNormalSurface(surface, SurfaceWrapper::Type::XWayland);
                wrapper = candidate;
                isNewWrapper = false; // matched from prelaunch, not newly created
                break;
            }
        }
    }

    if (!wrapper) {
        wrapper = new SurfaceWrapper(Helper::instance()->qmlEngine(),
                                     surface,
                                     SurfaceWrapper::Type::XWayland,
                                     targetAppId);
        if (surface->isToplevel() && !wrapper->skipDockPreView())
            seedUnmatchedPrelaunchLastSize(targetAppId, wrapper);
        m_workspace->addSurface(wrapper);
        isNewWrapper = true; // newly created
    }

    // IM candidate panel detection via XWayland xprop
    if (m_imCandidatePanelManager
        && m_imCandidatePanelManager->checkAndApplyIMCandidatePanel(wrapper, surface)) {
        Q_EMIT surfaceWrapperAdded(wrapper);
        return;
    }

    // Initialize wrapper
    auto updateSurfaceWithParentContainer = [this, wrapper, surface] {
        updateWrapperContainer(wrapper, surface->parentSurface());
    };
    surface->safeConnect(&WXWaylandSurface::parentSurfaceChanged,
                         this,
                         updateSurfaceWithParentContainer);
    updateSurfaceWithParentContainer();
    Q_ASSERT(wrapper->parentItem());
    setupSurfaceWindowMenu(wrapper);
    // Only setup active watcher for newly created wrappers;
    // prelaunch splash wrappers already have it set up in createPrelaunchSplash
    if (isNewWrapper) {
        setupSurfaceActiveWatcher(wrapper);
        registerSurfaceToForeignToplevel(wrapper);
    }
    Q_EMIT surfaceWrapperAdded(wrapper);
}

void ShellHandler::registerSurfaceToForeignToplevel(SurfaceWrapper *wrapper)
{
    if (!wrapper->skipDockPreView()) {
        m_treelandForeignToplevel->addSurface(wrapper);
    }
    connect(wrapper, &SurfaceWrapper::skipDockPreViewChanged, this, [this, wrapper] {
        if (wrapper->skipDockPreView()) {
            m_treelandForeignToplevel->removeSurface(wrapper);
        } else {
            m_treelandForeignToplevel->addSurface(wrapper);
        }
    });
}

void ShellHandler::setupDockPreview()
{
    Q_ASSERT(m_dockPreview);

    connect(m_treelandForeignToplevel,
            &ForeignToplevelManagerInterfaceV1::requestDockPreview,
            this,
            &ShellHandler::onDockPreview);
    connect(m_treelandForeignToplevel,
            &ForeignToplevelManagerInterfaceV1::requestDockPreviewTooltip,
            this,
            &ShellHandler::onDockPreviewTooltip);
    connect(m_treelandForeignToplevel,
            &ForeignToplevelManagerInterfaceV1::requestDockClose,
            m_dockPreview,
            [this]() {
                QMetaObject::invokeMethod(m_dockPreview, "close");
            });
}

void ShellHandler::onDockPreview(std::vector<SurfaceWrapper *> surfaces,
                                 WSurface *target,
                                 QPoint pos,
                                 ForeignToplevelManagerInterfaceV1::PreviewDirection direction)
{
    if (!m_dockPreview)
        return;

    SurfaceWrapper *dockWrapper = m_rootSurfaceContainer->getSurface(target);
    Q_ASSERT(dockWrapper);

    QMetaObject::invokeMethod(m_dockPreview,
                              "show",
                              QVariant::fromValue(surfaces),
                              QVariant::fromValue(dockWrapper),
                              QVariant::fromValue(pos),
                              QVariant::fromValue(direction));
}

void ShellHandler::onDockPreviewTooltip(
    QString tooltip,
    WSurface *target,
    QPoint pos,
    ForeignToplevelManagerInterfaceV1::PreviewDirection direction)
{
    if (!m_dockPreview)
        return;

    SurfaceWrapper *dockWrapper = m_rootSurfaceContainer->getSurface(target);
    Q_ASSERT(dockWrapper);
    QMetaObject::invokeMethod(m_dockPreview,
                              "showTooltip",
                              QVariant::fromValue(tooltip),
                              QVariant::fromValue(dockWrapper),
                              QVariant::fromValue(pos),
                              QVariant::fromValue(direction));
}

void ShellHandler::onSurfaceInactivationRequested(SurfaceWrapper *wrapper)
{
    Q_ASSERT(wrapper);
    if (wrapper->type() != SurfaceWrapper::Type::Layer)
        m_workspace->removeActivedSurface(wrapper);

    auto *helper = Helper::instance();
    auto *seatManager = helper->seatManager();
    WSeat *primarySeat = helper->seat();
    const auto seats = seatManager->seats();
    auto *container = helper->rootSurfaceContainer();

    for (auto *seat : seats) {
        auto *seatContainer = container->getSeatContainer(seat);
        const bool isKeyboardFocusOwner = seatContainer->keyboardFocusSurface() == wrapper;
        const bool isActivatedOwner = seatContainer->activatedSurface() == wrapper;

        if (isKeyboardFocusOwner) {
            // activateSurface implicitly clears the old activated state and falls back focus.
            // TODO(multi-seat): non-primary seats should also perform focus fallback once
            // per-seat workspace stacking is supported.
            if (seat == primarySeat) {
                helper->activateSurface(m_workspace->current()->latestActiveSurface(),
                                        Qt::OtherFocusReason,
                                        seat);
            } else {
                helper->requestKeyboardFocus(nullptr, Qt::OtherFocusReason, seat);
            }
            continue;
        }

        if (isActivatedOwner) { // not isKeyboardFocusOwner
            // Clear the activated state so foreign-toplevel clients do not observe a
            // minimized-but-activated window.
            helper->setActivatedSurface(nullptr, seat);
        }
    }
}

void ShellHandler::setupSurfaceActiveWatcher(SurfaceWrapper *wrapper)
{
    Q_ASSERT_X(wrapper->container(), Q_FUNC_INFO, "Must setContainer at first!");

    if (wrapper->type() == SurfaceWrapper::Type::XdgPopup) {
        // When the popup surface gains focus capability (mapped + grab active),
        // move keyboard focus to the popup. SeatSurfaceManager handles
        // grab tracking and focus restoration when the grab ends.
        connect(wrapper, &SurfaceWrapper::hasFocusCapabilityChanged, this, [this, wrapper]() {
            if (!wrapper->hasFocusCapability())
                return;
            m_rootSurfaceContainer->givePopupFocus(wrapper);
        });
    } else if (wrapper->type() == SurfaceWrapper::Type::Layer) {
        connect(wrapper, &SurfaceWrapper::hasFocusCapabilityChanged, this, [this, wrapper]() {
            if (wrapper->hasFocusCapability()) {
                auto layerSurface = qobject_cast<WLayerSurface *>(wrapper->shellSurface());
                Q_ASSERT(layerSurface->keyboardInteractivity()
                         != WLayerSurface::KeyboardInteractivity::None);
                /*
                 * For OnDemand keyboardInteractivity, only allow surfaces with z-order above
                 * normal windows (Top/Overlay) to receive keyboard focus, to avoid dock/dde-desktop
                 * grabbing focus when they restart.
                 */
                if (layerSurface->layer() >= WLayerSurface::LayerType::Top
                    || layerSurface->keyboardInteractivity()
                        == WLayerSurface::KeyboardInteractivity::Exclusive)
                    Helper::instance()->requestKeyboardFocus(wrapper);
            } else {
                onSurfaceInactivationRequested(wrapper);
            }
        });
    } else { // Xdgtoplevel or X11 or Splash
        connect(wrapper, &SurfaceWrapper::activationRequested, this, [this, wrapper]() {
            if (wrapper->showOnWorkspace(m_workspace->current()->id()))
                Helper::instance()->activateSurface(wrapper);
            else
                m_workspace->pushActivedSurface(wrapper);
        });

        connect(wrapper, &SurfaceWrapper::inactivationRequested, this, [this, wrapper]() {
            onSurfaceInactivationRequested(wrapper);
        });

        if (wrapper->hasActiveCapability()) {
            if (wrapper->showOnWorkspace(m_workspace->current()->id()))
                Helper::instance()->activateSurface(wrapper);
            else
                m_workspace->pushActivedSurface(wrapper);
        }
    }
}

void ShellHandler::onLayerSurfaceAdded(WLayerSurface *surface)
{
    if (!surface->output() && !m_rootSurfaceContainer->primaryOutput()) {
        qCWarning(lcTlShell) << "No output, will close layer surface!";
        surface->closed();
        return;
    }
    auto wrapper =
        new SurfaceWrapper(Helper::instance()->qmlEngine(), surface, SurfaceWrapper::Type::Layer);

    wrapper->setSkipSwitcher(true);
    wrapper->setSkipMutiTaskView(true);
    updateLayerSurfaceContainer(wrapper);

    connect(surface, &WLayerSurface::layerChanged, this, [this, wrapper] {
        updateLayerSurfaceContainer(wrapper);
    });

    setupSurfaceActiveWatcher(wrapper);
    Q_ASSERT(wrapper->parentItem());
    Q_EMIT surfaceWrapperAdded(wrapper);
}

void ShellHandler::onLayerSurfaceRemoved(WLayerSurface *surface)
{
    auto wrapper = m_rootSurfaceContainer->getSurface(surface->surface());
    if (!wrapper) {
        qCWarning(lcTlShell) << "A layerSurface that not in any Container is removing!";
        return;
    }
    Q_EMIT surfaceWrapperAboutToRemove(wrapper);
    m_rootSurfaceContainer->destroyForSurface(wrapper);
}

void ShellHandler::updateLayerSurfaceContainer(SurfaceWrapper *surface)
{
    auto layer = qobject_cast<WLayerSurface *>(surface->shellSurface());
    Q_ASSERT(layer);

    if (auto oldContainer = surface->container())
        oldContainer->removeSurface(surface);

    switch (layer->layer()) {
    case WLayerSurface::LayerType::Background:
        m_backgroundContainer->addSurface(surface);
        break;
    case WLayerSurface::LayerType::Bottom:
        m_bottomContainer->addSurface(surface);
        break;
    case WLayerSurface::LayerType::Top:
        m_topContainer->addSurface(surface);
        break;
    case WLayerSurface::LayerType::Overlay:
        m_overlayContainer->addSurface(surface);
        break;
    default:
        Q_UNREACHABLE_RETURN();
    }
}

void ShellHandler::onInputPopupSurfaceV2Added(WInputPopupSurface *surface)
{
    auto wrapper = new SurfaceWrapper(Helper::instance()->qmlEngine(),
                                      surface,
                                      SurfaceWrapper::Type::InputPopup);
    auto parent = surface->parentSurface();
    auto parentWrapper = m_rootSurfaceContainer->getSurface(parent);
    parentWrapper->addSubSurface(wrapper);
    m_popupContainer->addSurface(wrapper);
    // m_popupContainer is a simple SurfaceContainer, so input popups need the
    // same explicit initialization marker as xdg popups for their first layout.
    wrapper->setHasInitializeContainer(true);
    wrapper->setOwnsOutput(parentWrapper->ownsOutput());
    Q_ASSERT(wrapper->parentItem());
    Q_EMIT surfaceWrapperAdded(wrapper);
}

void ShellHandler::onInputPopupSurfaceV2Removed(WInputPopupSurface *surface)
{
    auto wrapper = m_rootSurfaceContainer->getSurface(surface->surface());
    Q_EMIT surfaceWrapperAboutToRemove(wrapper);
    wrapper->setHasInitializeContainer(false);
    m_rootSurfaceContainer->destroyForSurface(wrapper);
}

void ShellHandler::setupSurfaceWindowMenu(SurfaceWrapper *wrapper)
{
    Q_ASSERT(m_windowMenu);
    connect(wrapper,
            &SurfaceWrapper::windowMenuRequested,
            m_windowMenu,
            [this, wrapper](QPointF pos) {
                bool ok = QMetaObject::invokeMethod(m_windowMenu,
                                          "showWindowMenu",
                                          QVariant::fromValue(wrapper),
                                          QVariant::fromValue(pos));
                qCDebug(lcTlShortcut) << "showWindowMenu invokeMethod result=" << ok;
            });
}

void ShellHandler::onWindowMenuClosed()
{
    Helper::instance()->activateSurface(m_workspace->current()->latestActiveSurface());
}

void ShellHandler::handleDdeShellSurfaceAdded(WSurface *surface, SurfaceWrapper *wrapper)
{
    wrapper->setIsDDEShellSurface(true);
    auto ddeShellSurface = DDEShellSurfaceInterface::get(surface);
    Q_ASSERT(ddeShellSurface);
    auto updateLayer = [ddeShellSurface, wrapper] {
        if (ddeShellSurface->role().value() == DDEShellSurfaceInterface::OVERLAY)
            wrapper->setSurfaceRole(SurfaceWrapper::SurfaceRole::Overlay);
    };

    if (ddeShellSurface->role().has_value())
        updateLayer();

    connect(ddeShellSurface, &DDEShellSurfaceInterface::roleChanged, this, [updateLayer] {
        updateLayer();
    });

    if (ddeShellSurface->yOffset().has_value())
        wrapper->setAutoPlaceYOffset(ddeShellSurface->yOffset().value());

    connect(ddeShellSurface,
            &DDEShellSurfaceInterface::yOffsetChanged,
            this,
            [wrapper](uint32_t offset) {
                wrapper->setAutoPlaceYOffset(offset);
            });

    if (ddeShellSurface->surfacePos().has_value())
        wrapper->setClientRequstPos(ddeShellSurface->surfacePos().value());

    connect(ddeShellSurface,
            &DDEShellSurfaceInterface::positionChanged,
            this,
            [wrapper](QPoint pos) {
                wrapper->setClientRequstPos(pos);
            });

    if (ddeShellSurface->skipSwitcher().has_value())
        wrapper->setSkipSwitcher(ddeShellSurface->skipSwitcher().value());

    if (ddeShellSurface->skipDockPreView().has_value())
        wrapper->setSkipDockPreView(ddeShellSurface->skipDockPreView().value());

    if (ddeShellSurface->skipMutiTaskView().has_value())
        wrapper->setSkipMutiTaskView(ddeShellSurface->skipMutiTaskView().value());

    wrapper->setAcceptKeyboardFocus(ddeShellSurface->acceptKeyboardFocus());

    connect(ddeShellSurface,
            &DDEShellSurfaceInterface::skipSwitcherChanged,
            this,
            [wrapper](bool skip) {
                wrapper->setSkipSwitcher(skip);
            });
    connect(ddeShellSurface,
            &DDEShellSurfaceInterface::skipDockPreViewChanged,
            this,
            [wrapper](bool skip) {
                wrapper->setSkipDockPreView(skip);
            });
    connect(ddeShellSurface,
            &DDEShellSurfaceInterface::skipMutiTaskViewChanged,
            this,
            [wrapper](bool skip) {
                wrapper->setSkipMutiTaskView(skip);
            });
    connect(ddeShellSurface,
            &DDEShellSurfaceInterface::acceptKeyboardFocusChanged,
            this,
            [wrapper](bool accept) {
                wrapper->setAcceptKeyboardFocus(accept);
            });
}

void ShellHandler::setResourceManagerAtom(WAYLIB_SERVER_NAMESPACE::WXWayland *xwayland,
                                          const QByteArray &value)
{
    auto xcb_conn = xwayland->xcbConnection();
    auto root = xwayland->xcbScreen()->root;
    xcb_change_property(xcb_conn,
                        XCB_PROP_MODE_REPLACE,
                        root,
                        xwayland->atom("RESOURCE_MANAGER"),
                        XCB_ATOM_STRING,
                        8,
                        value.size(),
                        value.constData());
    xcb_flush(xcb_conn);
}
