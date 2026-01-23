// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "shellhandler.h"

#include "common/treelandlogging.h"
#include "core/qmlengine.h"
#include "core/windowconfigstore.h"
#include "layersurfacecontainer.h"
#include "modules/app-id-resolver/appidresolver.h"
#include "modules/dde-shell/ddeshellmanagerinterfacev1.h"
#include "rootsurfacecontainer.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"
#include "treelandconfig.hpp"
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

#include <qwcompositor.h>
#include <qwxwaylandsurface.h>

#include <QPointer>
#include <QTimer>

#include <optional>
#include <algorithm>

QW_USE_NAMESPACE
WAYLIB_SERVER_USE_NAMESPACE

#define TREELAND_XDG_SHELL_VERSION 5

ShellHandler::ShellHandler(RootSurfaceContainer *rootContainer)
    : m_rootSurfaceContainer(rootContainer)
    , m_backgroundContainer(new LayerSurfaceContainer(rootContainer))
    , m_bottomContainer(new LayerSurfaceContainer(rootContainer))
    , m_workspace(new Workspace(rootContainer))
    , m_topContainer(new LayerSurfaceContainer(rootContainer))
    , m_overlayContainer(new LayerSurfaceContainer(rootContainer))
    , m_popupContainer(new SurfaceContainer(rootContainer))
    , m_windowConfigStore(new WindowConfigStore(this))
{
    m_backgroundContainer->setZ(RootSurfaceContainer::BackgroundZOrder);
    m_bottomContainer->setZ(RootSurfaceContainer::BottomZOrder);
    m_workspace->setZ(RootSurfaceContainer::NormalZOrder);
    m_topContainer->setZ(RootSurfaceContainer::TopZOrder);
    m_overlayContainer->setZ(RootSurfaceContainer::OverlayZOrder);
    m_popupContainer->setZ(RootSurfaceContainer::PopupZOrder);
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
                                                  QW_NAMESPACE::qw_buffer *iconBuffer)
{
    if (!Helper::instance()->globalConfig()->enablePrelaunchSplash())
        return;
    if (!m_appIdResolverManager)
        return;
    if (appId.isEmpty())
        return;
    // If a prelaunch wrapper with the same appId already exists, skip creating a duplicate.
    if (std::any_of(m_prelaunchWrappers.cbegin(),
                    m_prelaunchWrappers.cend(),
                    [&appId](SurfaceWrapper *w) { return w && w->appId() == appId; })) {
        return;
    }
    // Flow: add appId to pending list -> wait for dconfig init ->
    // if pending still exists, create splash; otherwise a real window appeared and cleared it.
    if (m_pendingPrelaunchAppIds.contains(appId))
        return;
    m_pendingPrelaunchAppIds.insert(appId);

    auto createSplash = [this, appId, iconBuffer](const QSize &last) {
        if (!m_pendingPrelaunchAppIds.contains(appId))
            return; // app window already created while waiting for dconfig
        m_pendingPrelaunchAppIds.remove(appId);

        if (std::any_of(m_prelaunchWrappers.cbegin(),
                        m_prelaunchWrappers.cend(),
                        [&appId](SurfaceWrapper *w) { return w && w->appId() == appId; })) {
            return;
        }

        QSize initialSize;
        if (last.isValid() && last.width() > 0 && last.height() > 0) {
            initialSize = last;
        }

        auto *wrapper = new SurfaceWrapper(Helper::instance()->qmlEngine(),
                                           nullptr,
                                           initialSize,
                                           appId,
                                           iconBuffer);
        m_workspace->addSurface(wrapper);
        m_prelaunchWrappers.append(wrapper);

        // After configurable timeout, if still unmatched (not converted and still in the list), destroy
        // the splash wrapper
        qlonglong timeoutMs = Helper::instance()->globalConfig()->prelaunchSplashTimeoutMs();
        // Bounds: <=0 disables auto-destroy, >60000 clamps to 60000
        if (timeoutMs > 60000) {
            timeoutMs = 60000;
        }
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
                                   qCDebug(treelandShell)
                                       << "Prelaunch splash timeout, destroy wrapper appId="
                                       << wrapper->appId();
                                   m_prelaunchWrappers.removeAt(idx);
                                   m_rootSurfaceContainer->destroyForSurface(wrapper);
                               });
        }
    };

    if (m_windowConfigStore) {
        m_windowConfigStore->withLastSizeFor(appId, this, createSplash);
    } else {
        createSplash({});
    }
}

