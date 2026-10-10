// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "waylandserver.h"
#include "lockscreen/lockscreenmanager.h"
#include "showdesktop/showdesktop.h"
#include "surfaceprotocolcontroller.h"
#include "focuscontroller.h"
#include "input/compositorinputfilter.h"
#include "idle/idlemanager.h"
#include "capture/capturemanager.h"
#include "seat/dragcontroller.h"
#include "pointerconstraintsmanager.h"
#include "seatmanager.h"
#include "common/treelandlogging.h"
#include "common/shellaction.h"
#include "core/qmlengine.h"
#include "core/rootsurfacecontainer.h"
#include "core/shellhandler.h"
#include "dconfig/dconfigmanager.h"
#include "core/treeland.h"
#include "greeter/greeterproxy.h"
#include "greeter/sessionmodel.h"
#include "greeter/usermodel.h"
#include "inputmanager.h"
#include "interfaces/multitaskviewinterface.h"
#include "modules/capture/capture.h"
#include "modules/dde-shell/ddeshellattached.h"
#include "modules/dde-shell/ddeshellmanagerinterfacev1.h"
#include "modules/dde-shell/ddeshellmanagerinterfacev2.h"
#include "modules/ddm/ddminterfacev1.h"
#include "modules/input-manager/inputmanagerinterfacev1.h"
#include "modules/keyboard-shortcuts-inhibit/keyboardshortcutsinhibitmanager.h"
#include "modules/keyboard-state-notify/keyboardstatenotifymanagerinterfacev1.h"
#include "modules/active-notify/activenotifymanagerinterfacev1.h"
#include "modules/output-manager/outputmanagement.h"
#include "modules/output-uuid/outputuuidmanagerinterfacev1.h"
#include "modules/personalization/personalizationmanagerinterfacev1.h"
#include "modules/appearance/appearanceinterfacev1.h"
#include "modules/appearance/appearancemanagerinterfacev1.h"
#include "modules/compositor-action/compositoractioninterfacev1.h"
#include "modules/resource/treelandremotesource.h"
#include "modules/screensaver/screensaverinterfacev2.h"
#include "modules/shortcut/shortcutcontroller.h"
#include "modules/shortcut/shortcutmanager.h"
#include "modules/shortcut/shortcutrunner.h"
#include "modules/wallpaper-color/wallpapercolorinterfacev1.h"
#include "modules/virtual-output/virtualoutputmanagerinterfacev1.h"
#include "output/output.h"
#include "output/outputmanager.h"
#include "seatuserconfig.hpp"
#include "session/session.h"
#include "surface/surfacecontainer.h"
#include "surface/surfacewrapper.h"
#include "treelandconfig.hpp"
#include "treelanduserconfig.hpp"
#include "utils/fpsdisplaymanager.h"
#include "wallpaper/wallpapermanager.h"
#include "wallpapershellinterfacev1.h"
#include "workspace/workspace.h"

#include <xcb/xcb.h>
#include <xcb/xproto.h>

#include <WBackend>
#include <wcursorshapemanagerv1.h>
#include <wremotesubsurfacemanagerv1.h>
#include <wrenderhelper.h>
#include <WInputMethodHelper>
#include <WOutput>
#include <WServer>
#include <WXdgOutput>
#include <wayland-util.h>
#include <woutputmanagerv1.h>
#include <woutputrenderwindow.h>
#include <wpointerconstraintsv1.h>
#include <wqmlcreator.h>
#include <wquickcursor.h>
#include <wseat.h>
#include <wsecuritycontextmanager.h>
#include <wsocket.h>
#include <ext-background-effect-v1-protocol.h>
#include <wlr_all.h>
#include <wxwayland.h>
#include <wxwaylandsurface.h>

#include <DGuiApplicationHelper>

#include <QLoggingCategory>
#include <QQmlContext>
#include <QQuickWindow>
#include <QPropertyAnimation>

#include <unistd.h>
#include <utility>

#define EXT_DATA_CONTROL_MANAGER_V1_VERSION 1
#define WLR_FRACTIONAL_SCALE_V1_VERSION 1
#define EXT_BACKGROUND_EFFECT_MANAGER_V1_VERSION 1

WaylandServer *WaylandServer::m_instance = nullptr;

