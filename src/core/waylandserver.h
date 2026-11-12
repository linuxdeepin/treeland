// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wlr_fwd.h>
#include "core/qmlengine.h"
#include "modules/activation/activationmanagerinterfacev1.h"
#include "modules/wallpaper/wallpapermanagerinterfacev1.h"
#include "modules/wallpaper/wallpapernotifierinterfacev1.h"
#include "modules/show-desktop/showdesktopinterfacev1.h"
#ifndef DISABLE_XWAYLAND
#include "modules/xwindow-control/xwindowcontrolinterfacev1.h"
#endif
#include "utils/fpsdisplaymanager.h"

#include <xcb/xproto.h>

#include <wextforeigntoplevellistv1.h>
#include <wglobal.h>
#include <wscoplistener.h>
#include <wpointer.h>
#include <woutputmanagerv1.h>
#include <wqmlcreator.h>
#include <wseat.h>
#include <wxdgdecorationmanager.h>
#include <wxdgdialogmanagerv1.h>
#include <wxdgtopleveltagmanager.h>

#include <QList>
#include <QMap>
#include <QEvent>

#include <memory>
#include <optional>

class QJsonObject;
class SeatUserDConfig;

Q_MOC_INCLUDE(<QDBusObjectPath>)
Q_MOC_INCLUDE(<woutput.h>)
Q_MOC_INCLUDE(<wlayersurface.h>)
Q_MOC_INCLUDE(<wtoplevelsurface.h>)
Q_MOC_INCLUDE(<wxdgsurface.h>)
Q_MOC_INCLUDE("core/rootsurfacecontainer.h")
Q_MOC_INCLUDE("modules/capture/capture.h")
Q_MOC_INCLUDE("modules/wallpaper/wallpapershellinterfacev1.h")
Q_MOC_INCLUDE("surface/surfacewrapper.h")
Q_MOC_INCLUDE("workspace/workspace.h")
Q_MOC_INCLUDE("treelandconfig.hpp")
Q_MOC_INCLUDE("treelanduserconfig.hpp")
Q_MOC_INCLUDE("animationconfig.hpp")

QT_BEGIN_NAMESPACE
class QDBusObjectPath;
class QQuickItem;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE
class WBackend;
class WClientPrivate;
class WCursor;
class WExtForeignToplevelListV1;
class WForeignToplevel;
class WOutput;
class WOutputRenderWindow;
class WServer;
class WSessionLock;
class WSessionLockManager;
class WSocket;
class WSurface;
class WSurfaceItem;
class WToplevelSurface;
class WXdgDecorationManager;
class WXdgOutputManager;
#ifndef DISABLE_XWAYLAND
class WXWayland;
#endif
class WPointerConstraintsV1;

class WForeignToplevel;
class WExtForeignToplevelListV1;
class WRelativePointerManagerV1;
class WSessionLockManager;
class WSessionLock;
WAYLIB_SERVER_END_NAMESPACE

class ShowDesktop;
class LockScreenManager;
class IdleManager;
class CaptureManager;
class DragController;
class GestureManager;
class InputContext;
class CompositorInputFilter;
class FocusController;
class SurfaceProtocolController;
class SeatManager;
class PointerConstraintsManager;

WAYLIB_SERVER_USE_NAMESPACE

