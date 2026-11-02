// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "shellhandler.h"
#include "session/sessionprelaunch.h"
#include "session/sessionmanager.h"

#include "common/treelandlogging.h"
#include "core/imcandidatepanelmanager.h"
#include "core/qmlengine.h"
#include "core/windowconfigstore.h"
#include "layersurfacecontainer.h"
#include "modules/snap-target/snaphandler.h"
#include "modules/dde-shell/ddeshellmanagerinterfacev1.h"
#include "modules/dde-shell/ddeshellmanagerinterfacev2.h"
#include "modules/foreign-toplevel/foreigntoplevelmanagerv3.h"
#include "modules/layer-shell-extension/layershellextensionmanagerinterfacev1.h"
#include "modules/show-desktop/showdesktopinterfacev1.h"
#include "modules/wine-window-management/winewindowmanagement.h"
#include "modules/wine-window-state/winewindowstate.h"
#include "output/output.h"
#include "rootsurfacecontainer.h"
#include "core/waylandserver.h"
#include "seat/seatmanager.h"
#include "session/session.h"
#include "session/sessionseatmanager.h"
#include "session/sessionoutputmanager.h"
#include "session/sessionprotocols.h"
#include "surface/seatsurfacemanager.h"
#include "surface/surfacewrapper.h"
#include "treelandconfig.hpp"
#include "treelanduserconfig.hpp"
#include "wallpapershellinterfacev1.h"
#include "workspace/workspace.h"
#include "workspace/workspacemodel.h"

#include <xcb/xcb.h>

#include <winputmethodhelper.h>
#include <winputpopupsurface.h>
#include <wlayershell.h>
#include <wlayersurface.h>
#include <woutputrenderwindow.h>
#include <woutputitem.h>
#include <woutputlayout.h>
#include <wserver.h>
#include <wxdgoutput.h>
#include <wxdgpopupsurface.h>
#include <wxdgshell.h>
#include <wxdgtoplevelsurface.h>
#include <wxwayland.h>
#include <wxwaylandsurface.h>
#include <wxwaylandsurfaceitem.h>

#include <QColor>
#include <QPointer>
#include <QTimer>
#include <private/qquickoverlay_p.h>

#include <algorithm>
#include <functional>
#include <optional>

WAYLIB_SERVER_USE_NAMESPACE

const QLatin1String PRIVILEGED_OVERLAY_TAG("org.deepin.treeland.privileged-overlay");
const QLatin1String SNAP_MASK_TAG("org.deepin.treeland.snap-mask");

ShellHandler::ShellHandler(RootSurfaceContainer *rootContainer, [[maybe_unused]] WServer *server)
    : m_rootSurfaceContainer(rootContainer)
    , m_backgroundContainer(new LayerSurfaceContainer(rootContainer))
    , m_bottomContainer(new LayerSurfaceContainer(rootContainer))
    , m_workspace(new Workspace(rootContainer))
    , m_topContainer(new LayerSurfaceContainer(rootContainer))
    , m_overlayContainer(new LayerSurfaceContainer(rootContainer))
    , m_popupContainer(new SurfaceContainer(rootContainer))
    , m_privilegedOverlayContainer(new SurfaceContainer(rootContainer))
    , m_snapMaskContainer(new SurfaceContainer(rootContainer))
    , m_windowConfigStore(new WindowConfigStore(this))
{
    qmlRegisterUncreatableType<ForeignToplevelManagerInterfaceV3>(
        "Treeland.Protocols", 1, 0, "ForeignToplevelManagerInterfaceV3",
        "Owned by Session");
    qRegisterMetaType<ForeignToplevelManagerInterfaceV3::PreviewDirection>();

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
    m_privilegedOverlayContainer->setZ(RootSurfaceContainer::PrivilegedOverlayZOrder);
    m_privilegedOverlayContainer->setObjectName(QStringLiteral("PrivilegedOverlayContainer"));
    m_snapMaskContainer->setZ(RootSurfaceContainer::SnapMaskLayerZOrder);
    m_snapMaskContainer->setObjectName(QStringLiteral("SnapMaskContainer"));
    connect(m_rootSurfaceContainer->outputModel(),
            &QAbstractItemModel::rowsInserted,
            this,
            [this] {
                for (auto *output : m_rootSurfaceContainer->outputs()) {
                    watchXWaylandDesktopOutput(output);
                }
                updateXWaylandDesktopProperties();
            });
    connect(m_rootSurfaceContainer->outputModel(),
            &QAbstractItemModel::rowsRemoved,
            this,
            &ShellHandler::updateXWaylandDesktopProperties);
    connect(m_workspace, &Workspace::currentIndexChanged,
            this, &ShellHandler::updateXWaylandDesktopProperties);
    connect(WaylandServer::instance(), &WaylandServer::showDesktopStateChanged,
            this, &ShellHandler::updateXWaylandDesktopProperties);
    connect(m_workspace, &Workspace::countChanged, this, &ShellHandler::onWorkspaceCountChanged);
    connect(m_workspace, &Workspace::workspaceAdded, this, &ShellHandler::workspaceAdded);
    watchXWaylandWorkspaceNames();
}