WaylandServer::WaylandServer(QObject *parent)
    : QObject(parent)
    , WObject()
    , m_sessionManager(new SessionManager(this))
    , m_wallpaperManager(new WallpaperManager(this))
    , m_renderWindow(new WOutputRenderWindow(this))
    , m_server(new WServer(this))
    , m_rootSurfaceContainer(new RootSurfaceContainer(m_renderWindow->contentItem()))
    , m_inputManager(new InputManager(this))
{
    m_idleManager = new IdleManager(this);
    m_captureManager = new CaptureManager(m_rootSurfaceContainer, this);
    m_isDDMDisplay = qEnvironmentVariableIsSet("DDM_DISPLAY_MANAGER");
    Q_ASSERT(!m_instance);
    m_instance = this;

    if (auto *configManager = DConfigManager::instance()) {
        m_config = configManager->initialUserConfig();
        m_globalConfig = configManager->globalConfig();
    }

    Q_ASSERT(m_config);
    Q_ASSERT(m_globalConfig);

    m_renderWindow->setColor(Qt::black);
    m_rootSurfaceContainer->setFlag(QQuickItem::ItemIsFocusScope, true);
    m_rootSurfaceContainer->setFocusPolicy(Qt::StrongFocus);

    m_shellHandler = new ShellHandler(m_rootSurfaceContainer, server());
    m_focusController = new FocusController(this);
    connect(DConfigManager::instance(), &DConfigManager::configReady,
            this, &WaylandServer::applyCurrentUserConfig);
    m_lockScreenManager = new LockScreenManager(this);
    m_surfaceProtocols = new SurfaceProtocolController(this);
    connect(m_shellHandler->workspace(),
            &Workspace::workspaceAdded,
            m_wallpaperManager,
            &WallpaperManager::syncAddWorkspace);
    // Follow the remoteDebug master switch (on by default) in every build type,
    // so the debug socket appears and disappears with it instead of only at
    // startup. The stored value arrives asynchronously -- dtk reports
    // configInitializeSucceed later -- so watch for changes first and only then
    // read the value: reading it now would publish the socket for a user who
    // stored `false`, until the value arrives.
    connect(m_globalConfig,
            &TreelandConfig::remoteDebugChanged,
            this,
            &WaylandServer::tryInitRemoteSource);
    if (DConfigManager::instance()->isInitializeSucceeded())
        tryInitRemoteSource();
    else
        connect(DConfigManager::instance(),
                &DConfigManager::initializeSucceed,
                this,
                &WaylandServer::tryInitRemoteSource);

    m_outputManager = new OutputManager(m_rootSurfaceContainer,
                                        m_globalConfig,
                                        m_wallpaperManager,
                                        m_shellHandler->workspace(),
                                        m_renderWindow,
                                        this);
    connect(m_outputManager, &OutputManager::modeChanged, this, &WaylandServer::outputModeChanged);
    m_showDesktop = new ShowDesktop(this, m_outputManager);
    connect(m_showDesktop, &ShowDesktop::stateChanged,
            this, &WaylandServer::showDesktopStateChanged);
    m_inputFilter = new CompositorInputFilter(this, m_focusController, m_idleManager,
                                              m_captureManager, m_outputManager);
    connect(m_inputFilter, &CompositorInputFilter::modifierKeyReleased,
            this, &WaylandServer::modifierKeyReleased);

    m_workspaceScaleAnimation = new QPropertyAnimation(m_shellHandler->workspace(), "scale", this);
    m_workspaceOpacityAnimation =
        new QPropertyAnimation(m_shellHandler->workspace(), "opacity", this);

    m_workspaceScaleAnimation->setDuration(1000);
    m_workspaceOpacityAnimation->setDuration(1000);
    m_workspaceScaleAnimation->setEasingCurve(QEasingCurve::OutExpo);
    m_workspaceOpacityAnimation->setEasingCurve(QEasingCurve::OutExpo);

    connect(m_renderWindow,
            &QQuickWindow::activeFocusItemChanged,
            this,
            &WaylandServer::onRenderWindowActiveFocusItemChanged);
}

WaylandServer::~WaylandServer()
{
    Q_ASSERT(m_instance == this);
    teardown();

    m_captureManager->teardown();
    m_idleManager->teardown();
    m_instance = nullptr;
    if (m_renderWindow) {
        m_renderWindow->disconnect();
    }

    if (backend()) {
        backend()->disconnect();
    }

    // destroy before m_rootSurfaceContainer
    delete m_shellHandler;
    if (m_rootSurfaceContainer) {
        for (auto s : std::as_const(m_rootSurfaceContainer->surfaces())) {
            if (auto c = s->container())
                c->removeSurface(s);
        }
        delete m_rootSurfaceContainer;
        m_rootSurfaceContainer = nullptr;
    }
    if (auto *configManager = DConfigManager::instance())
        configManager->stopActiveUserConfig();
    delete m_lockScreenManager;
    delete m_surfaceProtocols;
    delete m_dragController;
    delete m_captureManager;
    delete m_idleManager;
}

WaylandServer *WaylandServer::instance()
{
    return m_instance;
}

TreelandUserConfig *WaylandServer::config()
{
    return m_config;
}

TreelandConfig *WaylandServer::globalConfig()
{
    return m_globalConfig;
}

void WaylandServer::syncPaletteTypeWithWindowColorScheme(int32_t colorScheme)
{
    auto *guiHelper = DTK_GUI_NAMESPACE::DGuiApplicationHelper::instance();
    if (!guiHelper) {
        qCCritical(lcTlConfig) << "DGuiApplicationHelper instance not available, cannot sync "
                                      "palette type with window color scheme.";
        return;
    }

    qCDebug(lcTlConfig) << "Syncing palette type with window color scheme:" << colorScheme;

    switch (colorScheme) {
    case 1:
        guiHelper->setPaletteType(Dtk::Gui::DGuiApplicationHelper::DarkType);
        break;
    case 0:
        guiHelper->setPaletteType(Dtk::Gui::DGuiApplicationHelper::LightType);
        break;
    default:
        qCWarning(lcTlConfig) << "Unknown windowColorScheme:" << colorScheme
                                  << ", fallback to light.";
        guiHelper->setPaletteType(Dtk::Gui::DGuiApplicationHelper::LightType);
        break;
    }
}

void WaylandServer::tryInitRemoteSource()
{
    // remoteDebug is a master switch -- on by default, so the debug socket
    // exists without anyone enabling it, and off means "no debugging at all".
    // Toggling it creates or destroys the remote source without restarting the
    // server. The source itself is a thin QObject that only does work per call,
    // so keeping it while the switch is on costs nothing when no client is
    // attached.
    if (m_treelandRemoteSource) {
        if (!m_globalConfig->remoteDebug()) {
            delete m_treelandRemoteSource;
            m_treelandRemoteSource = nullptr;
        }
    } else if (m_globalConfig->remoteDebug()) {
        auto *source = new TreelandRemoteSource(this);
        // Binding or publishing can fail while the switch stays on; keep it
        // retryable rather than caching an object that never published.
        if (!source->isPublished()) {
            delete source;
            return;
        }
        m_treelandRemoteSource = source;
    }
}