class CaptureSourceSelector;
class DDEShellManagerInterfaceV1;
class DDEShellManagerInterfaceV2;
class DDMInterfaceV1;
class ForeignToplevelManagerInterfaceV3;
class FpsDisplayManager;
class GraphicsPlatform;
class GreeterProxy;
class ILockScreen;
class IMultitaskView;
class LockScreen;
class LockScreenInterface;
class Multitaskview;
class Output;
class OutputManager;
class PersonalizationManagerInterfaceV1;
class AppearanceInterfaceV1;
class AppearanceManagerInterfaceV1;
class CompositorActionInterfaceV1;
class DecorationManagerInterfaceV1;
class RootSurfaceContainer;
class ScreensaverInterfaceV2;
class SessionManager;
class SessionModel;
class ShellHandler;
class ShellActionExecutor;
class ShortcutController;
class ShortcutManagerV3;
class ShortcutRunner;
class SurfaceContainer;
class SurfaceWrapper;
class TreelandConfig;
class AnimationConfig;
class TreelandUserConfig;
class TreelandRemoteSource;
class Session;
class DesktopEnvironment;
Q_MOC_INCLUDE("session/session.h")
Q_MOC_INCLUDE("session/sessionmanager.h")
class UserModel;
class WallpaperColorInterfaceV1;
class ShowDesktopInterfaceV1;
class TreelandKeyboardStateNotifyManagerInterfaceV1;
class TreelandActiveNotifyManagerInterfaceV1;
class TreelandRegionWatchManagerInterfaceV1;
class KeyboardShortcutsInhibitManagerV1;
class TreelandWallpaperSurfaceInterfaceV1;
class WallpaperManager;
class WallpaperItem;
class TreelandInputManagerInterfaceV1;
class InputManager;

namespace Treeland {
class Treeland;
}

class WaylandServer : public QObject, public WAYLIB_SERVER_NAMESPACE::WObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(RootSurfaceContainer* rootSurfaceContainer READ rootSurfaceContainer CONSTANT FINAL)
    Q_PROPERTY(float animationSpeed READ animationSpeed WRITE setAnimationSpeed NOTIFY animationSpeedChanged FINAL)
    Q_PROPERTY(OutputMode outputMode READ outputMode WRITE setOutputMode NOTIFY outputModeChanged FINAL)
    Q_PROPERTY(SurfaceWrapper* activatedSurface READ activatedSurface NOTIFY activatedSurfaceChanged FINAL)
    Q_PROPERTY(Workspace* workspace READ workspace NOTIFY workspaceChanged FINAL)
    Q_PROPERTY(AnimationConfig* animationConfig READ animationConfig NOTIFY animationConfigChanged FINAL)
    Q_PROPERTY(TreelandUserConfig* config READ config NOTIFY configChanged FINAL)
    Q_PROPERTY(TreelandConfig* globalConfig READ globalConfig CONSTANT FINAL)
    Q_PROPERTY(bool blockActivateSurface READ blockActivateSurface WRITE setBlockActivateSurface NOTIFY blockActivateSurfaceChanged FINAL)
    Q_PROPERTY(bool noAnimation READ noAnimation WRITE setNoAnimation NOTIFY noAnimationChanged FINAL)
    Q_PROPERTY(RootSurfaceContainer* rootContainer READ rootContainer CONSTANT FINAL)

public:
    explicit WaylandServer(QObject *parent = nullptr);
    ~WaylandServer() override;

    static void syncPaletteTypeWithWindowColorScheme(int32_t colorScheme);

    enum class OutputMode
    {
        Copy,
        Extension
    };
    Q_ENUM(OutputMode)

    enum class CurrentMode
    {
        Normal,
        LockScreen,
        WindowSwitch,
        Multitaskview
    };
    Q_ENUM(CurrentMode)

    static WaylandServer *instance();
    bool isAvailable() const { return m_instance != nullptr; }
    AnimationConfig *animationConfig() const;
    TreelandUserConfig *config();
    TreelandConfig *globalConfig();

    SurfaceWrapper *activatedSurface() const;
    SessionManager *sessionManager() const;
    QmlEngine *qmlEngine() const;
    WOutputRenderWindow *window() const;
    GraphicsPlatform *graphicsPlatform() const;
    ShellHandler *shellHandler() const;
    Session *activeSession() const;
    DesktopEnvironment *desktop() const;
    QList<DesktopEnvironment *> desktopEnvironments() const;
    WallpaperManager *wallpaperManager() const;
    TreelandWallpaperNotifierInterfaceV1 *wallpaperNotifier() const;
    OutputManager *outputHardware() const;
    CompositorInputFilter *createInputFilter(InputContext *context, GestureManager *gestures,
                                             ShortcutController *controller, QObject *parent);
    void configureInputFilter(CompositorInputFilter *filter, ShortcutManagerV3 *shortcuts = nullptr) const;
    QQuickItem *taskSwitcher() const;
    void ensureTaskSwitcher();
    Q_INVOKABLE void setOutputScale(WOutput *output, qreal scale);
    void initializeSessionProtocols(Session *session);
    Workspace *workspace() const;
    WXdgOutputManager *xwaylandOutputManager() const;

    void init(Treeland::Treeland *treeland);

    RootSurfaceContainer *rootSurfaceContainer() const;
    WServer *server() const;
    Output *getOutput(WOutput *output) const;
    Output *getOutputByUuid(const QString &uuid) const;
    const QList<Output *> &outputs() const;

    float animationSpeed() const;
    void setAnimationSpeed(float newAnimationSpeed);

    OutputMode outputMode() const;
    void setOutputMode(OutputMode mode);
    Q_INVOKABLE void addOutput();

    void addSocket(WSocket *socket);
