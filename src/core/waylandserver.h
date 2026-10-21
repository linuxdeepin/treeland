// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wlr_fwd.h>
#include "core/qmlengine.h"
#include "modules/activation/activationmanagerinterfacev1.h"
#include "modules/wallpaper/wallpapermanagerinterfacev1.h"
#include "modules/wallpaper/wallpapernotifierinterfacev1.h"
#include "modules/show-desktop/showdesktopinterfacev1.h"
#include "modules/xwindow-control/xwindowcontrolinterfacev1.h"
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
Q_MOC_INCLUDE("surface/surfacewrapper.h")
Q_MOC_INCLUDE("workspace/workspace.h")
Q_MOC_INCLUDE("treelandconfig.hpp")
Q_MOC_INCLUDE("treelanduserconfig.hpp")

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
class WXWayland;
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
class ForeignToplevelManagerInterfaceV2;
class FpsDisplayManager;
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
class SettingManager;
class SessionModel;
class ShellHandler;
class ShellActionExecutor;
class ShortcutManagerV3;
class ShortcutRunner;
class SurfaceContainer;
class SurfaceWrapper;
class TreelandConfig;
class TreelandUserConfig;
class TreelandRemoteSource;
class UserModel;
class WallpaperColorInterfaceV1;
class ShowDesktopInterfaceV1;
class TreelandKeyboardStateNotifyManagerInterfaceV1;
class TreelandActiveNotifyManagerInterfaceV1;
class TreelandRegionWatchManagerInterfaceV1;
class KeyboardShortcutsInhibitManagerV1;
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
    Q_PROPERTY(Workspace* workspace READ workspace CONSTANT FINAL)
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
    TreelandUserConfig *config();
    TreelandConfig *globalConfig();

    SurfaceWrapper *activatedSurface() const;
    SessionManager *sessionManager() const;
    QmlEngine *qmlEngine() const;
    WOutputRenderWindow *window() const;
    ShellHandler *shellHandler() const;
    Workspace *workspace() const;
    WXdgOutputManager *xwaylandOutputManager() const;

    void init(Treeland::Treeland *treeland);

    RootSurfaceContainer *rootSurfaceContainer() const;
    WServer *server() const;
    Output *getOutput(WOutput *output) const;
    // Reverse lookup for the uuid addressing shared by the treeland-output-uuid
    // based protocols (output-manager-v2, output-group-manager-v1, ...).
    // Returns nullptr for an empty or unknown uuid; state checks (enabled,
    // live) stay with the callers.
    Output *getOutputByUuid(const QString &uuid) const;
    const QList<Output *> &outputs() const;

    float animationSpeed() const;
    void setAnimationSpeed(float newAnimationSpeed);

    OutputMode outputMode() const;
    void setOutputMode(OutputMode mode);
    Q_INVOKABLE void addOutput();

    void addSocket(WSocket *socket);
    [[nodiscard]] WXWayland *createXWayland();

    WSeat *seat() const;

    // Moves the primary seat cursor to @p position, first ending any
    // in-progress interactive move/resize for every seat so the motion is
    // not fed into an active transaction. Exposed for the debug control API.
    void setCursorPosition(const QPointF &position);

    bool toggleDebugMenuBar();

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
    void restoreFromShowDesktop(SurfaceWrapper *activeSurface = nullptr);
    void restoreShowDesktopFocus();

    bool setXWindowPositionRelative(uint wid, WSurface *anchor, wl_fixed_t dx, wl_fixed_t dy) const;
    SeatManager *seatManager() const;

    WSeat *getSeatForEvent(QInputEvent *event) const;
    WSeat *findSeatForSurface(SurfaceWrapper *wrapper) const;
    WSeat *getLastInteractingSeat(SurfaceWrapper *surface) const;
    WSeat *currentEventSeat() const;

    bool isDDMDisplay() const { return m_isDDMDisplay; }

    RootSurfaceContainer *rootContainer() const { return m_rootSurfaceContainer; }
    WBackend *backend() const;
public Q_SLOTS:
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
    void configChanged();
    void primaryOutputChanged();
    void activatedSurfaceChanged();

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
    friend class WallpaperManager;
    friend class WallpaperItem;
    friend class WallpaperSwitcherItem;
    friend class InputManager;
    friend class RootSurfaceContainer;
    friend class ShellHandler;
    friend class ShortcutRunner;
    friend class ShellActionExecutor;

    static WaylandServer *m_instance;
    TreelandUserConfig *m_config = nullptr;
    TreelandConfig *m_globalConfig = nullptr;
    Treeland::Treeland *m_treeland = nullptr;
    FpsDisplayManager *m_fpsManager = nullptr;
    SessionManager *m_sessionManager = nullptr;
    WallpaperManager *m_wallpaperManager = nullptr;

    CurrentMode m_currentMode{ CurrentMode::Normal };

    // QtQuick scene and presentation
    WOutputRenderWindow *m_renderWindow = nullptr;
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
    ShortcutManagerV3 *m_shortcutManager = nullptr;
    AppearanceInterfaceV1 *m_appearanceInterfaceV1 = nullptr;
    AppearanceManagerInterfaceV1 *m_appearanceManagerInterfaceV1 = nullptr;
    CompositorActionInterfaceV1 *m_compositorActionInterfaceV1 = nullptr;
    WallpaperColorInterfaceV1 *m_wallpaperColorV1 = nullptr;
    XWindowControlInterfaceV1 *m_xWindowControlInterfaceV1 = nullptr;
    DDEShellManagerInterfaceV1 *m_ddeShellV1 = nullptr;
    DDEShellManagerInterfaceV2 *m_ddeShellV2 = nullptr;
    DDMInterfaceV1 *m_ddmInterfaceV1 = nullptr;
    ScreensaverInterfaceV2 *m_screensaverInterfaceV2 = nullptr;
    TreelandWallpaperManagerInterfaceV1 *m_wallpaperManagerInterfaceV1 = nullptr;
    TreelandWallpaperNotifierInterfaceV1 *m_wallpaperNotifierInterfaceV1 = nullptr;
    TreelandKeyboardStateNotifyManagerInterfaceV1 *m_keyboardStateNotifyManagerInterfaceV1 = nullptr;
    TreelandActiveNotifyManagerInterfaceV1 *m_activeNotifyManagerInterfaceV1 = nullptr;
    KeyboardShortcutsInhibitManagerV1 *m_keyboardShortcutsInhibitManagerV1 = nullptr;
    // Desktop presentation state
    OutputManager *m_outputManager = nullptr;
    QPointer<QQuickItem> m_taskSwitch;

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
    InputManager *m_inputManager = nullptr;
    SurfaceProtocolController *m_surfaceProtocols = nullptr;
    ShowDesktop *m_showDesktop = nullptr;
    LockScreenManager *m_lockScreenManager = nullptr;
    FocusController *m_focusController = nullptr;
    CompositorInputFilter *m_inputFilter = nullptr;
    IdleManager *m_idleManager = nullptr;
    CaptureManager *m_captureManager = nullptr;
    DragController *m_dragController = nullptr;
    TreelandInputManagerInterfaceV1 *m_inputManagerInterfaceV1 = nullptr;
};