void WaylandServer::setWorkspaceVisible(bool visible)
{
    for (auto *surface : std::as_const(m_rootSurfaceContainer->surfaces())) {
        if (surface->type() == SurfaceWrapper::Type::Layer) {
            surface->setHideByLockScreen(m_currentMode == CurrentMode::LockScreen);
        }
    }

    if (m_noAnimation) {
        m_shellHandler->workspace()->setOpacity(visible ? 1.0 : 0.0);
        m_shellHandler->workspace()->setScale(visible ? 1.0 : 1.4);
        return;
    }

    if (visible) {
        m_workspaceScaleAnimation->stop();
        m_workspaceScaleAnimation->setStartValue(m_shellHandler->workspace()->scale());
        m_workspaceScaleAnimation->setEndValue(1.0);
        m_workspaceScaleAnimation->start();

        m_workspaceOpacityAnimation->stop();
        m_workspaceOpacityAnimation->setStartValue(m_shellHandler->workspace()->opacity());
        m_workspaceOpacityAnimation->setEndValue(1.0);
        m_workspaceOpacityAnimation->start();
    } else {
        m_workspaceScaleAnimation->stop();
        m_workspaceScaleAnimation->setStartValue(m_shellHandler->workspace()->scale());
        m_workspaceScaleAnimation->setEndValue(1.4);
        m_workspaceScaleAnimation->start();

        m_workspaceOpacityAnimation->stop();
        m_workspaceOpacityAnimation->setStartValue(m_shellHandler->workspace()->opacity());
        m_workspaceOpacityAnimation->setEndValue(0.0);
        m_workspaceOpacityAnimation->start();
    }
}

QmlEngine *WaylandServer::qmlEngine() const
{
    return qobject_cast<QmlEngine *>(::qmlEngine(this));
}

WOutputRenderWindow *WaylandServer::window() const
{
    return m_renderWindow;
}

SessionManager *WaylandServer::sessionManager() const
{
    return m_sessionManager;
}

ShellHandler *WaylandServer::shellHandler() const
{
    return m_shellHandler;
}

Workspace *WaylandServer::workspace() const
{
    return m_shellHandler->workspace();
}

bool WaylandServer::surfaceBelongsToCurrentSession(SurfaceWrapper *wrapper)
{
    if (wrapper->type() == SurfaceWrapper::Type::SplashScreen) {
        // TODO(rewine): Determine which user the splash screen belongs to by invoking the client of the prelaunch-splash protocol.
        // Currently, treeland does not support logging in with multiple users at the same time
        // so it is temporarily assumed that the splash screen must belong to the current user.
        return true;
    }
    WClient *client = wrapper->surface()->waylandClient();
    WSocket *socket = client ? client->socket()->rootSocket() : nullptr;
    return socket && socket->isEnabled();
}

void WaylandServer::deleteTaskSwitch()
{
    if (m_taskSwitch) {
        m_taskSwitch->deleteLater();
        m_taskSwitch = nullptr;
    }
    if (m_currentMode == CurrentMode::WindowSwitch) {
        setCurrentMode(CurrentMode::Normal);
    }
}

void WaylandServer::initializeWallpaper()
{
    if (m_wallpaperInitialized) {
        return;
    }

    m_wallpaperInitialized = true;
#ifndef DISABLE_DDM
    const QString userName = m_userModel->currentUserName();
    if (!userName.isEmpty()
        && DConfigManager::instance()->userConfig(userName) != m_config) {
        // LastLogin selected another user whose configuration is still loading.
        // applyCurrentUserConfig() will update the wallpaper once it is ready.
        return;
    }
#endif
    m_wallpaperManager->loadWallpaperConfig();
}

void WaylandServer::applyCurrentUserConfig(const QString &userName,
                                    TreelandUserConfig *config,
                                    SeatUserDConfig *seatConfig)
{
    if (!config || m_userModel->currentUserName() != userName) {
        return;
    }

    const bool configPointerChanged = config != m_config;
    if (configPointerChanged) {
        if (m_config) {
            disconnect(m_config,
                       &TreelandUserConfig::cursorThemeNameChanged,
                       m_sessionManager,
                       &SessionManager::syncActiveSessionCursorSettings);
            disconnect(m_config,
                       &TreelandUserConfig::cursorSizeChanged,
                       m_sessionManager,
                       &SessionManager::syncActiveSessionCursorSettings);
        }
        m_config = config;
        Q_EMIT configChanged();
    }

    connect(m_config,
            &TreelandUserConfig::cursorThemeNameChanged,
            m_sessionManager,
            &SessionManager::syncActiveSessionCursorSettings,
            Qt::UniqueConnection);
    connect(m_config,
            &TreelandUserConfig::cursorSizeChanged,
            m_sessionManager,
            &SessionManager::syncActiveSessionCursorSettings,
            Qt::UniqueConnection);

    auto user = m_userModel->currentUser();
    m_surfaceProtocols->setUserId(user ? user->UID() : getuid());
    if (m_wallpaperInitialized) {
        m_wallpaperManager->loadWallpaperConfig();
    }
    if (userName == "dde") {
        return;
    }

    if (seatConfig) {
        m_inputManager->setupSeatUserConfig(userName);
    }
    m_sessionManager->syncActiveSessionCursorSettings();
    syncPaletteTypeWithWindowColorScheme(m_config->windowColorScheme());
    tryInitRemoteSource();
    // TODO(YaoBing Xiao): Isolate workspaces for different users to prevent them from sharing the same one.
    m_shellHandler->workspace()->reloadFromConfig();
}

void WaylandServer::createBackend()
{
    m_backend = m_server->attach<WBackend>();
}

void WaylandServer::startDisplay()
{
    m_server->start();
}

