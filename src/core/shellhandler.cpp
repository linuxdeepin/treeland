// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "shellhandler.h"

#include "common/treelandlogging.h"
#include "core/qmlengine.h"
#include "core/windowsizestore.h"
#include "layersurfacecontainer.h"
#include "modules/app-id-resolver/appidresolver.h"
#include "modules/dde-shell/ddeshellmanagerinterfacev1.h"
#include "popupsurfacecontainer.h"
#include "rootsurfacecontainer.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"
#include "workspace/workspace.h"
#include "treelandconfig.hpp"

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

#include <QTimer>
#include <qloggingcategory.h>

#include <optional>

#include <qwcompositor.h>
#include <qwxwaylandsurface.h>

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
    , m_windowSizeStore(new WindowSizeStore(this))
{
    m_backgroundContainer->setZ(RootSurfaceContainer::BackgroundZOrder);
    m_bottomContainer->setZ(RootSurfaceContainer::BottomZOrder);
    m_workspace->setZ(RootSurfaceContainer::NormalZOrder);
    m_topContainer->setZ(RootSurfaceContainer::TopZOrder);
    m_overlayContainer->setZ(RootSurfaceContainer::OverlayZOrder);
    m_popupContainer->setZ(RootSurfaceContainer::PopupZOrder);
}

