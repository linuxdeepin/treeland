// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "surface/surfacecontainer.h"
#include "input/inputmanager.h"
#include "wallpaper/wallpapermanager.h"
#include "modules/foreign-toplevel/foreigntoplevelmanagerv3.h"
#include <QPointer>

class Workspace;
class QmlEngine;
class WaylandServer;
class TreelandUserConfig;
class SessionSwitcher;
class SessionInputChannel;
class SessionOutputManager;
class ForeignToplevelManagerInterfaceV3;
class Output;
Q_MOC_INCLUDE("workspace/workspace.h")
Q_MOC_INCLUDE("treelanduserconfig.hpp")
Q_MOC_INCLUDE("session/sessionswitcher.h")
Q_MOC_INCLUDE("session/sessionoutputmanager.h")
Q_MOC_INCLUDE("modules/foreign-toplevel/foreigntoplevelmanagerv3.h")

class DesktopSurfaceModel : public ObjectListModel<SurfaceWrapper>
{
    Q_OBJECT
    QML_ANONYMOUS
public:
    explicit DesktopSurfaceModel(QObject *parent) : ObjectListModel<SurfaceWrapper>("surface", parent) {}
};

// One complete desktop, assembled once with fixed dependencies. Business code
// and QML receive this object; login identity stays outside its public API.
class DesktopEnvironment : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(DesktopEnvironment)
    QML_UNCREATABLE("Created by the desktop composition root")
    Q_PROPERTY(TreelandUserConfig *config READ config CONSTANT FINAL)
    Q_PROPERTY(Workspace *workspace READ workspace CONSTANT FINAL)
    Q_PROPERTY(DesktopSurfaceModel *windows READ windows CONSTANT FINAL)
    Q_PROPERTY(InputManager *input READ input CONSTANT FINAL)
    Q_PROPERTY(WallpaperManager *wallpaper READ wallpaper CONSTANT FINAL)
    Q_PROPERTY(SessionSwitcher *switcher READ switcher CONSTANT FINAL)
    Q_PROPERTY(SessionOutputManager *outputs READ outputs CONSTANT FINAL)
    Q_PROPERTY(ForeignToplevelManagerInterfaceV3 *windowManagement READ windowManagement CONSTANT FINAL)
    Q_PROPERTY(SurfaceWrapper *activatedWindow READ activatedWindow NOTIFY activatedWindowChanged FINAL)
public:
    DesktopEnvironment(TreelandUserConfig *config, Workspace *workspace, WaylandServer *runtime,
                       InputManager::ConfigProvider seatConfiguration, InputManager::SeatProvider seats,
                       WallpaperManager::LibraryProvider library, QObject *parent);
    ~DesktopEnvironment() override;
    TreelandUserConfig *config() const;
    Workspace *workspace() const;
    DesktopSurfaceModel *windows() const;
    DesktopSurfaceModel *surfaces() const;
    void registerSurface(SurfaceWrapper *surface);
    InputManager *input() const;
    WallpaperManager *wallpaper() const;
    SessionSwitcher *switcher() const;
    SessionOutputManager *outputs() const;
    ForeignToplevelManagerInterfaceV3 *windowManagement() const;
    SurfaceWrapper *activatedWindow() const;
    QQuickItem *viewRoot() const;
    SessionInputChannel *inputChannel() const;
    void attachInputChannel(SessionInputChannel *channel);
    void attachOutputs(SessionOutputManager *outputs);
    void attachWindowManagement(ForeignToplevelManagerInterfaceV3 *manager);
    void registerWindow(SurfaceWrapper *window);
    void unregisterWindow(SurfaceWrapper *window);
    void activate();
    void deactivate();
    Q_INVOKABLE void activateWindow(SurfaceWrapper *window, int reason = Qt::OtherFocusReason);

Q_SIGNALS:
    void activatedWindowChanged();

private:
    void updateActivatedWindow();
    void closeDockPreview();
    void showDockPreview(std::vector<SurfaceWrapper *> surfaces,
                         WAYLIB_SERVER_NAMESPACE::WSurface *target, QPoint position,
                         ForeignToplevelManagerInterfaceV3::PreviewDirection direction,
                         ForeignToplevelManagerInterfaceV3::PreviewLayout layout);
    void showDockTooltip(WAYLIB_SERVER_NAMESPACE::WSurface *surface, WAYLIB_SERVER_NAMESPACE::WSurface *target, QPoint position,
                         ForeignToplevelManagerInterfaceV3::PreviewDirection direction);
    TreelandUserConfig *const m_config;
    QPointer<Workspace> m_workspace;
    WaylandServer *const m_runtime;
    DesktopSurfaceModel *const m_windows;
    DesktopSurfaceModel *const m_surfaces;
    InputManager *const m_input;
    WallpaperManager *const m_wallpaper;
    SessionSwitcher *const m_switcher;
    SessionInputChannel *m_inputChannel = nullptr;
    SessionOutputManager *m_outputs = nullptr;
    QPointer<ForeignToplevelManagerInterfaceV3> m_windowManagement;
    QPointer<QQuickItem> m_viewRoot;
    QPointer<QQuickItem> m_dockPreview;
    QPointer<SurfaceWrapper> m_activatedWindow;
};