void WaylandServer::initializeRendering()
{
    m_renderer.reset(WRenderHelper::createRenderer(m_backend->handle()));
    if (!m_renderer) {
        qCFatal(lcTlCore) << "Failed to create renderer";
    }

    m_allocator.reset(wlr_allocator_autocreate(m_backend->handle(), m_renderer.get()));
    if (!m_allocator) {
        qCFatal(lcTlCore) << "Failed to create allocator";
    }

    if (!wlr_renderer_init_wl_display(m_renderer.get(), m_server->handle())) {
        qCFatal(lcTlCore) << "Failed to initialize renderer wl_display";
    }

    if (!wlr_drm_create(m_server->handle(), m_renderer.get())) {
        qCCritical(lcTlCore) << "Failed to create DRM lease manager";
    }

    m_compositor = wlr_compositor_create(m_server->handle(), 6, m_renderer.get());
    if (!m_compositor) {
        qCFatal(lcTlCore) << "Failed to create server";
    }

    if (!wlr_subcompositor_create(m_server->handle())) {
        qCCritical(lcTlCore) << "Failed to create subcompositor";
    }
}

void WaylandServer::initializeRenderWindow()
{
    m_renderWindow->init(m_renderer.get(), m_allocator.get());
}

void WaylandServer::installPointerProtocols()
{
    m_server->attach<WRemoteSubsurfaceManagerV1>();
    m_server->attach<WCursorShapeManagerV1>();
}

void WaylandServer::installCommonProtocols()
{
    if (!wlr_viewporter_create(m_server->handle())) {
        qCCritical(lcTlCore) << "Failed to create viewporter";
    }

    if (!wlr_single_pixel_buffer_manager_v1_create(m_server->handle())) {
        qCCritical(lcTlCore) << "Failed to create single pixel buffer manager";
    }

    wlr_fractional_scale_manager_v1_create(m_server->handle(), WLR_FRACTIONAL_SCALE_V1_VERSION);
    if (!wlr_ext_background_effect_manager_v1_create(m_server->handle(),
                                                     EXT_BACKGROUND_EFFECT_MANAGER_V1_VERSION,
                                                     EXT_BACKGROUND_EFFECT_MANAGER_V1_CAPABILITY_BLUR)) {
        qCWarning(lcTlCore) << "Failed to create ext-background-effect-manager-v1, blur will be unavailable";
    }
    wlr_data_control_manager_v1_create(m_server->handle());
    wlr_ext_data_control_manager_v1_create(m_server->handle(), EXT_DATA_CONTROL_MANAGER_V1_VERSION);
    wlr_alpha_modifier_v1_create(m_server->handle());
    auto *foreignRegistry = wlr_xdg_foreign_registry_create(m_server->handle());
    wlr_xdg_foreign_v2_create(m_server->handle(), foreignRegistry);
}

void WaylandServer::startBackend()
{
    wlr_backend_start(m_backend->handle());
}

void WaylandServer::init(Treeland::Treeland *treeland)
{
    m_treeland = treeland;
    connect(m_sessionManager, &SessionManager::sessionChanged, treeland, &Treeland::Treeland::SessionChanged);
    connect(m_sessionManager,
            &SessionManager::xwaylandAuthChanged,
            treeland,
            &Treeland::Treeland::XWaylandAuthChanged);

    auto engine = qmlEngine();
#ifndef DISABLE_DDM
    m_greeterProxy = engine->singletonInstance<GreeterProxy *>("Treeland", "GreeterProxy");
    connect(m_greeterProxy, &GreeterProxy::initialUserSelected,
            this, &WaylandServer::initializeWallpaper);
    m_lockScreenManager->setGreeterProxy(m_greeterProxy);
    m_userModel = engine->singletonInstance<UserModel *>("Treeland", "UserModel");
    m_sessionModel = engine->singletonInstance<SessionModel *>("Treeland", "SessionModel");
#endif

    engine->setContextForObject(m_renderWindow, engine->rootContext());
    engine->setContextForObject(m_renderWindow->contentItem(), engine->rootContext());
    m_rootSurfaceContainer->setQmlEngine(engine);
    m_rootSurfaceContainer->init(server());

    createBackend();
    m_seatManager = new SeatManager(server(), this);
    m_dragController = new DragController(m_seatManager, this);

    auto *xdgOutputManager = installDesktopProtocols(engine);

    startDisplay();

    if (!initializeSeatsAndShell())
        return;

    installRuntimeProtocols(xdgOutputManager);

    // start() synchronously reports the initially available outputs. Their
    // per-output DConfig objects initialize asynchronously, so finish the scan
    // only after every reported output has either loaded its config or failed
    // initialization and fallen back to defaults.
    startBackend();
    m_outputManager->backendStarted();
}

SeatManager *WaylandServer::seatManager() const
{
    return m_seatManager;
}

WSeat *WaylandServer::getSeatForEvent(QInputEvent *event) const
{
    return m_seatManager->getSeatForEvent(event);
}

RootSurfaceContainer *WaylandServer::rootSurfaceContainer() const
{
    return m_rootSurfaceContainer;
}

WServer *WaylandServer::server() const
{
    return m_server;
}

void WaylandServer::fakePressSurfaceBottomRightToReszie(SurfaceWrapper *surface)
{
    auto position = surface->geometry().bottomRight();
    m_fakelastPressedPosition = position;
    m_primarySeat->setCursorPosition(position);
    Q_EMIT surface->resizeRequested(Qt::BottomEdge | Qt::RightEdge);
}

void WaylandServer::setCursorPosition(const QPointF &position)
{
    const auto seats = m_seatManager->seats();
    for (auto *seat : seats) {
        m_rootSurfaceContainer->endMoveResizeForSeat(seat);
    }
    m_primarySeat->setCursorPosition(position);
}

