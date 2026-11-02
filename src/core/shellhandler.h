// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wlr_fwd.h>
#include "modules/foreign-toplevel/foreigntoplevelmanagerv3.h"

#include <xcb/xcb.h>

#include <wglobal.h>

#include <QHash>
#include <QList>
#include <QMap>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QSize>

Q_MOC_INCLUDE("workspace/workspace.h")
Q_MOC_INCLUDE(<wlayersurface.h>)
Q_MOC_INCLUDE(<wxdgtoplevelsurface.h>)
#ifndef DISABLE_XWAYLAND
Q_MOC_INCLUDE(<wxwaylandsurface.h>)
#endif

class Session;
class TreelandUserConfig;
class WaylandServer;
class SurfaceWrapper;
class RootSurfaceContainer;
class LayerSurfaceContainer;
class Output;
class Workspace;
class SurfaceContainer;
class IMCandidatePanelManager;
class QmlEngine;
class ForeignToplevelManagerInterfaceV3;
class SessionPrelaunch;
class WineWindowStateManager;
class WineWindowManager;
class SnapTargetV1;

WAYLIB_SERVER_BEGIN_NAMESPACE
class WServer;
class WXdgToplevelSurface;
class WXdgPopupSurface;
class WXdgShell;
class WLayerShell;
class WLayerSurface;
#ifndef DISABLE_XWAYLAND
class WXWayland;
#endif
class WToplevelSurface; // forward declare base toplevel
class WInputMethodHelper;
class WInputPopupSurface;
class WSeat;
class WSurface;
#ifndef DISABLE_XWAYLAND
class WXWaylandSurface;
#endif
WAYLIB_SERVER_END_NAMESPACE

QT_BEGIN_NAMESPACE
class QQuickItem;
class QQuickWindow;
QT_END_NAMESPACE

class AppIdResolverManager; // forward declare new protocol manager
class WindowConfigStore;    // forward declare config store
class TreelandWallpaperShellInterfaceV1;
class TreelandWallpaperSurfaceInterfaceV1;
class LayerShellExtensionManagerInterfaceV1;

class ShellHandler : public QObject
{
    Q_OBJECT

public:
    explicit ShellHandler(RootSurfaceContainer *rootContainer,
                          WAYLIB_SERVER_NAMESPACE::WServer *server);
    [[nodiscard]] Workspace *workspace() const;
    Workspace *createSessionWorkspace(TreelandUserConfig *config);
    void setWorkspace(Workspace *workspace);
    Workspace *workspaceForSurface(SurfaceWrapper *wrapper) const;
    [[nodiscard]] LayerSurfaceContainer *backgroundContainer() const { return m_backgroundContainer; }
    [[nodiscard]] SurfaceContainer *popupContainer() const;
    [[nodiscard]] SurfaceContainer *privilegedOverlayContainer() const;
    [[nodiscard]] SurfaceContainer *snapMaskContainer() const;
    [[nodiscard]] RootSurfaceContainer *rootSurfaceContainer() const;
    [[nodiscard]] ForeignToplevelManagerInterfaceV3 *foreignToplevel() const;
    void registerSession(Session *session);

    void createComponent(QmlEngine *engine, QQuickItem *parentItem);
    void init(WAYLIB_SERVER_NAMESPACE::WServer *server, WAYLIB_SERVER_NAMESPACE::WSeat *seat);
#ifndef DISABLE_XWAYLAND
    [[nodiscard]] WAYLIB_SERVER_NAMESPACE::WXWayland *createXWayland(
        WAYLIB_SERVER_NAMESPACE::WServer *server,
        WAYLIB_SERVER_NAMESPACE::WSeat *seat,
        wlr_compositor *compositor,
        bool lazy);
    // FIXME: never call removeXWayland in treeland.cpp
    void removeXWayland(WAYLIB_SERVER_NAMESPACE::WXWayland *xwayland);

    WAYLIB_SERVER_NAMESPACE::WXWayland *defaultXWaylandSocket() const;
#endif

    TreelandWallpaperShellInterfaceV1 *wallpaperShell() const
    {
        return m_wallpaperShell;
    }

    [[nodiscard]] WAYLIB_SERVER_NAMESPACE::WInputMethodHelper *inputMethodHelper(
        WAYLIB_SERVER_NAMESPACE::WSeat *seat) const;
    void registerInputMethodHelper(WAYLIB_SERVER_NAMESPACE::WInputMethodHelper *helper);
    IMCandidatePanelManager *candidatePanels(Session *session) const;

Q_SIGNALS:
    void workspaceChanged();
    void workspaceAdded();
    void surfaceWrapperAdded(SurfaceWrapper *wrapper);
    void surfaceWrapperAboutToRemove(SurfaceWrapper *wrapper);

private Q_SLOTS:
    void onXdgToplevelSurfaceAdded(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface);
    void onXdgToplevelSurfaceRemoved(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface);