Workspace *ShellHandler::workspace() const
{
    return m_workspace;
}

void ShellHandler::createComponent(QmlEngine *engine)
{
    m_windowMenu = engine->createWindowMenu(Helper::instance());
}

void ShellHandler::initXdgShell(WServer *server)
{
    Q_ASSERT_X(!m_xdgShell, Q_FUNC_INFO, "Only init once!");
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
}

void ShellHandler::initLayerShell(WServer *server)
{
    Q_ASSERT_X(!m_layerShell, Q_FUNC_INFO, "Only init once!");
    Q_ASSERT_X(m_xdgShell, Q_FUNC_INFO, "Init xdg shell before layer shell!");
    m_layerShell = server->attach<WLayerShell>(m_xdgShell);
    connect(m_layerShell, &WLayerShell::surfaceAdded, this, &ShellHandler::onLayerSurfaceAdded);
    connect(m_layerShell, &WLayerShell::surfaceRemoved, this, &ShellHandler::onLayerSurfaceRemoved);
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
    connect(xwayland, &WXWayland::ready, xwayland, [xwayland] {
        auto atomPid = xwayland->atom("_NET_WM_PID");
        xwayland->setAtomSupported(atomPid, true);
        auto atomNoTitlebar = xwayland->atom("_DEEPIN_NO_TITLEBAR");
        xwayland->setAtomSupported(atomNoTitlebar, true);
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

void ShellHandler::initInputMethodHelper(WServer *server, WSeat *seat)
{
    Q_ASSERT_X(!m_inputMethodHelper, Q_FUNC_INFO, "Only init once!");
    m_inputMethodHelper = new WInputMethodHelper(server, seat);

    connect(m_inputMethodHelper,
            &WInputMethodHelper::inputPopupSurfaceV2Added,
            this,
            &ShellHandler::onInputPopupSurfaceV2Added);
    connect(m_inputMethodHelper,
            &WInputMethodHelper::inputPopupSurfaceV2Removed,
            this,
            &ShellHandler::onInputPopupSurfaceV2Removed);
}

void ShellHandler::onXdgToplevelSurfaceAdded(WXdgToplevelSurface *surface)
{
    // If there are prelaunch wrappers and the resolver is available -> attempt async resolve;
    // remaining logic continues in the callback on success
    if (!m_prelaunchWrappers.isEmpty() && m_appIdResolverManager) {
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
                    SurfaceWrapper *w = matchOrCreateXdgWrapper(raw, appId);
                    initXdgWrapperCommon(raw, w);
                    m_pendingAppIdResolveToplevels.removeAt(idx);
                });
            if (started) {
                qCDebug(treelandShell) << "AppIdResolver request sent (callback) pidfd=" << pidfd;
                return; // async path handles creation
            } else {
                int idx = m_pendingAppIdResolveToplevels.indexOf(surface);
                if (idx >= 0)
                    m_pendingAppIdResolveToplevels.removeAt(idx);
                qCDebug(treelandShell)
                    << "AppIdResolverManager present but requestResolve failed pidfd=" << pidfd;
            }
        }
    }
    // Async resolve not started or failed -> directly match or create
    SurfaceWrapper *wrapper = matchOrCreateXdgWrapper(surface, QString());
    initXdgWrapperCommon(surface, wrapper);
}