void WaylandServer::handleCompositorAction(uint32_t action)
{
    const auto shellAction = CompositorActionInterfaceV1::mapCompositorAction(action);
    if (!shellAction) {
        // Mapped at the protocol boundary: unsupported and unknown actions are
        // ignored (never fatal), per protocol.
        qCDebug(lcTlProtocol) << "Ignoring unsupported server action:" << action;
        return;
    }

    ShellActionExecutor::execute(*shellAction);
}

Output *WaylandServer::getOutput(WOutput *output) const
{
    return m_outputManager->outputFor(output);
}

const QList<Output *> &WaylandServer::outputs() const
{
    return m_outputManager->outputs();
}

void WaylandServer::addOutput()
{
    m_outputManager->requestAdditionalOutputs();
}

void WaylandServer::setOutputMode(OutputMode mode)
{
    m_outputManager->applyMode(
        mode == OutputMode::Copy ? OutputManager::Mode::Copy : OutputManager::Mode::Extension);
}

float WaylandServer::animationSpeed() const
{
    return m_animationSpeed;
}

void WaylandServer::setAnimationSpeed(float newAnimationSpeed)
{
    if (qFuzzyCompare(m_animationSpeed, newAnimationSpeed))
        return;
    m_animationSpeed = newAnimationSpeed;
    Q_EMIT animationSpeedChanged();
}

WaylandServer::OutputMode WaylandServer::outputMode() const
{
    return m_outputManager->mode() == OutputManager::Mode::Copy
        ? OutputMode::Copy
        : OutputMode::Extension;
}

/**
 * Add a WSocket to the Wayland server.
 * This function is used by Treeland::ActivateWayland.
 *
 * @param socket WSocket to add
 */
void WaylandServer::addSocket(WSocket *socket)
{
    m_server->addSocket(socket);
}

bool WaylandServer::toggleDebugMenuBar()
{
    bool ok = false;

    const auto outputs = rootSurfaceContainer()->outputs();
    if (outputs.isEmpty())
        return false;

    bool firstOutputDebugMenuBarIsVisible = false;
    if (auto menuBar = outputs.first()->debugMenuBar()) {
        firstOutputDebugMenuBarIsVisible = menuBar->isVisible();
    }

    for (const auto &output : outputs) {
        if (output->debugMenuBar()) {
            output->debugMenuBar()->setVisible(!firstOutputDebugMenuBarIsVisible);
            ok = true;
        }
    }

    return ok;
}

ShowDesktopInterfaceV1::State WaylandServer::showDesktopState() const
{
    return m_showDesktop->state();
}

WXdgOutputManager *WaylandServer::xwaylandOutputManager() const
{
    return m_outputManager->xwaylandOutputManager();
}

void WaylandServer::setLaunchpadMapped(WOutput *output, bool mapped)
{
    Q_EMIT launchpadMappedChanged(output, mapped);
}

void WaylandServer::showDesktop(WOutput *output)
{
    Q_EMIT showDesktopRequested(output);
}

void WaylandServer::startLockscreen(WOutput *output, bool showAnimation)
{
    Q_EMIT startLockscreened(output, showAnimation);
}

QString WaylandServer::currentWorkspaceWallpaper(WOutput *output)
{
    return m_wallpaperManager->currentWorkspaceWallpaper(output);
}

QString WaylandServer::currentLockScreenWallpaper(WOutput *output)
{
    return m_wallpaperManager->currentLockScreenWallpaper(output);
}

void WaylandServer::setMultitaskViewImpl(IMultitaskView *impl)
{
    m_multitaskView = impl;
}

void WaylandServer::setCurrentMode(CurrentMode mode)
{
    if (m_currentMode == mode)
        return;

    setBlockActivateSurface(mode != CurrentMode::Normal && mode != CurrentMode::Multitaskview);

    m_currentMode = mode;

    // Deactivate pointer constraints when leaving Normal mode (modal shell state).
    if (m_currentMode != CurrentMode::Normal && m_pointerConstraintsManager)
        m_pointerConstraintsManager->deactivateAll();

    Q_EMIT currentModeChanged();
}

void WaylandServer::prepareLockScreenTransition(bool isShutdownView)
{
    if (m_multitaskView) {
        m_multitaskView->immediatelyExit();
    }
    deleteTaskSwitch();
    if (isShutdownView) {
        return;
    }
    setCurrentMode(CurrentMode::LockScreen);
    setWorkspaceVisible(false);
}

WSeat *WaylandServer::seat() const
{
    return m_primarySeat;
}

Output *WaylandServer::getOutputAtCursor() const
{
    return m_outputManager->outputAtCursor();
}

DDMInterfaceV1 *WaylandServer::ddmInterfaceV1() const {
    return m_ddmInterfaceV1;
}

bool WaylandServer::activateUserSession(const QString &username, const QString &sessionId)
{
    if (!m_userModel->getUser(username))
        return false;

    const auto update = m_sessionManager->prepareActiveUserSession(username, sessionId);
    if (!update)
        return false;

    m_userModel->setCurrentUserName(username);
    m_sessionManager->commitActiveUserSession(update);
    return true;
}

void WaylandServer::enableRender() {
    m_renderWindow->setRenderEnabled(true);
}

void WaylandServer::disableRender() {
    m_renderWindow->setRenderEnabled(false);
}

void WaylandServer::setBlockActivateSurface(bool block)
{
    if (block == m_blockActivateSurface)
        return;
    m_blockActivateSurface = block;
    Q_EMIT blockActivateSurfaceChanged();
}

bool WaylandServer::blockActivateSurface() const
{
    return m_blockActivateSurface;
}

bool WaylandServer::noAnimation() const {
    return m_noAnimation;
}

void WaylandServer::setNoAnimation(bool noAnimation) {
    if (m_noAnimation == noAnimation)
        return;
    m_noAnimation = noAnimation;
    Q_EMIT noAnimationChanged();
}