#ifndef DISABLE_XWAYLAND
    [[nodiscard]] WXWayland *createXWayland(WSeat *seat);
#endif

    WSeat *seat() const;

    // The shortcut manager's controller, i.e. the live registry of bound
    // shortcuts. Exposed for the debug remote source; null before init.
    ShortcutController *shortcutController() const;

    // Moves the primary seat cursor to @p position, first ending any
    // in-progress interactive move/resize for every seat so the motion is
    // not fed into an active transaction. Exposed for the debug control API.
    void setCursorPosition(const QPointF &position);

    bool toggleDebugMenuBar();
    Q_INVOKABLE QString damageVisual() const;
    Q_INVOKABLE bool setDamageVisual(const QString &mode);

    ShowDesktopInterfaceV1::State showDesktopState() const;
    bool isShowDesktopAvailable() const;
    void setShowDesktopState(ShowDesktopInterfaceV1::State state);

    Q_INVOKABLE void setLaunchpadMapped(WOutput *output, bool mapped);
    Q_INVOKABLE void showDesktop(WOutput *output);
    Q_INVOKABLE void cancelShowDesktop(SurfaceWrapper *excludeSurface = nullptr);
    Q_INVOKABLE void startLockscreen(WOutput *output, bool showAnimation);
    Q_INVOKABLE QString currentWorkspaceWallpaper(WOutput *output);
    Q_INVOKABLE QString currentLockScreenWallpaper(WOutput *output);

    void setMultitaskViewImpl(IMultitaskView *impl);
    void setLockScreenImpl(ILockScreen *impl);

    CurrentMode currentMode() const
    {
        return m_currentMode;
    }

    void setCurrentMode(CurrentMode mode);

    bool isNormalOrMultitaskview() const
    {
        return m_currentMode == CurrentMode::Normal || m_currentMode == CurrentMode::Multitaskview;
    }

    bool isLockScreenAvailable() const;

    // When @p isShutdownView is true the current mode is left untouched, so the
    // standalone shutdown menu does not hide layer surfaces (desktop icons,
    // dock, panels) via setHideByLockScreen().
    void prepareLockScreenTransition(bool isShutdownView = false);
    void finishLockScreenTransition();
    void setNoAnimation(bool noAnimation);

    void showLockScreen(bool switchToGreeter = true);
    void showShutdownMenu();
    void showSwitchUser();

    Output* getOutputAtCursor() const;

    inline UserModel *userModel() const { return m_userModel; };
    inline SessionModel *sessionModel() const { return m_sessionModel; };
    DDMInterfaceV1 *ddmInterfaceV1() const;

    bool activateUserSession(const QString &username, const QString &sessionId);
    void enableRender();
    void disableRender();

    void setBlockActivateSurface(bool block);
    bool blockActivateSurface() const;
    bool noAnimation() const;
    void toggleFpsDisplay();

    void updateIdleInhibitor();
    // Idle state and inhibit sources, as reported by the treeland-debug
    // ``idle`` command. Never null after initialization.
    IdleManager *idleManager() const { return m_idleManager; }
    void restoreFromShowDesktop(SurfaceWrapper *activeSurface = nullptr);
    void restoreShowDesktopFocus();

