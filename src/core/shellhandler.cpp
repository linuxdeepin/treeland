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
#include <QSet>
#include <QTimer>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <optional>

QW_USE_NAMESPACE
WAYLIB_SERVER_USE_NAMESPACE

#define TREELAND_XDG_SHELL_VERSION 5

namespace {

uint32_t xwaylandWindowId(WXWaylandSurface *surface)
{
    if (!surface || surface->isInvalidated() || !surface->handle() || !surface->handle()->handle())
        return 0;

    return surface->handle()->handle()->window_id;
}

const char *xwaylandInputModelName(WXWaylandSurface::InputModel model)
{
    switch (model) {
    case WXWaylandSurface::InputModelNone:
        return "none";
    case WXWaylandSurface::InputModelPassive:
        return "passive";
    case WXWaylandSurface::InputModelLocal:
        return "local";
    case WXWaylandSurface::InputModelGlobal:
        return "global";
    }
    return "unknown";
}

WXWaylandSurface::InputModel xwaylandInputModel(WXWaylandSurface *surface)
{
    if (!surface || surface->isInvalidated() || !surface->handle() || !surface->handle()->handle())
        return WXWaylandSurface::InputModelNone;

    return surface->inputModel();
}

bool xwaylandSupportsWmTakeFocus(WXWaylandSurface *surface)
{
    return surface && !surface->isInvalidated() && surface->handle() && surface->handle()->handle()
        && surface->supportsWmTakeFocus();
}

QRect xwaylandGeometry(WXWaylandSurface *surface)
{
    if (!surface || surface->isInvalidated() || !surface->handle() || !surface->handle()->handle())
        return QRect();

    return surface->geometry();
}

WXWaylandSurface *xwaylandShellSurface(SurfaceWrapper *wrapper)
{
    if (!wrapper || wrapper->type() != SurfaceWrapper::Type::XWayland)
        return nullptr;

    return qobject_cast<WXWaylandSurface *>(wrapper->shellSurface());
}

WXWaylandSurface *managedXwaylandShellSurface(SurfaceWrapper *wrapper)
{
    auto *surface = xwaylandShellSurface(wrapper);
    if (!surface || surface->isBypassManager())
        return nullptr;

    return surface;
}

struct XWaylandParentResolution
{
    WXWaylandSurface *directParent = nullptr;
    WXWaylandSurface *resolvedParent = nullptr;
    WSurface *resolvedSurface = nullptr;
    SurfaceWrapper *resolvedWrapper = nullptr;
    int resolvedDepth = -1;
    bool chainTruncated = false;
    bool usedX11TreeParent = false;
    xcb_window_t x11TreeDirectWindow = XCB_WINDOW_NONE;
    xcb_window_t x11TreeResolvedWindow = XCB_WINDOW_NONE;
    int x11TreeResolvedDepth = -1;
    bool x11TreeQueried = false;
    bool x11TreeTruncated = false;
    const char *x11TreeRejectReason = "not-run";
};

QRect xwaylandRequestConfigureGeometry(WXWaylandSurface *surface)
{
    if (!surface || surface->isInvalidated() || !surface->handle() || !surface->handle()->handle())
        return QRect();

    return surface->requestConfigureGeometry();
}

WXWaylandSurface *findXWaylandSurfaceByWindowId(const QList<WXWayland *> &xwaylands,
                                                xcb_window_t windowId)
{
    if (windowId == XCB_WINDOW_NONE)
        return nullptr;

    for (auto *xwayland : xwaylands) {
        if (!xwayland)
            continue;

        const auto surfaces = xwayland->surfaceList();
        for (auto *candidate : surfaces) {
            if (xwaylandWindowId(candidate) == windowId)
                return candidate;
        }
    }

    return nullptr;
}

WXWayland *findXWaylandForSurface(const QList<WXWayland *> &xwaylands,
                                  WXWaylandSurface *surface)
{
    if (!surface)
        return nullptr;

    for (auto *xwayland : xwaylands) {
        if (!xwayland)
            continue;

        const auto surfaces = xwayland->surfaceList();
        if (surfaces.contains(surface))
            return xwayland;
    }

    return nullptr;
}

bool tryResolveXWaylandParentFromNativeTree(WXWaylandSurface *surface,
                                            RootSurfaceContainer *rootSurfaceContainer,
                                            const QList<WXWayland *> &xwaylands,
                                            XWaylandParentResolution *resolution)
{
    if (!surface || !rootSurfaceContainer || !resolution)
        return false;

    auto *xwayland = findXWaylandForSurface(xwaylands, surface);
    auto *connection = xwayland ? xwayland->xcbConnection() : nullptr;
    auto *screen = xwayland ? xwayland->xcbScreen() : nullptr;
    auto *directParent = surface->parentXWaylandSurface();
    const xcb_window_t directParentWindow = xwaylandWindowId(directParent);

    resolution->x11TreeDirectWindow = directParentWindow;
    resolution->x11TreeQueried = directParentWindow != XCB_WINDOW_NONE;

    if (!xwayland || !connection || directParentWindow == XCB_WINDOW_NONE) {
        resolution->x11TreeRejectReason = !xwayland ? "missing-xwayland"
            : (!connection ? "missing-xcb-connection" : "missing-direct-parent-window");
        return false;
    }

    const xcb_window_t rootWindow = screen ? screen->root : XCB_WINDOW_NONE;
    constexpr int maxNativeParentDepth = 32;
    QSet<xcb_window_t> visited;
    xcb_window_t currentWindow = directParentWindow;

    for (int depth = 0; currentWindow != XCB_WINDOW_NONE && depth < maxNativeParentDepth;
         ++depth) {
        if (currentWindow == rootWindow) {
            resolution->x11TreeRejectReason = "reached-root";
            return false;
        }

        if (visited.contains(currentWindow)) {
            resolution->x11TreeTruncated = true;
            resolution->x11TreeRejectReason = "loop";
            return false;
        }
        visited.insert(currentWindow);

        auto *candidate = findXWaylandSurfaceByWindowId(xwaylands, currentWindow);
        WSurface *candidateSurface = candidate && candidate != surface ? candidate->surface() : nullptr;
        auto *candidateWrapper =
            candidateSurface ? rootSurfaceContainer->getSurface(candidateSurface) : nullptr;
        if (candidateWrapper) {
            resolution->resolvedParent = candidate;
            resolution->resolvedSurface = candidateSurface;
            resolution->resolvedWrapper = candidateWrapper;
            resolution->resolvedDepth = depth;
            resolution->usedX11TreeParent = true;
            resolution->x11TreeResolvedWindow = currentWindow;
            resolution->x11TreeResolvedDepth = depth;
            resolution->x11TreeRejectReason = "resolved";

            qCDebug(lcTlXwayland)
                << "[XWL_PARENT_TREE] Resolved XWayland native parent tree:"
                << "window_id=" << xwaylandWindowId(surface)
                << "direct_parent_window_id=" << directParentWindow
                << "resolved_parent_window_id=" << currentWindow
                << "depth=" << depth
                << "resolved_parent=" << candidate
                << "parentSurface=" << candidateSurface
                << "parentWrapper=" << candidateWrapper
                << "parent_geometry=" << xwaylandGeometry(candidate)
                << "parent_request_geometry=" << xwaylandRequestConfigureGeometry(candidate);
            return true;
        }

        xcb_generic_error_t *error = nullptr;
        auto *reply = xcb_query_tree_reply(connection,
                                           xcb_query_tree(connection, currentWindow),
                                           &error);
        if (error) {
            qCDebug(lcTlXwayland)
                << "[XWL_PARENT_TREE] Failed to query XWayland native parent tree:"
                << "window_id=" << xwaylandWindowId(surface)
                << "query_window_id=" << currentWindow
                << "depth=" << depth
                << "xcb_error_code=" << error->error_code;
            std::free(error);
            if (reply)
                std::free(reply);
            resolution->x11TreeRejectReason = "xcb-error";
            return false;
        }

        if (!reply) {
            resolution->x11TreeRejectReason = "missing-query-reply";
            return false;
        }

        const xcb_window_t parentWindow = reply->parent;
        qCDebug(lcTlXwayland)
            << "[XWL_PARENT_TREE] XWayland native parent tree step:"
            << "window_id=" << xwaylandWindowId(surface)
            << "query_window_id=" << currentWindow
            << "parent_window_id=" << parentWindow
            << "depth=" << depth
            << "candidate=" << candidate
            << "candidate_surface=" << candidateSurface
            << "candidate_wrapper=" << candidateWrapper;
        std::free(reply);

        if (parentWindow == currentWindow) {
            resolution->x11TreeTruncated = true;
            resolution->x11TreeRejectReason = "self-parent";
            return false;
        }

        currentWindow = parentWindow;
    }

    if (currentWindow != XCB_WINDOW_NONE)
        resolution->x11TreeTruncated = true;
    resolution->x11TreeRejectReason =
        resolution->x11TreeTruncated ? "max-depth" : "no-parent";
    return false;
}

XWaylandParentResolution resolveXWaylandParent(WXWaylandSurface *surface,
                                               RootSurfaceContainer *rootSurfaceContainer,
                                               const QList<WXWayland *> &xwaylands)
{
    XWaylandParentResolution resolution;
    if (!surface || !rootSurfaceContainer)
        return resolution;

    constexpr int maxParentDepth = 16;
    auto *parent = surface->parentXWaylandSurface();
    resolution.directParent = parent;

    for (int depth = 0; parent && depth < maxParentDepth;
         ++depth, parent = parent->parentXWaylandSurface()) {
        if (parent == surface || parent->isInvalidated()) {
            resolution.chainTruncated = true;
            break;
        }

        auto *parentSurface = parent->surface();
        auto *parentWrapper = parentSurface ? rootSurfaceContainer->getSurface(parentSurface) : nullptr;
        if (!parentSurface || !parentWrapper)
            continue;

        resolution.resolvedParent = parent;
        resolution.resolvedSurface = parentSurface;
        resolution.resolvedWrapper = parentWrapper;
        resolution.resolvedDepth = depth;
        return resolution;
    }

    if (parent)
        resolution.chainTruncated = true;

    tryResolveXWaylandParentFromNativeTree(surface,
                                           rootSurfaceContainer,
                                           xwaylands,
                                           &resolution);
    return resolution;
}

void logXWaylandParentChain(WXWaylandSurface *surface,
                            RootSurfaceContainer *rootSurfaceContainer,
                            const XWaylandParentResolution &resolution)
{
    if (!surface || !rootSurfaceContainer)
        return;

    constexpr int maxParentDepth = 16;
    auto *parent = surface->parentXWaylandSurface();
    for (int depth = 0; parent && depth < maxParentDepth;
         ++depth, parent = parent->parentXWaylandSurface()) {
        auto *parentSurface = (!parent->isInvalidated()) ? parent->surface() : nullptr;
        auto *parentWrapper = parentSurface ? rootSurfaceContainer->getSurface(parentSurface) : nullptr;

        qCDebug(lcTlXwayland) << "[XWL_PARENT_CHAIN] XWayland parent chain:"
                               << "window_id=" << xwaylandWindowId(surface)
                               << "depth=" << depth
                               << "parent_window_id=" << xwaylandWindowId(parent)
                               << "parent=" << parent
                               << "parent_surface=" << parentSurface
                               << "parent_wrapper=" << parentWrapper
                               << "parent_geometry=" << xwaylandGeometry(parent)
                               << "parent_request_geometry="
                               << xwaylandRequestConfigureGeometry(parent)
                               << "resolved_here=" << (parent == resolution.resolvedParent)
                               << "chain_truncated=" << resolution.chainTruncated;

        if (parent == surface || parent->isInvalidated())
            break;
    }
}

QQuickItem *qtStackSiblingForTransient(SurfaceWrapper *wrapper, SurfaceWrapper *parentWrapper)
{
    if (!wrapper || !parentWrapper || !wrapper->parentItem())
        return nullptr;

    QQuickItem *sibling = parentWrapper;
    for (auto *child : parentWrapper->subSurfaces()) {
        if (child == wrapper)
            break;
        if (!child)
            continue;

        auto *last = child->stackLastSurface();
        if (last && last->parentItem() == wrapper->parentItem())
            sibling = last;
    }

    return sibling;
}

WXWaylandSurface *xwaylandRestackSiblingForTransient(SurfaceWrapper *wrapper,
                                                     SurfaceWrapper *parentWrapper)
{
    if (!wrapper || !parentWrapper)
        return nullptr;

    auto *sibling = managedXwaylandShellSurface(parentWrapper);
    for (auto *child : parentWrapper->subSurfaces()) {
        if (child == wrapper)
            break;
        if (auto *childSurface = managedXwaylandShellSurface(child))
            sibling = childSurface;
    }

    return sibling;
}

bool isCurrentXwaylandAssociation(WXWaylandSurface *surface, WSurface *associatedSurface)
{
    return surface && associatedSurface && surface->surface() == associatedSurface;
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
    if (!wrapper) {
        qCWarning(lcTlXwayland) << "[XWL_PARENT_CONTAINER] Skip container update for null wrapper:"
                                 << "parentSurface=" << parentSurface;
        return;
    }

    if (wrapper->parentSurface())
        wrapper->parentSurface()->removeSubSurface(wrapper);

    auto oldContainer = wrapper->container();
    auto moveToWorkspace = [this, wrapper, oldContainer] {
        if (oldContainer) {
            if (qobject_cast<Workspace *>(oldContainer) == nullptr) {
                oldContainer->removeSurface(wrapper);
                m_workspace->addSurface(wrapper);
            }
            return;
        }

        m_workspace->addSurface(wrapper);
    };

    if (parentSurface) {
        auto parentWrapper = m_rootSurfaceContainer->getSurface(parentSurface);
        if (!parentWrapper || parentWrapper == wrapper || !parentWrapper->container()) {
            qCWarning(lcTlXwayland)
                << "[XWL_PARENT_CONTAINER] Parent wrapper unavailable, keeping child in workspace:"
                << "wrapper=" << wrapper
                << "parentSurface=" << parentSurface
                << "parentWrapper=" << parentWrapper
                << "parentContainer=" << (parentWrapper ? parentWrapper->container() : nullptr);
            moveToWorkspace();
            return;
        }

        auto parentContainer = qobject_cast<SurfaceContainer *>(parentWrapper->container());
        if (!parentContainer) {
            qCWarning(lcTlXwayland)
                << "[XWL_PARENT_CONTAINER] Parent container is not a SurfaceContainer, keeping child in workspace:"
                << "wrapper=" << wrapper
                << "parentSurface=" << parentSurface
                << "parentWrapper=" << parentWrapper
                << "parentContainer=" << parentWrapper->container();
            moveToWorkspace();
            return;
        }

        parentWrapper->addSubSurface(wrapper);

        SurfaceContainer *targetContainer = parentContainer;
        bool targetNeedsManualInitialization = false;
        auto *xwaylandSurface = xwaylandShellSurface(wrapper);
        const bool managedXWaylandPopup =
            wrapper->isXWaylandPopupLikeTransient() && xwaylandSurface
            && !xwaylandSurface->isBypassManager();
        if (wrapper->isXWaylandPopupLikeTransient()) {
            if (managedXWaylandPopup) {
                targetContainer = parentContainer;
            } else {
                QQuickItem *ancestorContainer = parentWrapper->container();
                while (ancestorContainer
                       && ancestorContainer->parentItem() != m_popupContainer->parentItem()) {
                    ancestorContainer = ancestorContainer->parentItem();
                }

                if (ancestorContainer && ancestorContainer->z() > m_popupContainer->z()) {
                    wrapper->setZ(5);
                    targetContainer = parentContainer;
                } else {
                    targetContainer = m_popupContainer;
                }
            }
            targetNeedsManualInitialization = true;
        }

        qCDebug(lcTlXwayland) << "[XWL_POPUP_ROUTE] XWayland parent container route:"
                               << "wrapper=" << wrapper
                               << "parentSurface=" << parentSurface
                               << "parentWrapper=" << parentWrapper
                               << "oldContainer=" << oldContainer
                               << "parentContainer=" << parentContainer
                               << "targetContainer=" << targetContainer
                               << "popup_like=" << wrapper->isXWaylandPopupLikeTransient()
                               << "managed_xwayland_popup=" << managedXWaylandPopup
                               << "bypass_manager="
                               << (xwaylandSurface ? xwaylandSurface->isBypassManager() : false)
                               << "wrapper_z=" << wrapper->z()
                               << "wrapper_geometry=" << wrapper->geometry()
                               << "parent_geometry=" << parentWrapper->geometry();

        if (oldContainer != targetContainer) {
            if (oldContainer)
                oldContainer->removeSurface(wrapper);
            if (auto ws = qobject_cast<Workspace *>(targetContainer))
                ws->addSurface(wrapper, parentWrapper->workspaceId());
            else
                targetContainer->addSurface(wrapper);
        }

        if (targetNeedsManualInitialization) {
            wrapper->setHasInitializeContainer(true);
            wrapper->setOwnsOutput(parentWrapper->ownsOutput());
        }

        if (managedXWaylandPopup) {
            const bool sameQuickParent = wrapper->parentItem() == parentWrapper->parentItem();
            auto *qtSibling = qtStackSiblingForTransient(wrapper, parentWrapper);
            const bool qtStacked =
                sameQuickParent && qtSibling && wrapper != qtSibling && wrapper->stackAfter(qtSibling);
            auto *x11Sibling = xwaylandRestackSiblingForTransient(wrapper, parentWrapper);
            if (x11Sibling && x11Sibling != xwaylandSurface) {
                xwaylandSurface->restack(x11Sibling, WXWaylandSurface::XCB_STACK_MODE_ABOVE);
            } else {
                xwaylandSurface->restack(nullptr, WXWaylandSurface::XCB_STACK_MODE_ABOVE);
            }

            qCDebug(lcTlXwayland) << "[XWL_POPUP_STACK] XWayland managed popup transient stack sync:"
                                   << "window_id=" << xwaylandWindowId(xwaylandSurface)
                                   << "wrapper=" << wrapper
                                   << "parent_window_id="
                                   << xwaylandWindowId(managedXwaylandShellSurface(parentWrapper))
                                   << "parentWrapper=" << parentWrapper
                                   << "same_qquick_parent=" << sameQuickParent
                                   << "qt_stack_sibling=" << qtSibling
                                   << "qt_stacked=" << qtStacked
                                   << "x11_sibling_window_id=" << xwaylandWindowId(x11Sibling)
                                   << "x11_sibling=" << x11Sibling
                                   << "targetContainer=" << targetContainer
                                   << "wrapper_geometry=" << wrapper->geometry()
                                   << "parent_geometry=" << parentWrapper->geometry();

            if (!sameQuickParent || !qtStacked) {
                qCWarning(lcTlXwayland)
                    << "[XWL_POPUP_STACK] XWayland managed popup QtQuick stack sync incomplete:"
                    << "window_id=" << xwaylandWindowId(xwaylandSurface)
                    << "wrapper=" << wrapper
                    << "parentWrapper=" << parentWrapper
                    << "same_qquick_parent=" << sameQuickParent
                    << "qt_stack_sibling=" << qtSibling
                    << "targetContainer=" << targetContainer;
            }
        }
    } else {
        auto *xwaylandSurface = xwaylandShellSurface(wrapper);
        const bool oldContainerIsWorkspace = qobject_cast<Workspace *>(oldContainer) != nullptr;
        const bool unresolvedXWaylandPopupParent =
            xwaylandSurface && wrapper->isXWaylandPopupLikeTransient()
            && xwaylandSurface->parentXWaylandSurface();
        if (unresolvedXWaylandPopupParent) {
            if (oldContainer != m_popupContainer) {
                if (oldContainer)
                    oldContainer->removeSurface(wrapper);
                m_popupContainer->addSurface(wrapper);
            }

            wrapper->setHasInitializeContainer(true);

            qCDebug(lcTlXwayland)
                << "[XWL_POPUP_ROUTE] XWayland unresolved-parent popup route:"
                << "window_id=" << xwaylandWindowId(xwaylandSurface)
                << "parent_window_id="
                << xwaylandWindowId(xwaylandSurface->parentXWaylandSurface())
                << "wrapper=" << wrapper
                << "oldContainer=" << oldContainer
                << "targetContainer=" << m_popupContainer
                << "bypass_manager=" << xwaylandSurface->isBypassManager()
                << "window_types=" << xwaylandSurface->windowTypes()
                << "wrapper_geometry=" << wrapper->geometry();
            return;
        }

        if (xwaylandSurface && oldContainer && !oldContainerIsWorkspace
            && (xwaylandSurface->isBypassManager() || wrapper->isXWaylandPopupLikeTransient()
                || oldContainer == m_popupContainer)) {
            qCDebug(lcTlXwayland)
                << "[XWL_PARENT_CONTAINER] Keep parentless XWayland popup in current container:"
                << "window_id=" << xwaylandWindowId(xwaylandSurface)
                << "wrapper=" << wrapper
                << "oldContainer=" << oldContainer
                << "bypass_manager=" << xwaylandSurface->isBypassManager()
                << "popup_like=" << wrapper->isXWaylandPopupLikeTransient()
                << "window_types=" << xwaylandSurface->windowTypes();
            return;
        }

        moveToWorkspace();
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
    if (surface && surface->handle() && surface->handle()->handle()) {
        qCInfo(lcTlXwayland) << "[CREATE] XWayland surface added:"
                              << "window_id=" << surface->handle()->handle()->window_id
                              << "surface=" << surface
                              << "parent_window_id="
                              << xwaylandWindowId(surface->parentXWaylandSurface())
                              << "geometry=" << xwaylandGeometry(surface)
                              << "window_types=" << surface->windowTypes()
                              << "input_model=" << xwaylandInputModelName(xwaylandInputModel(surface))
                              << "bypass_manager=" << surface->isBypassManager()
                              << "supports_wm_take_focus="
                              << xwaylandSupportsWmTakeFocus(surface);
    } else {
        qCWarning(lcTlXwayland) << "[CREATE] XWayland surface added with null handle:" << surface;
    }

    surface->safeConnect(&WXWaylandSurface::focusIn,
                         this,
                         [surface = QPointer<WXWaylandSurface>(surface)] {
                             if (auto *raw = surface.data())
                                 Helper::instance()->acceptXWaylandFocus(raw, false);
                         });
    surface->safeConnect(&WXWaylandSurface::grabFocus,
                         this,
                         [surface = QPointer<WXWaylandSurface>(surface)] {
                             if (auto *raw = surface.data())
                                 Helper::instance()->acceptXWaylandFocus(raw, true);
                         });

    surface->safeConnect(&WXWaylandSurface::associated,
                         this,
                         [this, surface = QPointer<WXWaylandSurface>(surface)] {
                             auto raw = surface.data();
                             if (!raw)
                                 return; // surface destroyed before callback

                             qCInfo(lcTlXwayland) << "[ASSOCIATE] XWayland surface associated:"
                                                   << "window_id=" << (raw->handle() && raw->handle()->handle()
                                                                       ? raw->handle()->handle()->window_id : 0)
                                                   << "parent_window_id="
                                                   << xwaylandWindowId(raw->parentXWaylandSurface())
                                                   << "geometry=" << xwaylandGeometry(raw)
                                                   << "window_types=" << raw->windowTypes()
                                                   << "input_model="
                                                   << xwaylandInputModelName(xwaylandInputModel(raw))
                                                   << "bypass_manager=" << raw->isBypassManager()
                                                   << "supports_wm_take_focus="
                                                   << xwaylandSupportsWmTakeFocus(raw)
                                                   << "prelaunchWrappers=" << m_prelaunchWrappers.size()
                                                   << "closedSplashAppIds=" << m_closedSplashAppIds;

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

        uint32_t windowId = 0;
        if (surface && surface->handle() && surface->handle()->handle()) {
            windowId = surface->handle()->handle()->window_id;
        }
        qCInfo(lcTlXwayland) << "[DISSOCIATE] XWayland surface dissociating:"
                              << "window_id=" << windowId
                              << "parent_window_id="
                              << xwaylandWindowId(surface->parentXWaylandSurface())
                              << "wrapper=" << wrapper
                              << "geometry=" << xwaylandGeometry(surface)
                              << "window_types=" << surface->windowTypes()
                              << "input_model=" << xwaylandInputModelName(xwaylandInputModel(surface))
                              << "bypass_manager=" << surface->isBypassManager()
                              << "supports_wm_take_focus="
                              << xwaylandSupportsWmTakeFocus(surface);
        Helper::instance()->clearXWaylandPopupFocusState(surface);

        // Cancel pending async property fetch for this surface.
        auto *xwayland = surface->xwayland();
        if (xwayland && windowId != 0) {
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
    auto *associatedSurface = surface ? surface->surface() : nullptr;
    if (!isCurrentXwaylandAssociation(surface, associatedSurface)) {
        qCDebug(lcTlXwayland) << "[XWL_ASSOCIATION_SKIP] Skip initial property fetch for unassociated surface:"
                               << "window_id=" << xwaylandWindowId(surface)
                               << "surface=" << surface
                               << "associatedSurface=" << associatedSurface;
        return;
    }

    auto *xwayland = surface->xwayland();
    if (!xwayland) {
        ensureXwaylandWrapper(surface, appId, associatedSurface);
        return;
    }

    auto windowId = xwaylandWindowId(surface);
    QVector<WXWayland::AsyncPropRequest> requests;
    if (m_imCandidatePanelManager) {
        requests.append({ m_imCandidatePanelManager->imCandidatePanelAtom(), XCB_ATOM_CARDINAL });
    }

    if (requests.isEmpty()) {
        ensureXwaylandWrapper(surface, appId, associatedSurface);
        return;
    }

    xwayland->readAsyncProperties(
        windowId,
        requests,
        50,
        [self = QPointer<ShellHandler>(this),
         surface = QPointer<WXWaylandSurface>(surface),
         associatedSurface = QPointer<WSurface>(associatedSurface),
         windowId,
         appId](xcb_window_t, const QMap<xcb_atom_t, QByteArray> &result) {
            auto *raw = surface.data();
            auto *expectedSurface = associatedSurface.data();
            if (!raw || !self)
                return;
            if (!isCurrentXwaylandAssociation(raw, expectedSurface)) {
                qCDebug(lcTlXwayland) << "[XWL_ASSOCIATION_SKIP] Drop stale initial property reply:"
                                       << "window_id=" << windowId
                                       << "surface=" << raw
                                       << "expectedSurface=" << expectedSurface
                                       << "currentSurface=" << raw->surface();
                return;
            }
            self->onInitialPropertiesReady(raw, expectedSurface, appId, result);
        });
}

void ShellHandler::onInitialPropertiesReady(WXWaylandSurface *surface,
                                            WSurface *associatedSurface,
                                            const QString &appId,
                                            const QMap<xcb_atom_t, QByteArray> &result)
{
    if (!isCurrentXwaylandAssociation(surface, associatedSurface)) {
        qCDebug(lcTlXwayland) << "[XWL_ASSOCIATION_SKIP] Drop stale initial properties before wrapper creation:"
                               << "window_id=" << xwaylandWindowId(surface)
                               << "surface=" << surface
                               << "expectedSurface=" << associatedSurface
                               << "currentSurface=" << (surface ? surface->surface() : nullptr);
        return;
    }

    if (m_imCandidatePanelManager) {
        bool value = IMCandidatePanelManager::parseIMCandidatePanelProperty(
            result,
            m_imCandidatePanelManager->imCandidatePanelAtom());
        surface->setProperty("imCandidatePanel", value);
    }
    ensureXwaylandWrapper(surface, appId, associatedSurface);
}

void ShellHandler::ensureXwaylandWrapper(WXWaylandSurface *surface,
                                         const QString &targetAppId,
                                         WSurface *associatedSurface)
{
    if (!isCurrentXwaylandAssociation(surface, associatedSurface)) {
        qCDebug(lcTlXwayland) << "[XWL_WRAPPER_SKIP] Skip wrapper creation for stale XWayland association:"
                               << "window_id=" << xwaylandWindowId(surface)
                               << "surface=" << surface
                               << "expectedSurface=" << associatedSurface
                               << "currentSurface=" << (surface ? surface->surface() : nullptr);
        return;
    }

    // Check if this matches a closed splash screen
    if (!targetAppId.isEmpty() && m_closedSplashAppIds.contains(targetAppId)) {
        qCWarning(lcTlXwayland) << "[CLOSE_BY_SPLASH] XWayland surface closed due to closedSplashAppIds match:"
                                 << "appId=" << targetAppId
                                 << "window_id=" << xwaylandWindowId(surface);
        m_closedSplashAppIds.remove(targetAppId);
        surface->close();
        return;
    }

    SurfaceWrapper *wrapper = nullptr;
    bool isNewWrapper = true;

    wrapper = m_rootSurfaceContainer->getSurface(surface);
    if (!wrapper)
        wrapper = m_rootSurfaceContainer->getSurface(associatedSurface);

    if (wrapper) {
        qCDebug(lcTlXwayland) << "[WRAPPER_REUSE] Reusing existing XWayland wrapper:"
                               << "window_id=" << xwaylandWindowId(surface)
                               << "wrapper=" << wrapper
                               << "surface=" << surface
                               << "associatedSurface=" << associatedSurface;
        if (m_imCandidatePanelManager)
            m_imCandidatePanelManager->checkAndApplyIMCandidatePanel(wrapper, surface);
        return;
    }

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
        qCInfo(lcTlXwayland) << "[WRAPPER_CREATE] New XWayland wrapper created:"
                              << "wrapper=" << wrapper
                              << "appId=" << targetAppId
                              << "window_id=" << xwaylandWindowId(surface)
                              << "parent_window_id="
                              << xwaylandWindowId(surface->parentXWaylandSurface())
                              << "geometry=" << xwaylandGeometry(surface)
                              << "window_types=" << surface->windowTypes()
                              << "input_model=" << xwaylandInputModelName(xwaylandInputModel(surface))
                              << "bypass_manager=" << surface->isBypassManager()
                              << "supports_wm_take_focus="
                              << xwaylandSupportsWmTakeFocus(surface);
    }

    // IM candidate panel detection via XWayland xprop
    if (m_imCandidatePanelManager
        && m_imCandidatePanelManager->checkAndApplyIMCandidatePanel(wrapper, surface)) {
        Q_EMIT surfaceWrapperAdded(wrapper);
        return;
    }

    // Initialize wrapper
    auto updateSurfaceWithParentContainer = [this,
                                             wrapper = QPointer<SurfaceWrapper>(wrapper),
                                             surface = QPointer<WXWaylandSurface>(surface)] {
        auto *rawWrapper = wrapper.data();
        auto *rawSurface = surface.data();
        if (!rawWrapper || !rawSurface)
            return;
        if (!rawSurface->surface()) {
            qCDebug(lcTlXwayland) << "[XWL_PARENT_CONTAINER] Skip container update for dissociated XWayland surface:"
                                   << "window_id=" << xwaylandWindowId(rawSurface)
                                   << "wrapper=" << rawWrapper
                                   << "surface=" << rawSurface;
            return;
        }

        const auto parentResolution =
            resolveXWaylandParent(rawSurface, m_rootSurfaceContainer.data(), m_xwaylands);
        WSurface *parentSurface = parentResolution.resolvedSurface;
        SurfaceWrapper *parentWrapper = parentResolution.resolvedWrapper;
        qCDebug(lcTlXwayland) << "[XWL_PARENT_CONTAINER] XWayland container update:"
                               << "window_id=" << xwaylandWindowId(rawSurface)
                               << "wrapper=" << rawWrapper
                               << "direct_parent_window_id="
                               << xwaylandWindowId(parentResolution.directParent)
                               << "resolved_parent_window_id="
                               << xwaylandWindowId(parentResolution.resolvedParent)
                               << "resolved_depth=" << parentResolution.resolvedDepth
                               << "chain_truncated=" << parentResolution.chainTruncated
                               << "used_x11_tree_parent="
                               << parentResolution.usedX11TreeParent
                               << "x11_tree_direct_window_id="
                               << parentResolution.x11TreeDirectWindow
                               << "x11_tree_resolved_window_id="
                               << parentResolution.x11TreeResolvedWindow
                               << "x11_tree_resolved_depth="
                               << parentResolution.x11TreeResolvedDepth
                               << "x11_tree_queried=" << parentResolution.x11TreeQueried
                               << "x11_tree_truncated="
                               << parentResolution.x11TreeTruncated
                               << "x11_tree_reject_reason="
                               << parentResolution.x11TreeRejectReason
                               << "parentSurface=" << parentSurface
                               << "parentWrapper=" << parentWrapper;
        if (parentResolution.directParent
            && (parentResolution.directParent->isInvalidated()
                || parentResolution.directParent->surface() != parentResolution.resolvedSurface)) {
            logXWaylandParentChain(rawSurface, m_rootSurfaceContainer.data(), parentResolution);
        }
        updateWrapperContainer(rawWrapper, parentSurface);
    };
    surface->safeConnect(&WXWaylandSurface::parentXWaylandSurfaceChanged,
                         this,
                         updateSurfaceWithParentContainer);
    updateSurfaceWithParentContainer();

    for (auto *child : surface->children()) {
        if (!child || !child->surface())
            continue;

        auto *childWrapper = m_rootSurfaceContainer->getSurface(child->surface());
        if (!childWrapper)
            continue;

        qCDebug(lcTlXwayland) << "[XWL_PARENT_CONTAINER] Retrying XWayland child container update:"
                               << "parentWrapper=" << wrapper
                               << "childWrapper=" << childWrapper
                               << "childSurface=" << child->surface();
        updateWrapperContainer(childWrapper, surface->surface());
    }

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
        auto activateWrapper = [this, wrapper]() {
            if (wrapper->isXWaylandPopupLikeTransient()) {
                qCDebug(lcTlXwayland) << "[XWL_AUTO_ACTIVATE_SKIP] Keep parent focus for XWayland popup-like transient:"
                                       << "wrapper=" << wrapper
                                       << "parentWrapper=" << wrapper->parentSurface()
                                       << "surface=" << wrapper->shellSurface();
                return;
            }

            if (wrapper->showOnWorkspace(m_workspace->current()->id()))
                Helper::instance()->activateSurface(wrapper);
            else
                m_workspace->pushActivedSurface(wrapper);
        };

        connect(wrapper, &SurfaceWrapper::activationRequested, this, activateWrapper);

        connect(wrapper, &SurfaceWrapper::inactivationRequested, this, [this, wrapper]() {
            onSurfaceInactivationRequested(wrapper);
        });

        if (wrapper->hasActiveCapability()) {
            activateWrapper();
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