void WaylandServer::toggleFpsDisplay()
{
    if (m_fpsDisplay) {
        m_fpsDisplay->deleteLater();
        m_fpsDisplay = nullptr;
        return;
    }

    m_fpsDisplay = qmlEngine()->createFpsDisplay(m_renderWindow->contentItem());
}

/**
 * Move a XWayland window's surface corresponding to wid, to a
 * position relative to a WSurface. Top-left point is always used.
 *
 * @param wid X Window ID for the XWayland surface
 * @param anchor The anchor WSurface to be relative to
 * @param dx Horizontal distance between the top-left point of anchor and the destination
 * @param dy Vertical distance between the top-left point of anchor and the destination
 */
bool WaylandServer::setXWindowPositionRelative(uint wid, WSurface *anchor, wl_fixed_t dx, wl_fixed_t dy) const
{
    SurfaceWrapper *ach = m_rootSurfaceContainer->getSurface(anchor);
    if (!ach) {
        qCWarning(lcTlCore) << "setXWindowPositionRelative: Failed to get SurfaceWrapper from WSurface";
        return false;
    }

    SurfaceWrapper *target = nullptr;
    for (SurfaceWrapper *wrapper : std::as_const(rootSurfaceContainer()->surfaces())) {
        if (wrapper->type() == SurfaceWrapper::Type::XWayland) {
            wlr_xwayland_surface *surface =
                wlr_xwayland_surface_try_from_wlr_surface(wrapper->surface()->handle());
            if (surface && surface->window_id == static_cast<xcb_window_t>(wid)) {
                target = wrapper;
                break;
            }
        }
    }
    if (!target) {
        qCWarning(lcTlCore) << "setXWindowPositionRelative: XWayland surface corresponding to WID" << wid << "not found!";
        return false;
    }

    QRectF rect(ach->position(), target->size());
    rect.translate(wl_fixed_to_double(dx), wl_fixed_to_double(dy));

    // For XWayland surfaces, setting wrapper position while following
    // implicit surface position may get overwritten by feedback updates.
    // Temporarily switch to compositor-driven position so moveTo() sends
    // configure with the new coordinates.
    target->setXwaylandPositionFromSurface(false);
    target->setPosition(rect.topLeft());
    target->setXwaylandPositionFromSurface(true);
    return true;
}

WXWayland *WaylandServer::createXWayland()
{
    return shellHandler()->createXWayland(server(), m_primarySeat, m_compositor, false);
}

WSeat *WaylandServer::findSeatForSurface(SurfaceWrapper *wrapper) const
{
    return getLastInteractingSeat(wrapper);
}

void WaylandServer::switchWorkspaceForSeat(WSeat *seat, int index)
{
    if (!seat) {
        return;
    }

    workspace()->switchTo(index);
}

void WaylandServer::updateIdleInhibitor()
{
    m_idleManager->updateIdleInhibitor();
}

WBackend *WaylandServer::backend() const
{
    return m_backend;
}

void WaylandServer::activateSurface(SurfaceWrapper *wrapper, Qt::FocusReason reason,
                                        WSeat *seat, bool raise)
{
    m_focusController->activateSurface(wrapper, reason, seat, raise);
}

void WaylandServer::forceActivateSurface(SurfaceWrapper *wrapper, Qt::FocusReason reason,
                                             WSeat *seat)
{
    m_focusController->forceActivateSurface(wrapper, reason, seat);
}

void WaylandServer::requestKeyboardFocus(SurfaceWrapper *wrapper, Qt::FocusReason reason,
                                             WSeat *seat)
{
    m_focusController->requestKeyboardFocus(wrapper, reason, seat);
}

SurfaceWrapper *WaylandServer::keyboardFocusSurface() const
{
    return m_focusController->keyboardFocusSurface();
}

SurfaceWrapper *WaylandServer::activatedSurface() const
{
    return m_focusController->activatedSurface();
}

void WaylandServer::setActivatedSurface(SurfaceWrapper *surface, WSeat *seat, bool raise)
{
    m_focusController->setActivatedSurface(surface, seat, raise);
}

void WaylandServer::onRenderWindowActiveFocusItemChanged()
{
    m_focusController->onRenderWindowActiveFocusItemChanged();
}

WSeat *WaylandServer::getLastInteractingSeat(SurfaceWrapper *surface) const
{
    return m_focusController->getLastInteractingSeat(surface);
}

void WaylandServer::updateSurfaceSeatInteraction(SurfaceWrapper *surface, WSeat *seat)
{
    m_focusController->updateSurfaceSeatInteraction(surface, seat);
}

WSeat *WaylandServer::currentEventSeat() const
{
    return m_inputFilter->currentEventSeat();
}

void WaylandServer::cancelShowDesktop(SurfaceWrapper *surface)
{
    m_showDesktop->cancelShowDesktop(surface);
}

void WaylandServer::restoreFromShowDesktop(SurfaceWrapper *surface)
{
    m_showDesktop->restoreFromShowDesktop(surface);
}

void WaylandServer::restoreShowDesktopFocus()
{
    m_showDesktop->restoreShowDesktopFocus();
}

void WaylandServer::setShowDesktopState(ShowDesktopInterfaceV1::State state)
{
    m_showDesktop->setState(state);
}

void WaylandServer::setLockScreenImpl(ILockScreen *impl)
{
    m_lockScreenManager->setLockScreenImpl(impl);
}

void WaylandServer::showLockScreen(bool switchToGreeter)
{
    m_lockScreenManager->showLockScreen(switchToGreeter);
}

bool WaylandServer::isLockScreenAvailable() const
{
    return m_lockScreenManager->isLockScreenAvailable();
}

void WaylandServer::showShutdownMenu()
{
    m_lockScreenManager->showShutdownMenu();
}