#ifndef DISABLE_XWAYLAND
    bool setXWindowPositionRelative(uint wid, WSurface *anchor, wl_fixed_t dx, wl_fixed_t dy) const;
#endif
    SeatManager *seatManager() const;

    WSeat *getSeatForEvent(QInputEvent *event) const;
    WSeat *findSeatForSurface(SurfaceWrapper *wrapper) const;
    WSeat *getLastInteractingSeat(SurfaceWrapper *surface) const;
    WSeat *currentEventSeat() const;

    bool isDDMDisplay() const { return m_isDDMDisplay; }

    RootSurfaceContainer *rootContainer() const { return m_rootSurfaceContainer; }
    WBackend *backend() const;
public Q_SLOTS:
    void initializeWallpaper();
    void activateSurface(SurfaceWrapper *wrapper,
                         Qt::FocusReason reason = Qt::OtherFocusReason,
                         WSeat *seat = nullptr,
                         bool raise = true);
    void forceActivateSurface(SurfaceWrapper *wrapper,
                              Qt::FocusReason reason = Qt::OtherFocusReason,
                              WSeat *seat = nullptr);
    void requestKeyboardFocus(SurfaceWrapper *wrapper,
                              Qt::FocusReason reason = Qt::OtherFocusReason,
                              WSeat *seat = nullptr);
    void fakePressSurfaceBottomRightToReszie(SurfaceWrapper *surface);
    bool surfaceBelongsToCurrentSession(SurfaceWrapper *wrapper);

Q_SIGNALS:
    void wallpaperChanged();
    void activeSessionChanged();
    void workspaceChanged();
    void workspaceAdded();
    void animationConfigChanged();
    void configChanged();
    void primaryOutputChanged();
    void activatedSurfaceChanged();
    void desktopChanged();

    void animationSpeedChanged();
    void outputModeChanged();

    void currentModeChanged();
    void noAnimationChanged();

    void blockActivateSurfaceChanged();
    void requestQuit();

    void launchpadMappedChanged(WOutput *output, bool mapped);
    void showDesktopRequested(WOutput *output);
    void showDesktopStateChanged();
    void startLockscreened(WOutput *output, bool showAnimation);
    void modifierKeyReleased(QKeyEvent *event);

private Q_SLOTS:
    void deleteTaskSwitch();
    void onWorkspaceChanged();
    void applyActiveSessionConfig();
    void onWallpaperNotifierBound(wl_resource *resource);
    void onWallpaperSurfaceAdded(TreelandWallpaperSurfaceInterfaceV1 *surface);

private:
    WXdgOutputManager *installDesktopProtocols(QmlEngine *engine);
    bool initializeSeatsAndShell();
    void installRuntimeProtocols(WXdgOutputManager *xdgOutputManager);
    void createBackend();
    void startDisplay();
    void initializeRendering();
    void initializeRenderWindow();
    void installPointerProtocols();
    void installCommonProtocols();
    void startBackend();
    void handleCompositorAction(uint32_t action);
    void applyCurrentUserConfig(const QString &userName,
                                TreelandUserConfig *config,
                                SeatUserDConfig *seatConfig);
    SurfaceWrapper *keyboardFocusSurface() const;
    void setActivatedSurface(SurfaceWrapper *newActivateSurface,
                             WSeat *seat = nullptr,
                             bool raise = true);
    void setWorkspaceVisible(bool visible);
    void updateSurfaceSeatInteraction(SurfaceWrapper *surface, WSeat *seat);
    void switchWorkspaceForSeat(WSeat *seat, int index);

private Q_SLOTS:
    void onRenderWindowActiveFocusItemChanged();

