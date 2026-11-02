// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <utility>
#include "desktopenvironment.h"
#include "core/waylandserver.h"
#include "core/qmlengine.h"
#include "core/shellhandler.h"
#include "core/rootsurfacecontainer.h"
#include "session/sessionswitcher.h"
#include "session/sessioninputchannel.h"
#include "session/sessionoutputmanager.h"
#include "modules/foreign-toplevel/foreigntoplevelmanagerv3.h"
#include "surface/surfacewrapper.h"
#include "workspace/workspace.h"
#include <woutputrenderwindow.h>

DesktopEnvironment::DesktopEnvironment(TreelandUserConfig *config, Workspace *workspace,
                                       WaylandServer *runtime,
                                       InputManager::ConfigProvider seatConfiguration,
                                       InputManager::SeatProvider seats,
                                       WallpaperManager::LibraryProvider library, QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_workspace(workspace)
    , m_runtime(runtime)
    , m_windows(new DesktopSurfaceModel(this))
    , m_surfaces(new DesktopSurfaceModel(this))
    , m_input(new InputManager(std::move(seatConfiguration), std::move(seats), runtime->seatManager(), runtime->backend(), this))
    , m_wallpaper(new WallpaperManager(config, workspace, {
        std::move(library),
        [runtime]() -> const QList<Output *> & { return runtime->outputs(); },
        [runtime] { return runtime->wallpaperNotifier(); },
        runtime->shellHandler()->wallpaperShell()
      }, this))
    , m_switcher(new SessionSwitcher(workspace, runtime->qmlEngine(), runtime, this))
    , m_viewRoot(new QQuickItem(runtime->rootSurfaceContainer()))
{
    m_viewRoot->setParent(this);
    auto *engine = runtime->qmlEngine();
    // C++ visual parents have no QML creation context unless explicitly set.
    engine->setContextForObject(m_viewRoot, engine->rootContext());
    QQmlEngine::setObjectOwnership(m_viewRoot, QQmlEngine::CppOwnership);
    m_viewRoot->setZ(RootSurfaceContainer::OverlayZOrder);
    m_viewRoot->setVisible(false);
    m_viewRoot->setEnabled(false);
    connect(runtime, &WaylandServer::activatedSurfaceChanged,
            this, &DesktopEnvironment::updateActivatedWindow);
}

DesktopEnvironment::~DesktopEnvironment()
{
    delete m_viewRoot;
}

TreelandUserConfig *DesktopEnvironment::config() const { return m_config; }
Workspace *DesktopEnvironment::workspace() const { return m_workspace; }
DesktopSurfaceModel *DesktopEnvironment::windows() const { return m_windows; }
InputManager *DesktopEnvironment::input() const { return m_input; }
WallpaperManager *DesktopEnvironment::wallpaper() const { return m_wallpaper; }
SessionSwitcher *DesktopEnvironment::switcher() const { return m_switcher; }
SessionOutputManager *DesktopEnvironment::outputs() const { return m_outputs; }
ForeignToplevelManagerInterfaceV3 *DesktopEnvironment::windowManagement() const { return m_windowManagement; }
SurfaceWrapper *DesktopEnvironment::activatedWindow() const { return m_activatedWindow; }
QQuickItem *DesktopEnvironment::viewRoot() const { return m_viewRoot; }

void DesktopEnvironment::attachOutputs(SessionOutputManager *outputs)
{
    Q_ASSERT(!m_outputs && outputs);
    m_outputs = outputs;
    outputs->setParent(this);
}

void DesktopEnvironment::attachWindowManagement(ForeignToplevelManagerInterfaceV3 *manager)
{
    Q_ASSERT(!m_windowManagement && manager);
    m_windowManagement = manager;
    // Views are permanently wired to this desktop's models and protocol.
    m_dockPreview = m_runtime->qmlEngine()->createDockPreview(m_viewRoot, this);
    connect(manager, &ForeignToplevelManagerInterfaceV3::requestDockClose,
            this, &DesktopEnvironment::closeDockPreview);
    connect(manager, &ForeignToplevelManagerInterfaceV3::requestDockPreview,
            this, &DesktopEnvironment::showDockPreview);
    connect(manager, &ForeignToplevelManagerInterfaceV3::requestDockPreviewTooltip,
            this, &DesktopEnvironment::showDockTooltip);
}

void DesktopEnvironment::activate()
{
    m_viewRoot->setEnabled(true);
    m_viewRoot->setVisible(true);
    m_switcher->activate();
    m_wallpaper->refresh();
    updateActivatedWindow();
}

void DesktopEnvironment::deactivate()
{
    m_switcher->deactivate();
    closeDockPreview();
    m_viewRoot->setEnabled(false);
    m_viewRoot->setVisible(false);
}

void DesktopEnvironment::activateWindow(SurfaceWrapper *window, int reason)
{
    if (m_windows->hasObject(window))
        m_runtime->forceActivateSurface(window, static_cast<Qt::FocusReason>(reason));
}

void DesktopEnvironment::updateActivatedWindow()
{
    auto *window = m_runtime->activatedSurface();
    if (!m_windows->hasObject(window))
        window = nullptr;
    if (m_activatedWindow == window)
        return;
    m_activatedWindow = window;
    Q_EMIT activatedWindowChanged();
}

void DesktopEnvironment::closeDockPreview()
{
    if (m_dockPreview)
        QMetaObject::invokeMethod(m_dockPreview, "close");
}

void DesktopEnvironment::showDockPreview(std::vector<SurfaceWrapper *> surfaces, WSurface *target,
                                        QPoint position, ForeignToplevelManagerInterfaceV3::PreviewDirection direction)
{
    auto *window = m_runtime->rootContainer()->getSurface(target);
    if (!m_surfaces->hasObject(window))
        return;
    QMetaObject::invokeMethod(m_dockPreview, "show", QVariant::fromValue(surfaces),
        QVariant::fromValue(window), QVariant::fromValue(position), QVariant::fromValue(direction));
}

void DesktopEnvironment::showDockTooltip(WSurface *surface, WSurface *target, QPoint position,
                                        ForeignToplevelManagerInterfaceV3::PreviewDirection direction)
{
    auto *window = m_runtime->rootContainer()->getSurface(target);
    if (!m_surfaces->hasObject(window))
        return;
    QMetaObject::invokeMethod(m_dockPreview, "showTooltip", QVariant::fromValue(surface),
        QVariant::fromValue(window), QVariant::fromValue(position), QVariant::fromValue(direction));
}

void DesktopEnvironment::registerWindow(SurfaceWrapper *window)
{
    m_windows->addObject(window);
}

void DesktopEnvironment::unregisterWindow(SurfaceWrapper *window)
{
    m_windows->removeObject(window);
    m_surfaces->removeObject(window);
    if (m_activatedWindow == window) {
        m_activatedWindow = nullptr;
        Q_EMIT activatedWindowChanged();
    }
}

DesktopSurfaceModel *DesktopEnvironment::surfaces() const { return m_surfaces; }
void DesktopEnvironment::registerSurface(SurfaceWrapper *surface) { m_surfaces->addObject(surface); }

SessionInputChannel *DesktopEnvironment::inputChannel() const { return m_inputChannel; }
void DesktopEnvironment::attachInputChannel(SessionInputChannel *channel)
{
    Q_ASSERT(!m_inputChannel && channel);
    m_inputChannel = channel;
    m_switcher->setInputFilter(channel->filter());
    m_switcher->setShortcutController(channel->shortcuts(), channel->shortcutProtocol());
}