void ShellHandler::watchXWaylandDesktopOutput(Output *output)
{
    connect(output,
            &Output::exclusiveZoneChanged,
            this,
            &ShellHandler::updateXWaylandDesktopProperties,
            Qt::UniqueConnection);
    connect(output->outputItem(),
            &WOutputItem::geometryChanged,
            this,
            &ShellHandler::updateXWaylandDesktopProperties,
            Qt::UniqueConnection);
    connect(output->output(),
            &WOutput::enabledChanged,
            this,
            &ShellHandler::updateXWaylandDesktopProperties,
            Qt::UniqueConnection);
}

void ShellHandler::watchXWaylandWorkspaceNames()
{
    for (int i = 0; i < m_workspace->count(); ++i) {
        connect(m_workspace->modelAt(i),
                &WorkspaceModel::nameChanged,
                this,
                &ShellHandler::updateXWaylandDesktopProperties,
                Qt::UniqueConnection);
    }
}

void ShellHandler::updateXWaylandDesktopProperties()
{
    auto *outputManager = WaylandServer::instance()->xwaylandOutputManager();
    connect(outputManager,
            &WXdgOutputManager::scaleOverrideChanged,
            this,
            &ShellHandler::updateXWaylandDesktopProperties,
            Qt::UniqueConnection);
    connect(outputManager->layout(),
            &WOutputLayout::outputsChanged,
            this,
            &ShellHandler::updateXWaylandDesktopProperties,
            Qt::UniqueConnection);

    for (auto *xwayland : std::as_const(m_xwaylands)) {
        auto session = WaylandServer::instance()->sessionManager()->sessionForXWayland(xwayland);
        if (!session)
            continue;
        auto *workspace = session->workspace();
        const qreal scale = session->outputManager()->effectiveScale();
        QRect geometry;
        QRect workarea;
        for (auto *output : m_rootSurfaceContainer->outputs()) {
            auto *state = session->outputManager()->outputState(output);
            const auto logical = state->geometry();
            const auto valid = state->validGeometry();
            geometry = geometry.united(QRect(qFloor(logical.x() * scale),
                                              qFloor(logical.y() * scale),
                                              qCeil(logical.width() * scale),
                                              qCeil(logical.height() * scale)));
            workarea = workarea.united(QRect(qFloor(valid.x() * scale),
                                              qFloor(valid.y() * scale),
                                              qCeil(valid.width() * scale),
                                              qCeil(valid.height() * scale)));
        }
        if (geometry.isEmpty() || workarea.isEmpty())
            continue;
        QStringList sessionNames;
        for (int i = 0; i < workspace->count(); ++i)
            sessionNames.append(workspace->modelAt(i)->name());
        xwayland->setDesktopProperties(workspace->count(),
                                       geometry.size(),
                                       workspace->currentIndex(),
                                       sessionNames,
                                       QVector<QPoint>(workspace->count(), QPoint(0, 0)),
                                       QVector<QRect>(workspace->count(), workarea),
                                       session->isActive()
                                           && WaylandServer::instance()->showDesktopState()
                                               == ShowDesktopInterfaceV1::State::Show);
    }
}

void ShellHandler::updateWrapperContainer(SurfaceWrapper *wrapper, WSurface *parentSurface)
{
    if (wrapper->parentSurface())
        wrapper->parentSurface()->removeSubSurface(wrapper);

    auto oldContainer = wrapper->container();
    if (parentSurface) {
        auto *parent = m_rootSurfaceContainer->getSurface(parentSurface);
        if (parent && parent->session() != wrapper->session())
            return;
    }
    if (parentSurface) {
        auto parentWrapper = m_rootSurfaceContainer->getSurface(parentSurface);
        if (!parentWrapper)
            return;

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
                workspaceForSurface(wrapper)->addSurface(wrapper);
            }
            // else do nothing, already in workspace
        } else {
            workspaceForSurface(wrapper)->addSurface(wrapper);
        }
    }
}

SessionPrelaunch *ShellHandler::prelaunchForSurface(WToplevelSurface *surface) const
{
    const auto owner = WaylandServer::instance()->sessionManager()->sessionForClient(surface->waylandClient());
    return owner ? owner->prelaunch() : nullptr;
}

void ShellHandler::initializePrelaunchSurface(SurfaceWrapper *wrapper)
{
    setupSurfaceActiveWatcher(wrapper);
    registerSurfaceToForeignToplevel(wrapper);
}

Workspace *ShellHandler::createSessionWorkspace(TreelandUserConfig *config)
{
    auto *workspace = new Workspace(m_rootSurfaceContainer, config);
    workspace->setZ(RootSurfaceContainer::NormalZOrder);
    workspace->setSize(m_rootSurfaceContainer->size());
    workspace->deactivate();
    return workspace;
}

void ShellHandler::setWorkspace(Workspace *workspace)
{
    Q_ASSERT(workspace);
    if (workspace == m_workspace)
        return;
    workspace->setOpacity(m_workspace->opacity());
    workspace->setScale(m_workspace->scale());
    m_workspace->deactivate();
    disconnect(m_workspace, nullptr, this, nullptr);
    m_workspace = workspace;
    m_workspace->setEnabled(true);
    m_workspace->setVisible(true);
    connect(m_workspace, &Workspace::currentIndexChanged,
            this, &ShellHandler::updateXWaylandDesktopProperties);
    connect(m_workspace, &Workspace::countChanged, this, &ShellHandler::onWorkspaceCountChanged);
    connect(m_workspace, &Workspace::workspaceAdded, this, &ShellHandler::workspaceAdded);
    watchXWaylandWorkspaceNames();
    Q_EMIT workspaceChanged();
    updateXWaylandDesktopProperties();
}