void WaylandServer::showSwitchUser()
{
    m_lockScreenManager->showSwitchUser();
}

void WaylandServer::finishLockScreenTransition()
{
    setCurrentMode(CurrentMode::Normal);
    setWorkspaceVisible(true);
#ifdef EXT_SESSION_LOCK_V1
    setNoAnimation(false);
#endif
    if (auto *surface = activatedSurface()) {
        if (surface->hasFocusCapability())
            requestKeyboardFocus(surface, Qt::NoFocusReason);
    }
}

bool WaylandServer::isShowDesktopAvailable() const
{
    return m_showDesktop->isAvailable();
}

WXdgOutputManager *WaylandServer::installDesktopProtocols(QmlEngine *engine)
{
    m_ddmInterfaceV1 = server()->attach<DDMInterfaceV1>();

    auto *outputManager = server()->attach<WOutputManagerV1>();
    m_outputManager->setBackend(backend());
    m_outputManager->setOutputManagementProtocol(outputManager);

    m_ddeShellV1 = server()->attach<DDEShellManagerInterfaceV1>();

    m_surfaceProtocols->installRegionWatch(server());

    connect(m_ddeShellV1, &DDEShellManagerInterfaceV1::toggleMultitaskview, this, [this] {
        if (m_multitaskView) {
            m_multitaskView->toggleMultitaskView(IMultitaskView::ActiveReason::ShortcutKey);
        }
    });
    connect(m_ddeShellV1,
            &DDEShellManagerInterfaceV1::lockScreenCreated,
            m_lockScreenManager,
            &LockScreenManager::handleLockScreen);

    m_ddeShellV2 = server()->attach<DDEShellManagerInterfaceV2>();

    m_compositorActionInterfaceV1 = server()->attach<CompositorActionInterfaceV1>();
    connect(m_compositorActionInterfaceV1,
            &CompositorActionInterfaceV1::triggered,
            this,
            &WaylandServer::handleCompositorAction);
    m_shellHandler->createComponent(engine, m_renderWindow->contentItem());

    m_surfaceProtocols->installToplevels(server(), m_shellHandler);
    m_inputFilter->installRelativePointer(server(), m_seatManager);

    auto *xdgOutputManager =
        server()->attach<WXdgOutputManager>(m_rootSurfaceContainer->outputLayout());

    auto *outputManagerV1 = server()->attach<OutputManagerV1>();
    connect(m_rootSurfaceContainer,
            &RootSurfaceContainer::primaryOutputChanged,
            outputManagerV1,
            &OutputManagerV1::onPrimaryOutputChanged);
    server()->attach<OutputUuidManagerV1>();
    server()->attach<OutputUuidHeadManagerV1>();
    connect(m_rootSurfaceContainer,
            &RootSurfaceContainer::primaryOutputChanged,
            m_sessionManager,
            &SessionManager::syncActiveSessionXWaylandPrimaryOutput);
    m_wallpaperColorV1 = server()->attach<WallpaperColorInterfaceV1>();
    m_showDesktop->initialize(server());
    m_xWindowControlInterfaceV1 = server()->attach<XWindowControlInterfaceV1>();
    auto *virtualOutputInterface = server()->attach<VirtualOutputManagerInterfaceV1>();
    m_outputManager->setVirtualOutputInterface(virtualOutputInterface);

    m_captureManager->installProtocol(server(), engine, m_renderWindow);
    m_surfaceProtocols->installPersonalization(server());

    // New protocols (treeland-protocols 0.6.0)
    m_appearanceInterfaceV1 = server()->attach<AppearanceInterfaceV1>();
    m_appearanceManagerInterfaceV1 = server()->attach<AppearanceManagerInterfaceV1>();
    m_surfaceProtocols->installDecorationCustomization(server());

    DConfigManager::instance()->initializeActiveUserConfig(this);
    m_showDesktop->connectProtocol();

    qmlRegisterUncreatableType<Personalization>("Treeland.Protocols",
                                                1,
                                                0,
                                                "Personalization",
                                                "Only for Enum");

    qmlRegisterUncreatableType<DDEShellHelper>("Treeland.Protocols",
                                               1,
                                               0,
                                               "DDEShellHelper",
                                               "Only for attached");
    qmlRegisterUncreatableType<CaptureSource>("Treeland.Protocols",
                                              1,
                                              0,
                                              "CaptureSource",
                                              "An abstract class");
    qmlRegisterType<CaptureContextV1>("Treeland.Protocols", 1, 0, "CaptureContextV1");
    qmlRegisterType<CaptureSourceSelector>("Treeland.Protocols", 1, 0, "CaptureSourceSelector");

    server()->attach<WSecurityContextManager>();

    return xdgOutputManager;
}

bool WaylandServer::initializeSeatsAndShell()
{
    // Initialize seats from configuration
    m_primarySeat =
        m_seatManager->initializeFromConfig(QStringLiteral(TREELAND_SYSCONFDIR "/seats.json"), server());
    if (!m_primarySeat) {
        qCCritical(lcTlCore) << "Failed to initialize seats!";
        return false;
    }

    // Setup all seats (cursor, keyboard focus, event filter)
    m_seatManager->setupAllSeats(m_renderWindow,
                                 m_rootSurfaceContainer->outputLayout(),
                                 m_inputFilter,
                                 m_rootSurfaceContainer->cursor());

    // Connect device signals and handle device lifecycle
    m_seatManager->connectBackendSignals(backend());
    connect(m_seatManager, &SeatManager::deviceAdded, this, [this](WInputDevice *device) {
        m_seatManager->assignDevice(device,
                                    m_renderWindow,
                                    m_rootSurfaceContainer->outputLayout(),
                                    m_primarySeat);
    });

    m_dragController->initialize(m_ddeShellV1 != nullptr);

    // Assign existing devices
    m_seatManager->assignExistingDevices(backend());

    if (m_rootSurfaceContainer) {
        m_rootSurfaceContainer->setupSeatManagement();
    }

    if (!m_primarySeat) {
        qCCritical(lcTlCore) << "No seat available after initialization, cannot continue";
        return false;
    }
    m_shellHandler->init(server(), m_primarySeat);

    // Relay the seat's input method presence to org.deepin.Compositor1:
    // treeland-sd hands a fresh Wayland socket to the input method when the
    // active one is destroyed but its client is still running (replacement
    // race between simultaneously started instances), which otherwise leaves
    // the session with no input method at all.
    connect(m_shellHandler->inputMethodHelper(),
            &WInputMethodHelper::inputMethodChanged,
            m_treeland,
            &Treeland::Treeland::InputMethodChanged);

    connect(m_shellHandler->wallpaperShell(),
            &TreelandWallpaperShellInterfaceV1::wallpaperSurfaceAdded,
            m_wallpaperManager,
            &WallpaperManager::handleWallpaperSurfaceAdded);

    return true;
}