private:
    friend class PointerConstraintsManager;
    friend class SessionManager;
    friend class SessionInputContext;
    friend class TreelandWallpaperSurfaceInterfaceV1;
    friend class WallpaperManager;
    friend class WallpaperItem;
    friend class WallpaperSwitcherItem;
    friend class InputManager;
    friend class RootSurfaceContainer;
    friend class ShellHandler;
    friend class ShortcutRunner;
    friend class ShellActionExecutor;

    static WaylandServer *m_instance;
    void setAnimationConfig(AnimationConfig *config);
    AnimationConfig *m_animationConfig = nullptr;
    TreelandUserConfig *m_config = nullptr;
    TreelandConfig *m_globalConfig = nullptr;
    Treeland::Treeland *m_treeland = nullptr;
    FpsDisplayManager *m_fpsManager = nullptr;
    SessionManager *m_sessionManager = nullptr;
    bool m_wallpaperInitialized = false;

    CurrentMode m_currentMode{ CurrentMode::Normal };

    // QtQuick scene and presentation
    WOutputRenderWindow *m_renderWindow = nullptr;
    std::unique_ptr<GraphicsPlatform> m_graphicsPlatform;
    QQuickItem *m_dockPreview = nullptr;
    QQuickItem *m_fpsDisplay = nullptr;

    // Wayland runtime and scene root
    WServer *m_server = nullptr;
    WBackend *m_backend = nullptr;
    WUniquePointer<wlr_renderer> m_renderer;
    WUniquePointer<wlr_allocator> m_allocator;
    WPointer<wlr_compositor> m_compositor;
    RootSurfaceContainer *m_rootSurfaceContainer = nullptr;

    // Seat and desktop protocol adapters
    WSeat *m_primarySeat = nullptr;
    ShellHandler *m_shellHandler = nullptr;
    WPointerConstraintsV1 *m_pointerConstraintsV1 = nullptr;
    PointerConstraintsManager *m_pointerConstraintsManager = nullptr;
    CompositorActionInterfaceV1 *m_compositorActionInterfaceV1 = nullptr;
    WallpaperColorInterfaceV1 *m_wallpaperColorV1 = nullptr;
#ifndef DISABLE_XWAYLAND
    XWindowControlInterfaceV1 *m_xWindowControlInterfaceV1 = nullptr;
#endif
    DDEShellManagerInterfaceV1 *m_ddeShellV1 = nullptr;
    DDEShellManagerInterfaceV2 *m_ddeShellV2 = nullptr;
    DDMInterfaceV1 *m_ddmInterfaceV1 = nullptr;
    ScreensaverInterfaceV2 *m_screensaverInterfaceV2 = nullptr;
    TreelandWallpaperNotifierInterfaceV1 *m_wallpaperNotifierInterfaceV1 = nullptr;
    TreelandKeyboardStateNotifyManagerInterfaceV1 *m_keyboardStateNotifyManagerInterfaceV1 = nullptr;
    TreelandActiveNotifyManagerInterfaceV1 *m_activeNotifyManagerInterfaceV1 = nullptr;
    KeyboardShortcutsInhibitManagerV1 *m_keyboardShortcutsInhibitManagerV1 = nullptr;
    // Desktop presentation state
    OutputManager *m_outputManager = nullptr;

    float m_animationSpeed = 1.0;
    std::optional<QPointF> m_fakelastPressedPosition;

    QPropertyAnimation *m_workspaceScaleAnimation{ nullptr };
    QPropertyAnimation *m_workspaceOpacityAnimation{ nullptr };

    IMultitaskView *m_multitaskView{ nullptr };
    UserModel *m_userModel{ nullptr };
    SessionModel *m_sessionModel{ nullptr };
    GreeterProxy *m_greeterProxy{ nullptr };

    bool m_blockActivateSurface{ false };

    bool m_noAnimation{ false };
    bool m_isDDMDisplay{ false };
    void tryInitRemoteSource();

    TreelandRemoteSource *m_treelandRemoteSource = nullptr;

    SeatManager *m_seatManager = nullptr;
    SurfaceProtocolController *m_surfaceProtocols = nullptr;
    ShowDesktop *m_showDesktop = nullptr;
    LockScreenManager *m_lockScreenManager = nullptr;
    FocusController *m_focusController = nullptr;
    // Startup filter and relative-pointer transport; user seats use their input channel.
    CompositorInputFilter *m_bootstrapInputFilter = nullptr;
    IdleManager *m_idleManager = nullptr;
    CaptureManager *m_captureManager = nullptr;
    DragController *m_dragController = nullptr;
};