SurfaceWrapper *ShellHandler::matchOrCreateXdgWrapper(WXdgToplevelSurface *surface,
                                                      const QString &targetAppId)
{
    SurfaceWrapper *wrapper = nullptr;

    if (!targetAppId.isEmpty()) {
        m_pendingPrelaunchAppIds.remove(targetAppId);
        for (int i = 0; i < m_prelaunchWrappers.size(); ++i) {
            auto *candidate = m_prelaunchWrappers[i];
            if (candidate->appId() == targetAppId) {
                qCDebug(treelandShell) << "match prelaunch xdg" << targetAppId;
                m_prelaunchWrappers.removeAt(i);
                wrapper = candidate;
                wrapper->convertToNormalSurface(surface, SurfaceWrapper::Type::XdgToplevel);
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
    }
    return wrapper;
}

void ShellHandler::initXdgWrapperCommon(WXdgToplevelSurface *surface, SurfaceWrapper *wrapper)
{
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
    setupSurfaceActiveWatcher(wrapper);
    Q_EMIT surfaceWrapperAdded(wrapper);
}

void ShellHandler::onXdgToplevelSurfaceRemoved(WXdgToplevelSurface *surface)
{
    auto wrapper = m_rootSurfaceContainer->getSurface(surface);
    // If async resolve still pending, cancel it. If wrapper never created, just return: compositor
    // never exposed this surface (from treeland's perspective).
    if (!wrapper) {
        if (!m_pendingAppIdResolveToplevels.removeOne(surface)) {
            qCWarning(treelandShell)
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
            m_windowConfigStore->saveSize(wrapper->appId(), s);
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
    surface->safeConnect(&qw_xwayland_surface::notify_associate, this, [this, surface = QPointer<WXWaylandSurface>(surface)] {
        auto raw = surface.data();
        if (!raw)
            return; // surface destroyed before callback
        // If prelaunch wrappers exist and resolver is available, attempt async resolve; if started, remaining logic handled in callback, then return
        if (!m_prelaunchWrappers.isEmpty() && m_appIdResolverManager) {
            int pidfd = raw->pidFD();
            if (pidfd >= 0) {
                m_pendingAppIdResolveToplevels.append(raw);
                bool started = m_appIdResolverManager->resolvePidfd(
                    pidfd,
                    [this, surface](const QString &appId) {
                        auto raw = surface.data();
                        if (!raw)
                            return; // surface destroyed before callback
                        int idx = m_pendingAppIdResolveToplevels.indexOf(raw);
                        if (idx < 0)
                            return; // removed before callback
                        SurfaceWrapper *w = matchOrCreateXwaylandWrapper(raw, appId);
                        initXwaylandWrapperCommon(raw, w);
                        m_pendingAppIdResolveToplevels.removeAt(idx);
                    });
                if (started) {
                    qCDebug(treelandShell)
                        << "(XWayland) AppIdResolver request sent (callback)";
                    return; // async path
                } else {
                    int idx = m_pendingAppIdResolveToplevels.indexOf(raw);
                    if (idx >= 0)
                        m_pendingAppIdResolveToplevels.removeAt(idx);
                    qCDebug(treelandShell)
                        << "(XWayland) requestResolve failed pidfd=" << pidfd;
                }
            }
        }
        // Async path not taken: directly match or create (empty appId triggers fallback retrieval)
        SurfaceWrapper *wrapper = matchOrCreateXwaylandWrapper(raw, QString());
        initXwaylandWrapperCommon(raw, wrapper);
    });
    surface->safeConnect(&qw_xwayland_surface::notify_dissociate, this, [this, surface] {
        auto wrapper = m_rootSurfaceContainer->getSurface(surface->surface());
        qCDebug(treelandShell) << "WXWayland::notify_dissociate" << surface << wrapper;
        // Cancel pending async resolve if still present. If wrapper never created, return.
        if (!wrapper) {
            if (!m_pendingAppIdResolveToplevels.removeOne(surface)) {
                qCWarning(treelandShell)
                    << "WXWayland::notify_dissociate for unknown surface" << surface;
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
                m_windowConfigStore->saveSize(wrapper->appId(), s);
            }
        }
        Q_EMIT surfaceWrapperAboutToRemove(wrapper);
        m_rootSurfaceContainer->destroyForSurface(wrapper);
    });
}

SurfaceWrapper *ShellHandler::matchOrCreateXwaylandWrapper(WXWaylandSurface *surface,
                                                           const QString &targetAppId)
{
    SurfaceWrapper *wrapper = nullptr;
    if (!targetAppId.isEmpty()) {
        m_pendingPrelaunchAppIds.remove(targetAppId);
        for (int i = 0; i < m_prelaunchWrappers.size(); ++i) {
            auto *candidate = m_prelaunchWrappers[i];
            if (candidate->appId() == targetAppId) {
                qCDebug(treelandShell) << "match prelaunch xwayland" << targetAppId;
                m_prelaunchWrappers.removeAt(i);
                wrapper = candidate;
                wrapper->convertToNormalSurface(surface, SurfaceWrapper::Type::XWayland);
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
    }
    return wrapper;
}

void ShellHandler::initXwaylandWrapperCommon(WXWaylandSurface *surface, SurfaceWrapper *wrapper)
{
    auto updateSurfaceWithParentContainer = [this, wrapper, surface] {
        updateWrapperContainer(wrapper, surface->parentSurface());
    };
    surface->safeConnect(&WXWaylandSurface::parentSurfaceChanged,
                         this,
                         updateSurfaceWithParentContainer);
    updateSurfaceWithParentContainer();
    Q_ASSERT(wrapper->parentItem());
    setupSurfaceWindowMenu(wrapper);
    setupSurfaceActiveWatcher(wrapper);
    Q_EMIT surfaceWrapperAdded(wrapper);
}

void ShellHandler::setupSurfaceActiveWatcher(SurfaceWrapper *wrapper)
{
    Q_ASSERT_X(wrapper->container(), Q_FUNC_INFO, "Must setContainer at first!");

    if (wrapper->type() == SurfaceWrapper::Type::XdgPopup) {
        connect(wrapper, &SurfaceWrapper::requestActive, this, [this, wrapper]() {
            auto parent = wrapper->parentSurface();
            while (parent->type() == SurfaceWrapper::Type::XdgPopup) {
                parent = parent->parentSurface();
            }
            if (!parent) {
                qCCritical(treelandShell) << "A new popup without toplevel parent!";
                return;
            }
            if (!parent->showOnWorkspace(m_workspace->current()->id())) {
                qCWarning(treelandShell)
                    << "A popup active, but it's parent not in current workspace!";
                return;
            }
            Helper::instance()->activateSurface(parent);
        });

        /*
        connect(wrapper, &SurfaceWrapper::requestInactive, this, [this, wrapper]() {
            auto parent = wrapper->parentSurface();
            if (!parent || parent->type() == SurfaceWrapper::Type::XdgPopup)
                return;
            Helper::instance()->activateSurface(m_workspace->current()->latestActiveSurface());
        });
        */
    } else if (wrapper->type() == SurfaceWrapper::Type::Layer) {
        connect(wrapper, &SurfaceWrapper::requestActive, this, [wrapper]() {
            auto layerSurface = qobject_cast<WLayerSurface *>(wrapper->shellSurface());
            if (layerSurface->keyboardInteractivity() == WLayerSurface::KeyboardInteractivity::None)
                return;
            /*
             * if LayerSurface's keyboardInteractivity is `OnDemand`, only allow `Overlay` layer
             * surface get keyboard focus, to avoid dock/dde-desktop grab keyboard focus When they
             * restart
             */
            if (layerSurface->layer() == WLayerSurface::LayerType::Overlay
                || layerSurface->keyboardInteractivity()
                    == WLayerSurface::KeyboardInteractivity::Exclusive)
                Helper::instance()->activateSurface(wrapper);
        });

        connect(wrapper, &SurfaceWrapper::requestInactive, this, [this]() {
            Helper::instance()->activateSurface(m_workspace->current()->latestActiveSurface());
        });
    } else { // Xdgtoplevel or X11
        connect(wrapper, &SurfaceWrapper::requestActive, this, [this, wrapper]() {
            if (wrapper->showOnWorkspace(m_workspace->current()->id()))
                Helper::instance()->activateSurface(wrapper);
            else
                m_workspace->pushActivedSurface(wrapper);
        });

        connect(wrapper, &SurfaceWrapper::requestInactive, this, [this, wrapper]() {
            m_workspace->removeActivedSurface(wrapper);
            Helper::instance()->activateSurface(m_workspace->current()->latestActiveSurface());
        });
    }
}

void ShellHandler::onLayerSurfaceAdded(WLayerSurface *surface)
{
    if (!surface->output() && !m_rootSurfaceContainer->primaryOutput()) {
        qCWarning(treelandShell) << "No output, will close layer surface!";
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
        qCWarning(treelandShell) << "A layerSurface that not in any Container is removing!";
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
    wrapper->setOwnsOutput(parentWrapper->ownsOutput());
    Q_ASSERT(wrapper->parentItem());
    Q_EMIT surfaceWrapperAdded(wrapper);
}

void ShellHandler::onInputPopupSurfaceV2Removed(WInputPopupSurface *surface)
{
    auto wrapper = m_rootSurfaceContainer->getSurface(surface->surface());
    Q_EMIT surfaceWrapperAboutToRemove(wrapper);
    m_rootSurfaceContainer->destroyForSurface(wrapper);
}

void ShellHandler::setupSurfaceWindowMenu(SurfaceWrapper *wrapper)
{
    Q_ASSERT(m_windowMenu);
    connect(wrapper,
            &SurfaceWrapper::requestShowWindowMenu,
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