void ShellHandler::onWorkspaceCountChanged()
{
    watchXWaylandWorkspaceNames();
    updateXWaylandDesktopProperties();
}

Workspace *ShellHandler::workspaceForSurface(SurfaceWrapper *wrapper) const
{
    if (auto *session = wrapper->session())
        return session->workspace();
    return m_workspace;
}

Workspace *ShellHandler::workspace() const
{
    return m_workspace;
}

SurfaceContainer *ShellHandler::popupContainer() const
{
    return m_popupContainer;
}

SurfaceContainer *ShellHandler::snapMaskContainer() const
{
    return m_snapMaskContainer;
}

RootSurfaceContainer *ShellHandler::rootSurfaceContainer() const
{
    return m_rootSurfaceContainer;
}

ForeignToplevelManagerInterfaceV3 *ShellHandler::foreignToplevel() const
{
    auto *session = WaylandServer::instance()->activeSession();
    return session ? session->windowManagement() : nullptr;
}

SurfaceContainer *ShellHandler::privilegedOverlayContainer() const
{
    return m_privilegedOverlayContainer;
}

void ShellHandler::createComponent(QmlEngine *engine, QQuickItem *parentItem)
{
    m_windowMenu = engine->createWindowMenu(WaylandServer::instance());
    QObject::connect(m_windowMenu, SIGNAL(closed()), this, SLOT(onWindowMenuClosed()));
    Q_UNUSED(parentItem)

}

void ShellHandler::init(WServer *server, WSeat *seat)
{
    Q_ASSERT_X(server, Q_FUNC_INFO, "server must not be null");
    Q_ASSERT_X(seat, Q_FUNC_INFO, "seat must not be null");
    Q_ASSERT_X(!m_wallpaperShell, Q_FUNC_INFO, "Only init once!");
    Q_ASSERT_X(!m_layerShellExtensionManagerInterfaceV1, Q_FUNC_INFO, "Only init once!");

    m_layerShellExtensionManagerInterfaceV1 =
        server->attach<LayerShellExtensionManagerInterfaceV1>();

    m_snapTarget = server->attach<SnapTargetV1>();
    m_snapTarget->setOutputRenderWindow(
        qobject_cast<WOutputRenderWindow *>(m_rootSurfaceContainer->window()));

    m_wallpaperShell = server->attach<TreelandWallpaperShellInterfaceV1>(m_wallpaperShell);
    if (WaylandServer::instance()->isDDMDisplay()) {
        m_wallpaperShell->setFilter([](WClient *client) {
            return WaylandServer::instance()->sessionManager()->isDDEUserClient(client);
        });
    }

    auto *overlay = QQuickOverlay::overlay(m_rootSurfaceContainer->window());
    overlay->setZ(RootSurfaceContainer::GlobalOverlayZOrder);
    overlay->setParentItem(m_rootSurfaceContainer);
}

