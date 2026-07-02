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
#include "rootsurfacecontainer.h"
#include "seat/helper.h"
#include "session/session.h"
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
                               m_prelaunchWrappers.removeAt(idx);
                               m_rootSurfaceContainer->destroyForSurface(wrapper);
                           });
    }
}

void ShellHandler::handlePrelaunchSplashClosed(const QString &appId, const QString &instanceId)
{
    Q_UNUSED(instanceId); // TODO: will be provided by AM DBus in future

    // Remove pending prelaunch request if it hasn't created a wrapper yet
    m_pendingPrelaunchAppIds.remove(appId);

    // Find and destroy any existing prelaunch wrapper with the matching appId
    for (int i = 0; i < m_prelaunchWrappers.size(); ++i) {
        auto *wrapper = m_prelaunchWrappers[i];
        if (wrapper->appId() == appId) {
            qCDebug(lcTlShell)
                << "Client requested close_splash, destroy wrapper appId=" << appId;
            m_prelaunchWrappers.removeAt(i);
            m_rootSurfaceContainer->destroyForSurface(wrapper);
            return;
        }
    }
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
    // If there are prelaunch wrappers or closed splash appIds and the resolver is available
    // -> attempt async resolve; remaining logic continues in the callback on success
    if ((!m_prelaunchWrappers.isEmpty() || !m_closedSplashAppIds.isEmpty())
        && m_appIdResolverManager) {
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

void ShellHandler::ensureXdgWrapper(WXdgToplevelSurface *surface, const QString &targetAppId)
{
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
        m_pendingPrelaunchAppIds.remove(targetAppId);
        for (int i = 0; i < m_prelaunchWrappers.size(); ++i) {
            auto *candidate = m_prelaunchWrappers[i];
            if (candidate->appId() == targetAppId) {
                qCDebug(lcTlShell) << "match prelaunch xdg" << targetAppId;
                m_prelaunchWrappers.removeAt(i);
                candidate->convertToNormalSurface(surface, SurfaceWrapper::Type::XdgToplevel);
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
                                     targetAppId);
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
    // Persist the last size of a normal window (prefer normalGeometry) when an appId is present
    if (m_windowConfigStore && !wrapper->appId().isEmpty()) {
        QSizeF sz = wrapper->normalGeometry().size();
        if (!sz.isValid() || sz.isEmpty()) {
            sz = wrapper->geometry().size();
        }
        const QSize s = sz.toSize();
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
                             // If prelaunch wrappers or closed splash appIds exist and resolver is
                             // available, attempt async resolve; if started, remaining logic
                             // handled in callback, then return
                             if ((!m_prelaunchWrappers.isEmpty() || !m_closedSplashAppIds.isEmpty())
                                 && m_appIdResolverManager) {
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
                                             self->m_pendingAppIdResolveToplevels.removeAt(idx);
                                             self->ensureXwaylandWrapper(raw, appId);
                                             if (auto current = surface.data())
                                                 self->fetchInitialProperties(current);
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
                             // Async path not taken: create/match first so the wrapper observes
                             // the initial map lifecycle, then fetch optional xprops.
                             ensureXwaylandWrapper(raw, QString());
                             fetchInitialProperties(raw);
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
        // Persist XWayland window size
        if (m_windowConfigStore && !wrapper->appId().isEmpty()) {
            QSizeF sz = wrapper->normalGeometry().size();
            if (!sz.isValid() || sz.isEmpty()) {
                sz = wrapper->geometry().size();
            }
            const QSize s = sz.toSize();
            if (s.isValid() && s.width() > 0 && s.height() > 0) {
                m_windowConfigStore->saveLastSize(wrapper->appId(), s);
            }
        }
        Q_EMIT surfaceWrapperAboutToRemove(wrapper);
        m_rootSurfaceContainer->destroyForSurface(wrapper);
    });
}

void ShellHandler::fetchInitialProperties(WXWaylandSurface *surface)
{
    if (!m_imCandidatePanelManager)
        return;

    auto *wrapper = m_rootSurfaceContainer->getSurface(surface);
    if (!wrapper || wrapper->isIMCandidatePanel())
        return;

    auto *xwayland = surface->xwayland();
    if (!xwayland)
        return;

    auto windowId = surface->handle()->handle()->window_id;
    auto atom = m_imCandidatePanelManager->imCandidatePanelAtom();
    if (atom == XCB_ATOM_NONE)
        return;

    QVector<WXWayland::AsyncPropRequest> requests;
    requests.append({ atom, XCB_ATOM_CARDINAL });

    xwayland->readAsyncProperties(
        windowId,
        requests,
        50,
        [self = QPointer<ShellHandler>(this),
         surface = QPointer<WXWaylandSurface>(surface)](xcb_window_t,
                                                        const QMap<xcb_atom_t, QByteArray> &result) {
            auto *raw = surface.data();
            if (!raw || !self)
                return;
            self->onInitialPropertiesReady(raw, result);
        });
}

void ShellHandler::onInitialPropertiesReady(WXWaylandSurface *surface,
                                            const QMap<xcb_atom_t, QByteArray> &result)
{
    if (m_imCandidatePanelManager) {
        bool value = IMCandidatePanelManager::parseIMCandidatePanelProperty(
            result,
            m_imCandidatePanelManager->imCandidatePanelAtom());
        surface->setProperty("imCandidatePanel", value);
        auto *wrapper = m_rootSurfaceContainer->getSurface(surface);
        if (wrapper)
            m_imCandidatePanelManager->checkAndApplyIMCandidatePanel(wrapper, surface);
    }
}

void ShellHandler::ensureXwaylandWrapper(WXWaylandSurface *surface, const QString &targetAppId)
{
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
        m_pendingPrelaunchAppIds.remove(targetAppId);
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

void ShellHandler::setupSurfaceActiveWatcher(SurfaceWrapper *wrapper)
{
    Q_ASSERT_X(wrapper->container(), Q_FUNC_INFO, "Must setContainer at first!");

    if (wrapper->type() == SurfaceWrapper::Type::XdgPopup) {
        connect(wrapper, &SurfaceWrapper::activationRequested, this, [this, wrapper]() {
            auto parent = wrapper->parentSurface();
            while (parent->type() == SurfaceWrapper::Type::XdgPopup) {
                parent = parent->parentSurface();
            }
            if (!parent) {
                qCCritical(lcTlShell) << "A new popup without toplevel parent!";
                return;
            }
            if (!parent->showOnWorkspace(m_workspace->current()->id())) {
                qCWarning(lcTlShell)
                    << "A popup active, but it's parent not in current workspace!";
                return;
            }
            Helper::instance()->activateSurface(parent);
        });

        /*
        connect(wrapper, &SurfaceWrapper::inactivationRequested, this, [this, wrapper]() {
            auto parent = wrapper->parentSurface();
            if (!parent || parent->type() == SurfaceWrapper::Type::XdgPopup)
                return;
            Helper::instance()->activateSurface(m_workspace->current()->latestActiveSurface());
        });
        */
    } else if (wrapper->type() == SurfaceWrapper::Type::Layer) {
        connect(wrapper, &SurfaceWrapper::activationRequested, this, [wrapper]() {
            auto layerSurface = qobject_cast<WLayerSurface *>(wrapper->shellSurface());
            if (layerSurface->keyboardInteractivity() == WLayerSurface::KeyboardInteractivity::None)
                return;

            /*
             * For OnDemand keyboardInteractivity, only allow surfaces with z-order above
             * normal windows (Top/Overlay) to receive keyboard focus, to avoid dock/dde-desktop
             * grabbing focus when they restart.
             */
            if (layerSurface->layer() >= WLayerSurface::LayerType::Top
                || layerSurface->keyboardInteractivity()
                    == WLayerSurface::KeyboardInteractivity::Exclusive)
                Helper::instance()->activateSurface(wrapper);
        });

        connect(wrapper, &SurfaceWrapper::inactivationRequested, this, [this, wrapper]() {
            // Only the current keyboard focus owner should drive focus fallback.
            // Closing an unfocused layer surface, such as a notification, must not
            // move focus away from the current owner, which may be a layer-shell
            // surface that does not participate in fallback history.
            if (Helper::instance()->keyboardFocusSurface() != wrapper)
                return;

            Helper::instance()->activateSurface(m_workspace->current()->latestActiveSurface());
        });
    } else { // Xdgtoplevel or X11 or Splash
        connect(wrapper, &SurfaceWrapper::activationRequested, this, [this, wrapper]() {
            if (wrapper->showOnWorkspace(m_workspace->current()->id()))
                Helper::instance()->activateSurface(wrapper);
            else
                m_workspace->pushActivedSurface(wrapper);
        });

        connect(wrapper, &SurfaceWrapper::inactivationRequested, this, [this, wrapper]() {
            m_workspace->removeActivedSurface(wrapper);
            // The window may already be inactive. Keep history cleanup above, but
            // avoid changing focus unless this window still owns keyboard focus.
            if (Helper::instance()->keyboardFocusSurface() != wrapper) {
                // Requests from task bars or other external controllers can minimize the
                // active window after keyboard focus has already moved away from it. Clear
                // the compositor active state so foreign-toplevel clients do not observe a
                // minimized-but-activated window, but do not fall back focus from the
                // current keyboard focus owner.
                if (Helper::instance()->activatedSurface() == wrapper)
                    Helper::instance()->setActivatedSurface(nullptr);
                return;
            }

            Helper::instance()->activateSurface(m_workspace->current()->latestActiveSurface());
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
                QMetaObject::invokeMethod(m_windowMenu,
                                          "showWindowMenu",
                                          QVariant::fromValue(wrapper),
                                          QVariant::fromValue(pos));
            });
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