void WaylandServer::installRuntimeProtocols(WXdgOutputManager *xdgOutputManager)
{
    initializeRendering();
    m_captureManager->installImageCapture(server(), m_surfaceProtocols->toplevelList());
    initializeRenderWindow();

    auto *xwaylandOutputManager =
        server()->attach<WXdgOutputManager>(m_rootSurfaceContainer->outputLayout());
    m_outputManager->setXdgOutputManagers(
        xdgOutputManager, xwaylandOutputManager, m_sessionManager);
    // User dde does not has a real Logind session, so just pass "0" as id
    m_sessionManager->updateActiveUserSession(QStringLiteral("dde"), QStringLiteral("0"));
    connect(m_userModel, &UserModel::userLoggedIn, m_sessionManager, &SessionManager::updateActiveUserSession);
    m_surfaceProtocols->installDecoration(server());

    auto gammaControlManager = wlr_gamma_control_manager_v1_create(server()->handle());
    m_outputManager->setGammaControlManager(gammaControlManager);

    installPointerProtocols();
    m_pointerConstraintsV1 = server()->attach<WPointerConstraintsV1>();
    m_pointerConstraintsManager = new PointerConstraintsManager(m_pointerConstraintsV1, this, this);

    installCommonProtocols();
    m_idleManager->initialize(server());
    m_focusController->installActivationProtocol(server());

    m_screensaverInterfaceV2 = server()->attach<ScreensaverInterfaceV2>();
    m_idleManager->setScreensaver(m_screensaverInterfaceV2);

    auto *outputPowerManager = wlr_output_power_manager_v1_create(server()->handle());
    m_outputManager->setOutputPowerManager(outputPowerManager);
    m_lockScreenManager->installSessionLock(server());

    m_wallpaperNotifierInterfaceV1 = server()->attach<TreelandWallpaperNotifierInterfaceV1>();
    if (isDDMDisplay()) {
        m_wallpaperNotifierInterfaceV1->setFilter([this](WClient *client) { return m_sessionManager->isDDEUserClient(client); });
    }
    connect(m_wallpaperNotifierInterfaceV1,
            &TreelandWallpaperNotifierInterfaceV1::bound,
            m_wallpaperManager,
            &WallpaperManager::onWallpaperNotifierBound);

    m_wallpaperManagerInterfaceV1 = server()->attach<TreelandWallpaperManagerInterfaceV1>();
    connect(m_wallpaperManagerInterfaceV1,
            &TreelandWallpaperManagerInterfaceV1::wallpaperCreated,
            m_wallpaperManager,
            &WallpaperManager::onWallpaperAdded);

    m_shortcutManager = server()->attach<ShortcutManagerV3>();
    connect(m_treeland,
            &Treeland::Treeland::SessionChanged,
            m_shortcutManager,
            &ShortcutManagerV3::onSessionChanged);
    m_shortcutManager->onSessionChanged();
    auto shortcutControl = m_shortcutManager->controller();
    auto *shortcutRunner = new ShortcutRunner(shortcutControl);
    connect(shortcutControl,
            &ShortcutController::actionTriggered,
            shortcutRunner,
            &ShortcutRunner::onActionTrigger);
    connect(shortcutControl,
            &ShortcutController::actionProgress,
            shortcutRunner,
            &ShortcutRunner::onActionProgress);
    connect(shortcutControl,
            &ShortcutController::actionFinished,
            shortcutRunner,
            &ShortcutRunner::onActionFinish);

    m_inputManagerInterfaceV1 = server()->attach<TreelandInputManagerInterfaceV1>();
    connect(m_inputManagerInterfaceV1,
            &TreelandInputManagerInterfaceV1::mouseSettingsCreated,
            m_inputManager,
            &InputManager::onMouseSettingsCreated);
    connect(m_inputManagerInterfaceV1,
            &TreelandInputManagerInterfaceV1::touchpadSettingsCreated,
            m_inputManager,
            &InputManager::onTouchpadSettingsCreated);
    connect(m_inputManagerInterfaceV1,
            &TreelandInputManagerInterfaceV1::keyboardSettingsCreated,
            m_inputManager,
            &InputManager::onKeyboardSettingsCreated);

    m_keyboardStateNotifyManagerInterfaceV1 = server()->attach<TreelandKeyboardStateNotifyManagerInterfaceV1>();
    m_activeNotifyManagerInterfaceV1 = server()->attach<TreelandActiveNotifyManagerInterfaceV1>();
    m_keyboardShortcutsInhibitManagerV1 = server()->attach<KeyboardShortcutsInhibitManagerV1>();
    m_inputFilter->setShortcutProtocols(m_shortcutManager, m_keyboardShortcutsInhibitManagerV1);
}