WXWayland *ShellHandler::createXWayland(WServer *server,
                                        WSeat *seat,
                                        wlr_compositor *compositor,
                                        [[maybe_unused]] bool lazy)
{
    auto *xwayland = server->attach<WXWayland>(compositor, false);
    m_xwaylands.append(xwayland);
    xwayland->setSeat(seat);
    connect(xwayland, &WXWayland::surfaceAdded, this, &ShellHandler::onXWaylandSurfaceAdded);
    // aboutToDissociate never fires when the XWayland instance is torn down.
    connect(xwayland, &WXWayland::surfaceRemoved, this, &ShellHandler::onXWaylandSurfaceRemoved);
    connect(xwayland, &WXWayland::ready, xwayland, [this, xwayland] {
        auto atomPid = xwayland->atom("_NET_WM_PID");
        xwayland->setAtomSupported(atomPid, true);
        auto atomNoTitlebar = xwayland->atom("_DEEPIN_NO_TITLEBAR");
        xwayland->setAtomSupported(atomNoTitlebar, true);

        for (auto *output : m_rootSurfaceContainer->outputs()) {
            watchXWaylandDesktopOutput(output);
        }
        updateXWaylandDesktopProperties();

        auto session = WaylandServer::instance()->sessionManager()->sessionForXWayland(xwayland);
        if (auto *panels = candidatePanels(session.get()))
            panels->setupXWayland(xwayland);
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
    auto *prelaunch = prelaunchForSurface(surface);
    if (prelaunch && prelaunch->needsResolution()) {
        int pidfd = surface->pidFD();
        if (pidfd >= 0) {
            // Register pending before starting async resolve (unified list)
            m_pendingAppIdResolveToplevels.append(surface);

            bool started = prelaunch->resolvePidfd(
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

void ShellHandler::onXWaylandSurfaceRemoved(WXWaylandSurface *surface)
{
    auto *wrapper = m_rootSurfaceContainer->getSurface(surface);
    if (!wrapper || wrapper->isAboutToRemove())
        return;
    if (auto *xwayland = surface->xwayland())
        xwayland->cancelAsyncProperties(surface->handle()->window_id);
    Q_EMIT surfaceWrapperAboutToRemove(wrapper);
    m_rootSurfaceContainer->destroyForSurface(wrapper);
}

void ShellHandler::ensureXdgWrapper(WXdgToplevelSurface *surface, const QString &targetAppId)
{
    auto *prelaunch = prelaunchForSurface(surface);
    if (prelaunch && prelaunch->consumeClosed(targetAppId)) {
        surface->close();
        return;
    }
    SurfaceWrapper *wrapper = prelaunch
        ? prelaunch->takeSplash(targetAppId, surface, SurfaceWrapper::Type::XdgToplevel) : nullptr;
    bool isNewWrapper = !wrapper;

    if (!wrapper) {
        wrapper = new SurfaceWrapper(WaylandServer::instance()->qmlEngine(),
                                     surface,
                                     SurfaceWrapper::Type::XdgToplevel,
                                     targetAppId);
        workspaceForSurface(wrapper)->addSurface(wrapper);
        isNewWrapper = true; // newly created
    }

    // Initialize wrapper. Prefer v2 when both are present: the deprecated v1
    // global is kept only for the migration period and dde-shell is expected
    // to migrate to v2; a client that binds both is not a supported case.
    if (auto *shellSurfaceV2 = DDEShellSurfaceV2::get(surface->surface())) {
        handleDdeShellSurfaceV2Added(shellSurfaceV2, wrapper);
    } else if (DDEShellSurfaceInterface::get(surface->surface())) {
        handleDdeShellSurfaceAdded(surface->surface(), wrapper);
    }
    auto updateSurfaceWithParentContainer = [this, wrapper, surface] {
        if (wrapper->surfaceRole() == SurfaceWrapper::SurfaceRole::PrivilegedOverlay)
            return;
        updateWrapperContainer(wrapper, surface->parentSurface());
    };

    QObject::connect(surface, &WXdgToplevelSurface::parentXdgSurfaceChanged,
                         this,
                         updateSurfaceWithParentContainer);
    updateSurfaceWithParentContainer();
    Q_ASSERT(wrapper->parentItem());
    if (surface->isInitialized()) {
        const auto initialState = surface->handle()->requested.fullscreen
            ? SurfaceWrapper::State::Fullscreen
            : surface->handle()->requested.maximized ? SurfaceWrapper::State::Maximized
                                                     : SurfaceWrapper::State::Normal;
        if (initialState != SurfaceWrapper::State::Normal)
            wrapper->setSurfaceStateDirectly(initialState);
    }
    setupSurfaceWindowMenu(wrapper);
    // Only setup active watcher for newly created wrappers;
    // prelaunch splash wrappers already have it set up in initializePrelaunchSurface
    if (isNewWrapper) {
        setupSurfaceActiveWatcher(wrapper);
        registerSurfaceToForeignToplevel(wrapper);
    }
    Q_EMIT surfaceWrapperAdded(wrapper);

    // Privileged overlay, IM candidate panel, and snap mask detection via xdg-toplevel-tag
    QPointer<SurfaceWrapper> wrapperPtr(wrapper);
    auto applyIfTaggedSurface = [this, wrapperPtr]() {
        if (!wrapperPtr)
            return;
        if (checkAndApplyPrivilegedOverlay(wrapperPtr))
            return;
        if (auto *panels = candidatePanels(wrapperPtr->session());
            panels && panels->checkAndApplyIMCandidatePanel(wrapperPtr))
            return;
        if (checkAndApplySnapMask(wrapperPtr))
            return;
    };
    QObject::connect(surface, &WXdgToplevelSurface::tagChanged, this, applyIfTaggedSurface);
    applyIfTaggedSurface();
}

bool ShellHandler::checkAndApplyPrivilegedOverlay(SurfaceWrapper *wrapper)
{
    auto *xdgSurface = qobject_cast<WXdgToplevelSurface *>(wrapper->shellSurface());
    if (!xdgSurface) return false;
    if (xdgSurface->tag() != PRIVILEGED_OVERLAY_TAG) {
        if (wrapper->surfaceRole() == SurfaceWrapper::SurfaceRole::PrivilegedOverlay)
            revokePrivilegedOverlay(wrapper);
        return false;
    }
    applyPrivilegedOverlay(wrapper);
    return true;
}

void ShellHandler::applyPrivilegedOverlay(SurfaceWrapper *wrapper)
{
    qCInfo(lcTlShellXdg) << "Privileged overlay promoted (tag)"
                                  << "wrapper =" << wrapper << "appId =" << wrapper->appId();
    // Move from the original container to the privileged overlay container
    if (auto *oldContainer = wrapper->container())
        oldContainer->removeSurface(wrapper);
    m_privilegedOverlayContainer->addSurface(wrapper);
    wrapper->disableWindowAnimation();
    wrapper->setSurfaceRole(SurfaceWrapper::SurfaceRole::PrivilegedOverlay);
    wrapper->setPositionAutomatic(false);
    wrapper->setHasInitializeContainer(true);
    wrapper->setSkipDockPreView(true);
    wrapper->setSkipMutiTaskView(true);
}

void ShellHandler::revokePrivilegedOverlay(SurfaceWrapper *wrapper)
{
    qCInfo(lcTlShellXdg) << "Privileged overlay revoked (tag removed)"
                                  << "wrapper =" << wrapper << "appId =" << wrapper->appId();
    // Move from the privileged overlay container back to the workspace
    if (auto *oldContainer = wrapper->container())
        oldContainer->removeSurface(wrapper);
    wrapper->setSurfaceRole(SurfaceWrapper::SurfaceRole::Normal);
    wrapper->setPositionAutomatic(true);
    wrapper->disableWindowAnimation(false);
    wrapper->setSkipDockPreView(false);
    wrapper->setSkipMutiTaskView(false);
    workspaceForSurface(wrapper)->addSurface(wrapper);
}

void ShellHandler::onXdgToplevelSurfaceRemoved(WXdgToplevelSurface *surface)
{
    auto wrapper = m_rootSurfaceContainer->getSurface(surface);

    // Remove any attached dde-shell objects before the wrapper-null early
    // return: a pending app-id resolve means no wrapper was ever created, yet
    // the wl_surface may still hold a shell-surface object. The wl_surface is
    // going away, so these objects only need cleanup, not a protocol destroy.
    if (auto interfaceV2 = DDEShellSurfaceV2::get(surface->surface())) {
        delete interfaceV2;
    }
    if (auto interface = DDEShellSurfaceInterface::get(surface->surface())) {
        delete interface;
    }

    // If async resolve still pending, cancel it. If wrapper never created, just return: server
    // never exposed this surface (from treeland's perspective).
    if (!wrapper) {
        if (!m_pendingAppIdResolveToplevels.removeOne(surface)) {
            qCWarning(lcTlShell)
                << "onXdgToplevelSurfaceRemoved for unknown surface" << surface;
        }
        return;
    }
    // Persist the last size of a normal window (prefer normalGeometry) when an appId is present
    if (m_windowConfigStore && !wrapper->appId().isEmpty()) {
        QSizeF sz = wrapper->normalGeometry().size();
        if ((!sz.isValid() || sz.isEmpty()) && wrapper->isNormal()) {
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
    auto wrapper = new SurfaceWrapper(WaylandServer::instance()->qmlEngine(),
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
    QObject::connect(surface, &WXWaylandSurface::associated,
                         this,
                         [this, surface = QPointer<WXWaylandSurface>(surface)] {
                             auto raw = surface.data();
                             if (!raw)
                                 return; // surface destroyed before callback
                             // If prelaunch wrappers or closed splash appIds exist and resolver is
                             // available, attempt async resolve; if started, remaining logic
                             // handled in callback, then return
                             auto *prelaunch = prelaunchForSurface(raw);
                             if (prelaunch && prelaunch->needsResolution()) {
                                 int pidfd = raw->pidFD();
                                 if (pidfd >= 0) {
                                     m_pendingAppIdResolveToplevels.append(raw);
                                     bool started = prelaunch->resolvePidfd(
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
    QObject::connect(surface, &WXWaylandSurface::aboutToDissociate, this, [this, surface] {
        auto wrapper = m_rootSurfaceContainer->getSurface(surface);
        qCDebug(lcTlShell) << "WXWayland::aboutToDissociate" << surface << wrapper;

        // Cancel pending async property fetch for this surface.
        auto *xwayland = surface->xwayland();
        if (xwayland) {
            auto windowId = surface->handle()->window_id;
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

void ShellHandler::fetchInitialProperties(WXWaylandSurface *surface, const QString &appId)
{
    auto *xwayland = surface->xwayland();
    if (!xwayland) {
        ensureXwaylandWrapper(surface, appId);
        return;
    }

    auto windowId = surface->handle()->window_id;
    QVector<WXWayland::AsyncPropRequest> requests;
    auto session = WaylandServer::instance()->sessionManager()->sessionForXWayland(xwayland);
    if (auto *panels = candidatePanels(session.get()))
        requests.append({ panels->imCandidatePanelAtom(), XCB_ATOM_CARDINAL });

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
    auto session = WaylandServer::instance()->sessionManager()->sessionForXWayland(surface->xwayland());
    if (auto *panels = candidatePanels(session.get())) {
        bool value = IMCandidatePanelManager::parseIMCandidatePanelProperty(
            result,
            panels->imCandidatePanelAtom());
        surface->setProperty("imCandidatePanel", value);
    }
    ensureXwaylandWrapper(surface, appId);
}

void ShellHandler::ensureXwaylandWrapper(WXWaylandSurface *surface, const QString &targetAppId)
{
    auto *prelaunch = prelaunchForSurface(surface);
    if (prelaunch && prelaunch->consumeClosed(targetAppId)) {
        surface->close();
        return;
    }
    SurfaceWrapper *wrapper = prelaunch
        ? prelaunch->takeSplash(targetAppId, surface, SurfaceWrapper::Type::XWayland) : nullptr;
    bool isNewWrapper = !wrapper;

    if (!wrapper) {
        wrapper = new SurfaceWrapper(WaylandServer::instance()->qmlEngine(),
                                     surface,
                                     SurfaceWrapper::Type::XWayland,
                                     targetAppId);
        workspaceForSurface(wrapper)->addSurface(wrapper);
        isNewWrapper = true; // newly created
    }

    // IM candidate panel detection via XWayland xprop
    if (auto *panels = candidatePanels(wrapper->session());
        panels && panels->checkAndApplyIMCandidatePanel(wrapper)) {
        Q_EMIT surfaceWrapperAdded(wrapper);
        return;
    }

    // Initialize wrapper
    wrapper->setModal(surface->isModal());
    auto surfaceGuard = QPointer<WXWaylandSurface>(surface);
    auto wrapperGuard = QPointer<SurfaceWrapper>(wrapper);
    auto updateSurfaceWithParentContainer = [this, surfaceGuard, wrapperGuard] {
        if (!surfaceGuard || !wrapperGuard || wrapperGuard->shellSurface() != surfaceGuard)
            return;

        updateWrapperContainer(wrapperGuard, surfaceGuard->parentSurface());

        if (!surfaceGuard->isBypassManager()) {
            if (auto *parent = surfaceGuard->parentXWaylandSurface())
                surfaceGuard->restack(parent, WXWaylandSurface::XCB_STACK_MODE_ABOVE);
        }
    };
    QObject::connect(surface,
                     &WXWaylandSurface::parentSurfaceChanged,
                     wrapper,
                     updateSurfaceWithParentContainer);
    updateSurfaceWithParentContainer();
    Q_ASSERT(wrapper->parentItem());
    const auto initialState = surface->handle()->fullscreen
        ? SurfaceWrapper::State::Fullscreen
        : (surface->handle()->maximized_horz && surface->handle()->maximized_vert)
        ? SurfaceWrapper::State::Maximized : SurfaceWrapper::State::Normal;
    if (isNewWrapper && initialState != SurfaceWrapper::State::Normal) {
        if (initialState == SurfaceWrapper::State::Fullscreen)
            wrapper->enterFullscreen();
        else
            wrapper->maximize();
    }
    setupSurfaceWindowMenu(wrapper);
    // Only setup active watcher for newly created wrappers;
    // prelaunch splash wrappers already have it set up in initializePrelaunchSurface
    if (isNewWrapper) {
        setupSurfaceActiveWatcher(wrapper);
        registerSurfaceToForeignToplevel(wrapper);
    }
    Q_EMIT surfaceWrapperAdded(wrapper);
}

void ShellHandler::registerSurfaceToForeignToplevel(SurfaceWrapper *wrapper)
{
    if (!wrapper->skipDockPreView()) {
        wrapper->session()->windowManagement()->addSurface(wrapper);
    }
    connect(wrapper, &SurfaceWrapper::skipDockPreViewChanged, this, [this, wrapper] {
        if (wrapper->skipDockPreView()) {
            wrapper->session()->windowManagement()->removeSurface(wrapper);
        } else {
            wrapper->session()->windowManagement()->addSurface(wrapper);
        }
    });
}

bool ShellHandler::checkAndApplySnapMask(SurfaceWrapper *wrapper)
{
    auto *xdgSurface = qobject_cast<WXdgToplevelSurface *>(wrapper->shellSurface());
    if (!xdgSurface || xdgSurface->tag() != SNAP_MASK_TAG)
        return false;
    if (wrapper->isSnapMask())
        return true;

    wrapper->setSkipSwitcher(true);
    wrapper->setSkipDockPreView(true);
    wrapper->setSkipMutiTaskView(true);
    wrapper->disableWindowAnimation();

    if (auto *oldContainer = wrapper->container())
        oldContainer->removeSurface(wrapper);
    m_snapMaskContainer->addSurface(wrapper);

    wrapper->setHasInitializeContainer(true);
    wrapper->setSnapMask(true);
    return true;
}

void ShellHandler::registerSession(Session *session)
{
    connect(session->prelaunch(), &SessionPrelaunch::splashCreated,
            this, &ShellHandler::initializePrelaunchSurface);
    connect(session->outputManager(), &SessionOutputManager::desktopGeometryChanged,
            this, &ShellHandler::updateXWaylandDesktopProperties);
    auto *xdgShell = session->protocols()->xdgShell();
    connect(xdgShell,
            &WXdgShell::toplevelSurfaceAdded,
            this,
            &ShellHandler::onXdgToplevelSurfaceAdded);
    connect(xdgShell,
            &WXdgShell::toplevelSurfaceRemoved,
            this,
            &::ShellHandler::onXdgToplevelSurfaceRemoved);
    connect(xdgShell, &WXdgShell::popupSurfaceAdded, this, &ShellHandler::onXdgPopupSurfaceAdded);
    connect(xdgShell,
            &WXdgShell::popupSurfaceRemoved,
            this,
            &ShellHandler::onXdgPopupSurfaceRemoved);

    auto *layerShell = session->protocols()->layerShell();
    connect(layerShell, &WLayerShell::surfaceAdded, this, &ShellHandler::onLayerSurfaceAdded);
    connect(layerShell, &WLayerShell::surfaceRemoved, this, &ShellHandler::onLayerSurfaceRemoved);
}

void ShellHandler::onSurfaceInactivationRequested(SurfaceWrapper *wrapper)
{
    Q_ASSERT(wrapper);
    if (wrapper->type() != SurfaceWrapper::Type::Layer)
        workspaceForSurface(wrapper)->removeActivedSurface(wrapper);

    auto *server = WaylandServer::instance();
    auto *seatManager = server->seatManager();
    WSeat *primarySeat = server->seat();
    const auto seats = seatManager->seats();
    auto *container = server->rootSurfaceContainer();

    for (auto *seat : seats) {
        auto *seatContainer = container->getSeatContainer(seat);
        const bool isKeyboardFocusOwner = seatContainer->keyboardFocusSurface() == wrapper;
        const bool isActivatedOwner = seatContainer->activatedSurface() == wrapper;

        if (isKeyboardFocusOwner) {
            // Clear the activated state up front instead of relying on activateSurface below:
            // the fallback can be refused (e.g. a minimized modal), and the non-primary seat
            // path does not activate anything, so a minimized surface must not stay activated.
            server->setActivatedSurface(nullptr, seat);
            // TODO(multi-seat): non-primary seats should also perform focus fallback once
            // per-seat workspace stacking is supported.
            if (seat == primarySeat) {
                server->activateSurface(m_workspace->current()->latestActiveSurface(),
                                        Qt::OtherFocusReason,
                                        seat,
                                        false);
            } else {
                server->requestKeyboardFocus(nullptr, Qt::OtherFocusReason, seat);
            }
            // A refused fallback leaves the focus on this surface; never keep keyboard
            // focus on a minimized surface.
            if (seatContainer->keyboardFocusSurface() == wrapper)
                server->requestKeyboardFocus(nullptr, Qt::OtherFocusReason, seat);
            continue;
        }

        if (isActivatedOwner) { // not isKeyboardFocusOwner
            // Clear the activated state so foreign-toplevel clients do not observe a
            // minimized-but-activated window.
            server->setActivatedSurface(nullptr, seat);
        }
    }
}

void ShellHandler::updateSurfaceAcceptKeyboardFocus(SurfaceWrapper *wrapper, bool accept)
{
    if (accept) {
        wrapper->setAcceptKeyboardFocus(true);
        return;
    }

    // DDE Shell properties may arrive after the surface has already entered
    // the activation history. Remove it explicitly instead of treating
    // keyboard-focus acceptance as an ActiveControlState capability.
    if (wrapper->type() != SurfaceWrapper::Type::Layer && wrapper->workspaceId() != -1)
        workspaceForSurface(wrapper)->removeActivedSurface(wrapper);

    auto *server = WaylandServer::instance();
    auto *seatManager = server->seatManager();
    const auto seats = seatManager->seats();
    for (auto *seat : seats) {
        auto *seatContainer = server->rootSurfaceContainer()->getSeatContainer(seat);
        if (!seatContainer)
            continue;

        const bool isActivated = seatContainer->activatedSurface() == wrapper;
        const bool hasKeyboardFocus = seatContainer->keyboardFocusSurface() == wrapper;
        if (!isActivated && !hasKeyboardFocus)
            continue;

        if (seat != server->seat()) {
            if (isActivated)
                server->setActivatedSurface(nullptr, seat);
            if (hasKeyboardFocus)
                server->requestKeyboardFocus(nullptr, Qt::OtherFocusReason, seat);
            continue;
        }

        auto *fallback = seatContainer->keyboardFocusSurface();
        if (!fallback || fallback == wrapper || !fallback->acceptKeyboardFocus())
            fallback = m_workspace->current()->latestActiveSurface();
        if (fallback && !fallback->acceptKeyboardFocus())
            fallback = nullptr;

        if (isActivated)
            server->activateSurface(fallback, Qt::OtherFocusReason, seat, false);
        else
            server->requestKeyboardFocus(fallback, Qt::OtherFocusReason, seat);
    }

    wrapper->setAcceptKeyboardFocus(false);
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
                        == WLayerSurface::KeyboardInteractivity::Exclusive) {
                    WaylandServer::instance()->cancelShowDesktop();
                    WaylandServer::instance()->requestKeyboardFocus(wrapper);
                }
            } else {
                onSurfaceInactivationRequested(wrapper);
            }
        });
    } else { // Xdgtoplevel or X11 or Splash
        connect(wrapper, &SurfaceWrapper::activationRequested, this, [this, wrapper]() {
            if (!wrapper->acceptKeyboardFocus())
                return;

            if (WaylandServer::instance()->sessionManager()->isActiveWindow(wrapper)
                && wrapper->showOnWorkspace(m_workspace->current()->id()))
                WaylandServer::instance()->activateSurface(wrapper);
            else
                workspaceForSurface(wrapper)->pushActivedSurface(wrapper);
        });

        connect(wrapper, &SurfaceWrapper::inactivationRequested, this, [this, wrapper]() {
            onSurfaceInactivationRequested(wrapper);
        });

        if (wrapper->hasActiveCapability() && wrapper->acceptKeyboardFocus()) {
            if (WaylandServer::instance()->sessionManager()->isActiveWindow(wrapper)
                && wrapper->showOnWorkspace(m_workspace->current()->id()))
                WaylandServer::instance()->activateSurface(wrapper);
            else
                workspaceForSurface(wrapper)->pushActivedSurface(wrapper);
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
        new SurfaceWrapper(WaylandServer::instance()->qmlEngine(), surface, SurfaceWrapper::Type::Layer);

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
        // wlr-layer-shell leaves the order inside one layer to the compositor.
        // Keep the DDE desktop (icon canvas) above other background surfaces,
        // such as third-party wallpapers, whatever order they were mapped in.
        if (surface->isDesktop())
            surface->setZ(1);
        break;
    case WLayerSurface::LayerType::Bottom:
        m_bottomContainer->addSurface(surface);
        break;
    case WLayerSurface::LayerType::Top:
        m_topContainer->addSurface(surface);
        if (surface->isLaunchpad() || surface->isQuickLaunchpad())
            surface->setZ(-1);
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
    auto wrapper = new SurfaceWrapper(WaylandServer::instance()->qmlEngine(),
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
    WaylandServer::instance()->activateSurface(m_workspace->current()->latestActiveSurface());
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

    // Placement hints (set_auto_placement / set_surface_position) only affect
    // the initial placement; apply them once here instead of tracking runtime
    // changes with ongoing signal connections.
    if (ddeShellSurface->yOffset().has_value())
        wrapper->setAutoPlaceCursorOffset(QPoint(0, int(ddeShellSurface->yOffset().value())));

    if (ddeShellSurface->surfacePos().has_value())
        wrapper->setClientRequstPos(ddeShellSurface->surfacePos().value());

    if (ddeShellSurface->skipSwitcher().has_value())
        wrapper->setSkipSwitcher(ddeShellSurface->skipSwitcher().value());

    if (ddeShellSurface->skipDockPreView().has_value())
        wrapper->setSkipDockPreView(ddeShellSurface->skipDockPreView().value());

    if (ddeShellSurface->skipMutiTaskView().has_value())
        wrapper->setSkipMutiTaskView(ddeShellSurface->skipMutiTaskView().value());

    updateSurfaceAcceptKeyboardFocus(wrapper, ddeShellSurface->acceptKeyboardFocus());

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
            [this, wrapper](bool accept) {
                updateSurfaceAcceptKeyboardFocus(wrapper, accept);
            });
}

void ShellHandler::handleDdeShellSurfaceV2Added(DDEShellSurfaceV2 *shellSurface,
                                                SurfaceWrapper *wrapper)
{
    wrapper->setIsDDEShellSurface(true);

    // The overlay role is the default of the v2 protocol; the normal role
    // returns the surface to the regular window layer.
    auto updateLayer = [shellSurface, wrapper] {
        wrapper->setSurfaceRole(shellSurface->role() == DDEShellSurfaceV2::OVERLAY
                                    ? SurfaceWrapper::SurfaceRole::Overlay
                                    : SurfaceWrapper::SurfaceRole::Normal);
    };
    updateLayer();
    connect(shellSurface, &DDEShellSurfaceV2::roleChanged, this, [updateLayer] {
        updateLayer();
    });

    // Placement hints (set_position_hint / set_cursor_placement_hint) only
    // affect the initial placement; apply them once here instead of tracking
    // runtime changes with ongoing signal connections.
    if (auto pos = shellSurface->positionHint())
        wrapper->setClientRequstPos(*pos);
    else if (auto offset = shellSurface->cursorPlacementHint())
        wrapper->setAutoPlaceCursorOffset(*offset);

    auto applySkipFlags = [wrapper](quint32 flags) {
        wrapper->setSkipSwitcher(flags & DDEShellSurfaceV2::SkipSwitcher);
        wrapper->setSkipDockPreView(flags & DDEShellSurfaceV2::SkipDockPreview);
        wrapper->setSkipMutiTaskView(flags & DDEShellSurfaceV2::SkipMultitaskView);
    };
    applySkipFlags(shellSurface->skipFlags());
    connect(shellSurface,
            &DDEShellSurfaceV2::skipFlagsChanged,
            this,
            applySkipFlags);

    wrapper->setAcceptKeyboardFocus(shellSurface->acceptKeyboardFocus());
    connect(shellSurface,
            &DDEShellSurfaceV2::acceptKeyboardFocusChanged,
            this,
            [wrapper](bool accept) {
                wrapper->setAcceptKeyboardFocus(accept);
            });
}

WInputMethodHelper *ShellHandler::inputMethodHelper(WSeat *seat) const
{
    for (const auto &session : WaylandServer::instance()->sessionManager()->sessions()) {
        if (auto *helper = session->seatManager()->inputMethodHelper(seat))
            return helper;
    }
    return nullptr;
}

IMCandidatePanelManager *ShellHandler::candidatePanels(Session *session) const
{
    return session ? session->seatManager()->candidatePanels() : nullptr;
}

void ShellHandler::registerInputMethodHelper(WInputMethodHelper *helper)
{
    connect(helper, &WInputMethodHelper::inputPopupSurfaceV2Added,
            this, &ShellHandler::onInputPopupSurfaceV2Added);
    connect(helper, &WInputMethodHelper::inputPopupSurfaceV2Removed,
            this, &ShellHandler::onInputPopupSurfaceV2Removed);
}