void ShellHandler::updateWrapperContainer(SurfaceWrapper *wrapper,
                                          WSurface *parentSurface)
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
void ShellHandler::handlePrelaunchSplashRequested(const QString &appId)
{
    if (!Helper::instance()->config()->enablePrelaunchSplash())
        return;
    if (!m_appIdResolverManager)
        return;
    if (appId.isEmpty())
        return;
    // If a prelaunch wrapper with the same appId already exists, skip creating a duplicate
    for (int i = 0; i < m_prelaunchWrappers.size(); ++i) {
        auto *w = m_prelaunchWrappers.at(i);
        if (w && w->appId() == appId) {
            return;
        }
    }
    QSize initialSize;
    if (m_windowSizeStore) {
        const QSize last = m_windowSizeStore->lastSizeFor(appId);
        if (last.isValid() && last.width() > 0 && last.height() > 0) {
            initialSize = last;
        }
    }
    auto *wrapper = new SurfaceWrapper(Helper::instance()->qmlEngine(), nullptr, initialSize, appId);
    m_workspace->addSurface(wrapper);
    m_prelaunchWrappers.append(wrapper);

    // After 5 seconds, if still unmatched (not converted and still in the list), destroy the splash wrapper
    QTimer::singleShot(5000, wrapper, [this, wrapper] {
        int idx = m_prelaunchWrappers.indexOf(wrapper);
        if (idx < 0) {
            return; // Already matched or removed earlier
        }
        qCDebug(treelandShell) << "Prelaunch splash timeout, destroy wrapper appId="
                               << wrapper->appId();
        m_prelaunchWrappers.removeAt(idx);
        m_rootSurfaceContainer->destroyForSurface(wrapper);
    });
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
    connect(xwayland, &WXWayland::ready, xwayland, [xwayland, this] {
        auto atomPid = xwayland->atom("_NET_WM_PID");
        xwayland->setAtomSupported(atomPid, true);
        auto atomNoTitlebar = xwayland->atom("_DEEPIN_NO_TITLEBAR");
        xwayland->setAtomSupported(atomNoTitlebar, true);
        // TODO: set other xsettings and sync
        setResourceManagerAtom(
            xwayland,
            QString("Xft.dpi:\t%1")
                .arg(96 * m_rootSurfaceContainer->window()->effectiveDevicePixelRatio())
                .toUtf8());
        connect(Helper::instance()->window(),
                &WOutputRenderWindow::effectiveDevicePixelRatioChanged,
                xwayland,
                [xwayland, this](qreal dpr) {
                    setResourceManagerAtom(xwayland, QString("Xft.dpi:\t%1").arg(96 * dpr).toUtf8());
                });
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
    // If there are prelaunch wrappers and the resolver is available -> attempt async resolve; remaining logic continues in the callback on success
    if (!m_prelaunchWrappers.isEmpty() && m_appIdResolverManager) {
        int pidfd = surface->pidFD();
        if (pidfd >= 0) {
            // Register pending before starting async resolve (unified list)
            m_pendingAppIdResolveToplevels.append(surface);

            bool started = m_appIdResolverManager->resolvePidfd(
                pidfd,
                [this, surface](const QString &appId) {
                    int idx = m_pendingAppIdResolveToplevels.indexOf(surface);
                    if (idx < 0)
                        return; // removed before callback
                    SurfaceWrapper *w = matchOrCreateXdgWrapper(surface, appId);
                    initXdgWrapperCommon(surface, w);
                    m_pendingAppIdResolveToplevels.removeAt(idx);
                });
            if (started) {
                qCDebug(treelandShell) << "AppIdResolver request sent (callback) pidfd=" << pidfd;
                return; // async path handles creation
            } else {
                m_pendingAppIdResolveToplevels.removeOne(surface);
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
        for (int i = 0; i < m_prelaunchWrappers.size(); ++i) {
            auto *candidate = m_prelaunchWrappers[i];
            if (candidate->appId() == targetAppId) {
                qCDebug(treelandShell) << "match prelaunch xdg" << targetAppId;
                m_prelaunchWrappers.remove(i);
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
    if (m_pendingAppIdResolveToplevels.removeOne(surface)) {
        qCInfo(treelandShell) << "Cancelled pending AppId resolve for surface:" << surface;
        if (!wrapper) {
            return; // wrapper was never created; nothing to persist or destroy
        }
    }
    auto interface = DDEShellSurfaceInterface::get(surface->surface());
    if (interface) {
        delete interface;
    }
    // Persist the last size of a normal window (prefer normalGeometry) when an appId is present
    if (m_windowSizeStore && wrapper && !wrapper->appId().isEmpty()) {
        QSizeF sz = wrapper->normalGeometry().size();
        if (!sz.isValid() || sz.isEmpty()) {
            sz = wrapper->geometry().size();
        }
        const QSize s = sz.toSize();
        if (s.isValid() && s.width() > 0 && s.height() > 0) {
            m_windowSizeStore->saveSize(wrapper->appId(), s);
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
    surface->safeConnect(&qw_xwayland_surface::notify_associate, this, [this, surface] {
        // If prelaunch wrappers exist and resolver is available, attempt async resolve; if started, remaining logic handled in callback, then return
        if (!m_prelaunchWrappers.isEmpty() && m_appIdResolverManager) {
            int pidfd = surface->pidFD();
            if (pidfd >= 0) {
                m_pendingAppIdResolveToplevels.append(surface);
                bool started = m_appIdResolverManager->resolvePidfd(
                    pidfd,
                    [this, surface](const QString &appId) {
                        int idx = m_pendingAppIdResolveToplevels.indexOf(surface);
                        if (idx < 0)
                            return; // removed before callback
                        SurfaceWrapper *w = matchOrCreateXwaylandWrapper(surface, appId);
                        initXwaylandWrapperCommon(surface, w);
                        m_pendingAppIdResolveToplevels.removeAt(idx);
                    });
                if (started) {
                    qCDebug(treelandShell)
                        << "(XWayland) AppIdResolver request sent (callback)";
                    return; // async path
                } else {
                    m_pendingAppIdResolveToplevels.removeOne(surface);
                    qCDebug(treelandShell)
                        << "(XWayland) requestResolve failed pidfd=" << pidfd;
                }
            }
        }
        // Async path not taken: directly match or create (empty appId triggers fallback retrieval)
        SurfaceWrapper *wrapper = matchOrCreateXwaylandWrapper(surface, QString());
        initXwaylandWrapperCommon(surface, wrapper);
    });
    surface->safeConnect(&qw_xwayland_surface::notify_dissociate, this, [this, surface] {
        auto wrapper = m_rootSurfaceContainer->getSurface(surface->surface());
        qCDebug(treelandShell) << "WXWayland::notify_dissociate" << surface << wrapper;
        // Cancel pending async resolve if still present. If wrapper never created, return.
        if (m_pendingAppIdResolveToplevels.removeOne(surface)) {
            qCInfo(treelandShell) << "Cancelled pending AppId resolve (XWayland) for surface:" << surface;
            if (!wrapper) {
                return; // never created
            }
        }
        // Persist XWayland window size (only if wrapper exists)
        if (m_windowSizeStore && wrapper
            && !wrapper->appId().isEmpty()) {
            QSizeF sz = wrapper->normalGeometry().size();
            if (!sz.isValid() || sz.isEmpty()) {
                sz = wrapper->geometry().size();
            }
            const QSize s = sz.toSize();
            if (s.isValid() && s.width() > 0 && s.height() > 0) {
                m_windowSizeStore->saveSize(wrapper->appId(), s);
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
        for (int i = 0; i < m_prelaunchWrappers.size(); ++i) {
            auto *candidate = m_prelaunchWrappers[i];
            if (candidate->appId() == targetAppId) {
                qCDebug(treelandShell) << "match prelaunch xwayland" << targetAppId;
                m_prelaunchWrappers.remove(i);
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
        connect(wrapper, &SurfaceWrapper::requestActive, this, [this, wrapper]() {
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

        connect(wrapper, &SurfaceWrapper::requestInactive, this, [this, wrapper]() {
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

// onAppIdResolved removed: using per-request callbacks now