    void onXdgPopupSurfaceAdded(WAYLIB_SERVER_NAMESPACE::WXdgPopupSurface *surface);
    void onXdgPopupSurfaceRemoved(WAYLIB_SERVER_NAMESPACE::WXdgPopupSurface *surface);

#ifndef DISABLE_XWAYLAND
    void onXWaylandSurfaceAdded(WAYLIB_SERVER_NAMESPACE::WXWaylandSurface *surface);
    void onXWaylandSurfaceRemoved(WAYLIB_SERVER_NAMESPACE::WXWaylandSurface *surface);
#endif

    void onLayerSurfaceAdded(WAYLIB_SERVER_NAMESPACE::WLayerSurface *surface);
    void onLayerSurfaceRemoved(WAYLIB_SERVER_NAMESPACE::WLayerSurface *surface);

    void onInputPopupSurfaceV2Added(WAYLIB_SERVER_NAMESPACE::WInputPopupSurface *surface);

    void onInputPopupSurfaceV2Removed(WAYLIB_SERVER_NAMESPACE::WInputPopupSurface *surface);
    void onWindowMenuClosed();

private:
    void setupSurfaceActiveWatcher(SurfaceWrapper *wrapper);
    void onSurfaceInactivationRequested(SurfaceWrapper *wrapper);
    void updateSurfaceAcceptKeyboardFocus(SurfaceWrapper *wrapper, bool accept);
    void setupSurfaceWindowMenu(SurfaceWrapper *wrapper);
    void updateLayerSurfaceContainer(SurfaceWrapper *surface);
    void registerSurfaceToForeignToplevel(SurfaceWrapper *wrapper);
    bool checkAndApplySnapMask(SurfaceWrapper *wrapper);
    void handleDdeShellSurfaceAdded(WAYLIB_SERVER_NAMESPACE::WSurface *surface,
                                    SurfaceWrapper *wrapper);
    void handleDdeShellSurfaceV2Added(class DDEShellSurfaceV2 *shellSurface,
                                      SurfaceWrapper *wrapper);
#ifndef DISABLE_XWAYLAND
    void updateXWaylandDesktopProperties();
    void watchXWaylandDesktopOutput(Output *output);
    void watchXWaylandWorkspaceNames();
#endif
    void onWorkspaceCountChanged();
    // Resolve launch state from the fixed client domain, never the active desktop.
    SessionPrelaunch *prelaunchForSurface(WAYLIB_SERVER_NAMESPACE::WToplevelSurface *surface) const;
    void initializePrelaunchSurface(SurfaceWrapper *wrapper);
    void ensureXdgWrapper(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface,
                          const QString &targetAppId);
#ifndef DISABLE_XWAYLAND
    void ensureXwaylandWrapper(WAYLIB_SERVER_NAMESPACE::WXWaylandSurface *surface,
                               const QString &targetAppId);
#endif
    void updateWrapperContainer(SurfaceWrapper *wrapper,
                                WAYLIB_SERVER_NAMESPACE::WSurface *parentSurface);
#ifndef DISABLE_XWAYLAND
    // Async X11 property fetch for XWayland surfaces
    void fetchInitialProperties(WAYLIB_SERVER_NAMESPACE::WXWaylandSurface *surface,
                                const QString &appId);
    void onInitialPropertiesReady(WAYLIB_SERVER_NAMESPACE::WXWaylandSurface *surface,
                                  const QString &appId,
                                  const QMap<xcb_atom_t, QByteArray> &result);
#endif
    bool checkAndApplyPrivilegedOverlay(SurfaceWrapper *wrapper);
    void applyPrivilegedOverlay(SurfaceWrapper *wrapper);
    void revokePrivilegedOverlay(SurfaceWrapper *wrapper);
    TreelandWallpaperShellInterfaceV1 *m_wallpaperShell = nullptr;
#ifndef DISABLE_XWAYLAND
    QList<WAYLIB_SERVER_NAMESPACE::WXWayland *> m_xwaylands;
#endif

    QPointer<RootSurfaceContainer> m_rootSurfaceContainer;
    LayerSurfaceContainer *m_backgroundContainer = nullptr;
    LayerSurfaceContainer *m_bottomContainer = nullptr;
    Workspace *m_workspace = nullptr;
    LayerSurfaceContainer *m_topContainer = nullptr;
    LayerSurfaceContainer *m_overlayContainer = nullptr;
    SurfaceContainer *m_popupContainer = nullptr;
    SurfaceContainer *m_privilegedOverlayContainer = nullptr;
    SurfaceContainer *m_snapMaskContainer = nullptr;
    QObject *m_windowMenu = nullptr;
    // Dock preview QML object
    // Pending toplevel surfaces (XDG or XWayland) awaiting async AppId resolve; callbacks continue
    // only if the pointer remains in this list
    QList<WAYLIB_SERVER_NAMESPACE::WToplevelSurface *> m_pendingAppIdResolveToplevels;
    LayerShellExtensionManagerInterfaceV1 *m_layerShellExtensionManagerInterfaceV1 = nullptr;
    WindowConfigStore *m_windowConfigStore = nullptr;
    SnapTargetV1 *m_snapTarget = nullptr;
};
