// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "modules/foreign-toplevel/foreigntoplevelmanagerv1.h"
#include "core/qmlengine.h"
#include "modules/shortcut/shortcutmanager.h"
#include "modules/virtual-output/virtualoutputmanager.h"
#include "modules/window-management/windowmanagement.h"
#include "utils/fpsdisplaymanager.h"

#include <wglobal.h>
#include <wqmlcreator.h>
#include <wseat.h>
#include <wxdgdecorationmanager.h>
#include <wextforeigntoplevellistv1.h>
#include <woutputmanagerv1.h>

#include <xcb/xproto.h>

#include <QList>
#include <optional>

Q_MOC_INCLUDE(<wtoplevelsurface.h>)
Q_MOC_INCLUDE(<wxdgsurface.h>)
Q_MOC_INCLUDE(<qwgammacontorlv1.h>)
Q_MOC_INCLUDE(<qwoutputmanagementv1.h>)
Q_MOC_INCLUDE("surface/surfacewrapper.h")
Q_MOC_INCLUDE("workspace/workspace.h")
Q_MOC_INCLUDE("core/rootsurfacecontainer.h")
Q_MOC_INCLUDE("modules/capture/capture.h")
Q_MOC_INCLUDE(<wlayersurface.h>)
Q_MOC_INCLUDE(<QDBusObjectPath>)
Q_MOC_INCLUDE("treelandconfig.hpp")
Q_MOC_INCLUDE("treelanduserconfig.hpp")

QT_BEGIN_NAMESPACE
class QQuickItem;
class QDBusObjectPath;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE
class WServer;
class WOutputRenderWindow;
class WOutputLayout;
class WClientPrivate;
class WCursor;
class WBackend;
class WOutputItem;
class WOutputViewport;
class WOutputLayer;
class WOutput;
class WXWayland;
class WXdgDecorationManager;
class WSocket;
class WSurface;
class WToplevelSurface;
class WSurfaceItem;
class WForeignToplevel;
class WExtForeignToplevelListV1;
class WOutputManagerV1;
class WLayerSurface;
class WSessionLockManager;
class WSessionLock;
WAYLIB_SERVER_END_NAMESPACE

QW_BEGIN_NAMESPACE
class qw_renderer;
class qw_allocator;
class qw_compositor;
class qw_idle_notifier_v1;
class qw_idle_inhibit_manager_v1;
class qw_output_configuration_v1;
class qw_output_power_manager_v1;
class qw_idle_inhibitor_v1;
QW_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE
QW_USE_NAMESPACE

class Output;
class SurfaceWrapper;
class SurfaceContainer;
class RootSurfaceContainer;
class ForeignToplevelV1;
class LockScreen;
class ShortcutManagerV2;
class ShortcutRunner;
class PersonalizationV1;
class WallpaperColorV1;
class WindowManagementV1;
class Multitaskview;
class DDEShellManagerInterfaceV1;
class PrelaunchSplash;
class WindowPickerInterface;
class VirtualOutputV1;
class ShellHandler;
class OutputManagerV1;
class CaptureSourceSelector;
class treeland_window_picker_v1;
class IMultitaskView;
class LockScreenInterface;
class ILockScreen;
class UserModel;
class OutputConfigState;
class OutputLifecycleManager;
class DDMInterfaceV1;
class TreelandConfig;
class TreelandUserConfig;
class FpsDisplayManager;
class ScreensaverInterfaceV1;
class SettingManager;

struct wlr_idle_inhibitor_v1;
struct wlr_output_power_v1_set_mode_event;
struct wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request;

namespace Treeland {
class Treeland;
}

QW_BEGIN_NAMESPACE
class qw_ext_foreign_toplevel_image_capture_source_manager_v1;
QW_END_NAMESPACE

struct Session : QObject {
    Q_OBJECT
public:
    int id = 0;
    uid_t uid = 0;
    QString username = {};
    WSocket *socket = nullptr;
    WXWayland *xwayland = nullptr;
    quint32 noTitlebarAtom = XCB_ATOM_NONE;
    SettingManager *settingManager = nullptr;

    ~Session();

Q_SIGNALS:
    void aboutToBeDestroyed();
};

class Helper : public WSeatEventFilter
{
    friend class RootSurfaceContainer;
    friend class ShortcutRunner;
    Q_OBJECT
    Q_PROPERTY(bool socketEnabled READ socketEnabled WRITE setSocketEnabled NOTIFY socketEnabledChanged FINAL)
    Q_PROPERTY(RootSurfaceContainer* rootContainer READ rootContainer CONSTANT FINAL)
    Q_PROPERTY(float animationSpeed READ animationSpeed WRITE setAnimationSpeed NOTIFY animationSpeedChanged FINAL)
    Q_PROPERTY(OutputMode outputMode READ outputMode WRITE setOutputMode NOTIFY outputModeChanged FINAL)
    Q_PROPERTY(SurfaceWrapper* activatedSurface READ activatedSurface NOTIFY activatedSurfaceChanged FINAL)
    Q_PROPERTY(Workspace* workspace READ workspace CONSTANT FINAL)
    Q_PROPERTY(TreelandUserConfig* config READ config CONSTANT FINAL)
    Q_PROPERTY(TreelandConfig* globalConfig READ globalConfig CONSTANT FINAL)
    Q_PROPERTY(bool blockActivateSurface READ blockActivateSurface WRITE setBlockActivateSurface NOTIFY blockActivateSurfaceChanged FINAL)
    Q_PROPERTY(bool noAnimation READ noAnimation WRITE setNoAnimation NOTIFY noAnimationChanged FINAL)
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit Helper(QObject *parent = nullptr);
    ~Helper() override;

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

    static Helper *instance();
    TreelandUserConfig *config();
    TreelandConfig *globalConfig();

    QmlEngine *qmlEngine() const;
    WOutputRenderWindow *window() const;
    ShellHandler *shellHandler() const;
    Workspace *workspace() const;

    void init(Treeland::Treeland *treeland);

    bool socketEnabled() const;
    void setSocketEnabled(bool newSocketEnabled);

    RootSurfaceContainer *rootContainer() const;
    Output *getOutput(WOutput *output) const;

    float animationSpeed() const;
    void setAnimationSpeed(float newAnimationSpeed);

    OutputMode outputMode() const;
    void setOutputMode(OutputMode mode);
    Q_INVOKABLE void addOutput();

    void addSocket(WSocket *socket);
    void removeXWayland(WXWayland *xwayland);
    void removeSession(std::shared_ptr<Session> session);
    WXWayland *xwaylandForUid(uid_t uid) const;
    WSocket *waylandSocketForUid(uid_t uid) const;
    std::shared_ptr<Session> sessionForId(int id) const;
    std::shared_ptr<Session> sessionForUid(uid_t uid) const;
    std::shared_ptr<Session> sessionForUser(const QString &username) const;
    std::shared_ptr<Session> sessionForXWayland(WXWayland *xwayland) const;
    std::shared_ptr<Session> sessionForSocket(WSocket *socket) const;
    std::weak_ptr<Session> activeSession() const;

    WSocket *globalWaylandSocket() const;
    WXWayland *globalXWayland() const;

    PersonalizationV1 *personalization() const;

    WSeat *seat() const;

    bool toggleDebugMenuBar();

    WindowManagementV1::DesktopState showDesktopState() const;

    Q_INVOKABLE bool isLaunchpad(WLayerSurface *surface) const;

    void handleWindowPicker(WindowPickerInterface *picker);

    RootSurfaceContainer *rootSurfaceContainer() const;

    void setMultitaskViewImpl(IMultitaskView *impl);
    void setLockScreenImpl(ILockScreen *impl);

    CurrentMode currentMode() const
    {
        return m_currentMode;
    }

    void setCurrentMode(CurrentMode mode);

    void showLockScreen(bool switchToGreeter = true);

    Output* getOutputAtCursor() const;

    UserModel *userModel() const;
    DDMInterfaceV1 *ddmInterfaceV1() const;

    void activateSession();
    void deactivateSession();
    void enableRender();
    void disableRender();

    void setBlockActivateSurface(bool block);
    bool blockActivateSurface() const;
    bool noAnimation() const;
    void toggleFpsDisplay();

    void updateIdleInhibitor();

    bool setXWindowPositionRelative(uint wid, WSurface *anchor, wl_fixed_t dx, wl_fixed_t dy) const;

public Q_SLOTS:
    void activateSurface(SurfaceWrapper *wrapper, Qt::FocusReason reason = Qt::OtherFocusReason);
    void forceActivateSurface(SurfaceWrapper *wrapper,
                              Qt::FocusReason reason = Qt::OtherFocusReason);
    void fakePressSurfaceBottomRightToReszie(SurfaceWrapper *surface);
    bool surfaceBelongsToCurrentSession(SurfaceWrapper *wrapper);

Q_SIGNALS:
    void socketEnabledChanged();
    void primaryOutputChanged();
    void activatedSurfaceChanged();

    void animationSpeedChanged();
    void socketFileChanged();
    void outputModeChanged();

    void currentModeChanged();
    void noAnimationChanged();

    void blockActivateSurfaceChanged();
    void requestQuit();

private Q_SLOTS:
    void onShowDesktop();
    void deleteTaskSwitch();
    void onPrepareForSleep(bool sleep);
    void onSessionNew(const QString &sessionId, const QDBusObjectPath &objectPath);
    void onSessionLock();
    void onSessionUnlock();

private:
    void onOutputAdded(WOutput *output);
    void onOutputRemoved(WOutput *output);
    void onSurfaceModeChanged(WSurface *surface, WXdgDecorationManager::DecorationMode mode);
    void setGamma(struct wlr_gamma_control_manager_v1_set_gamma_event *event);
    void onOutputTestOrApply(qw_output_configuration_v1 *config, bool onlyTest);
    void onSetOutputPowerMode(wlr_output_power_v1_set_mode_event *event);
    void onNewIdleInhibitor(wlr_idle_inhibitor_v1 *inhibitor);
    void onDockPreview(std::vector<SurfaceWrapper *> surfaces,
                       WSurface *target,
                       QPoint pos,
                       ForeignToplevelV1::PreviewDirection direction);
    void onDockPreviewTooltip(QString tooltip,
                              WSurface *target,
                              QPoint pos,
                              ForeignToplevelV1::PreviewDirection direction);
    void onSetCopyOutput(treeland_virtual_output_v1 *virtual_output);
    void onRestoreCopyOutput(treeland_virtual_output_v1 *virtual_output);
    void onSurfaceWrapperAdded(SurfaceWrapper *wrapper);
    void onSurfaceWrapperAboutToRemove(SurfaceWrapper *wrapper);
    void handleRequestDrag([[maybe_unused]] WSurface *surface);
    void handleLockScreen(LockScreenInterface *lockScreen);
    void handleNewForeignToplevelCaptureRequest(wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request *request);
    void onExtSessionLock(WSessionLock *lock);

private:
    void allowNonDrmOutputAutoChangeMode(WOutput *output);
    int indexOfOutput(WOutput *output) const;

    SurfaceWrapper *keyboardFocusSurface() const;
    void requestKeyboardFocusForSurface(SurfaceWrapper *newActivateSurface, Qt::FocusReason reason);
    SurfaceWrapper *activatedSurface() const;
    void setActivatedSurface(SurfaceWrapper *newActivateSurface);

    void setCursorPosition(const QPointF &position);

    bool beforeDisposeEvent(WSeat *seat, QWindow *watched, QInputEvent *event) override;
    bool afterHandleEvent([[maybe_unused]] WSeat *seat,
                          WSurface *watched,
                          QObject *surfaceItem,
                          QObject *,
                          QInputEvent *event) override;
    bool unacceptedEvent(WSeat *, QWindow *, QInputEvent *event) override;

    void handleLeftButtonStateChanged(const QInputEvent *event);
    void handleWhellValueChanged(const QInputEvent *event);
    bool doGesture(QInputEvent *event);
    Output *createNormalOutput(WOutput *output);
    Output *createCopyOutput(WOutput *output, Output *proxy);
    WOutputViewport *getOwnOutputViewport(WOutput *output);
    QList<SurfaceWrapper *> getWorkspaceSurfaces(Output *filterOutput = nullptr);
    void moveSurfacesToOutput(const QList<SurfaceWrapper *> &surfaces,
                              Output *targetOutput,
                              Output *sourceOutput);
    void handleCopyModeOutputDisable(Output *affectedOutput);
    void restoreCopyMode();
    void applyCopyModeToOutputs(Output *primaryOutput, const QList<SurfaceWrapper *> &surfaces);
    bool isNvidiaCardPresent();
    void setWorkspaceVisible(bool visible);
    void restoreFromShowDesktop(SurfaceWrapper *activeSurface = nullptr);
    void setNoAnimation(bool noAnimation);

    std::shared_ptr<Session> ensureSession(int id, QString username);
    void updateActiveUserSession(const QString &username, int id);
    bool isXWaylandClient(WClient *client);
    void configureNumlock();

    static Helper *m_instance;
    std::unique_ptr<TreelandUserConfig> m_config;
    std::unique_ptr<TreelandConfig> m_globalConfig;
    Treeland::Treeland *m_treeland = nullptr;
    FpsDisplayManager *m_fpsManager = nullptr;

    CurrentMode m_currentMode{ CurrentMode::Normal };

    // Sessions
    std::weak_ptr<Session> m_activeSession;
    QList<std::shared_ptr<Session>> m_sessions;

    // qtquick helper
    WOutputRenderWindow *m_renderWindow = nullptr;
    QQuickItem *m_dockPreview = nullptr;
    QQuickItem *m_fpsDisplay = nullptr;

    // gesture
    WServer *m_server = nullptr;
    RootSurfaceContainer *m_rootSurfaceContainer = nullptr;

    // wayland helper
    WSeat *m_seat = nullptr;
    WBackend *m_backend = nullptr;
    qw_renderer *m_renderer = nullptr;
    qw_allocator *m_allocator = nullptr;

    // protocols
    qw_compositor *m_compositor = nullptr;
    qw_idle_notifier_v1 *m_idleNotifier = nullptr;
    qw_idle_inhibit_manager_v1 *m_idleInhibitManager = nullptr;
    qw_output_power_manager_v1 *m_outputPowerManager = nullptr;
    qw_ext_foreign_toplevel_image_capture_source_manager_v1 *m_foreignToplevelImageCaptureManager = nullptr;
    ShellHandler *m_shellHandler = nullptr;
    WXdgDecorationManager *m_xdgDecorationManager = nullptr;
    WForeignToplevel *m_foreignToplevel = nullptr;
    WExtForeignToplevelListV1 *m_extForeignToplevelListV1 = nullptr;
    ForeignToplevelV1 *m_treelandForeignToplevel = nullptr;
    ShortcutManagerV2 *m_shortcutManager = nullptr;
    PersonalizationV1 *m_personalization = nullptr;
    WallpaperColorV1 *m_wallpaperColorV1 = nullptr;
    WOutputManagerV1 *m_outputManager = nullptr;
    WindowManagementV1 *m_windowManagement = nullptr;
    WindowManagementV1::DesktopState m_showDesktop = WindowManagementV1::DesktopState::Normal;
    DDEShellManagerInterfaceV1 *m_ddeShellV1 = nullptr;
    PrelaunchSplash *m_prelaunchSplash = nullptr; // treeland prelaunch splash protocol
    VirtualOutputV1 *m_virtualOutput = nullptr;
    OutputManagerV1 *m_outputManagerV1 = nullptr;
    DDMInterfaceV1 *m_ddmInterfaceV1 = nullptr;
    ScreensaverInterfaceV1 *m_screensaverInterfaceV1 = nullptr;
#ifdef EXT_SESSION_LOCK_V1
    WSessionLockManager *m_sessionLockManager = nullptr;
    QTimer *m_lockScreenGraceTimer = nullptr;
#endif
    // private data
    QList<Output *> m_outputList;
    OutputConfigState *m_outputConfigState = nullptr;
    OutputLifecycleManager *m_outputLifecycleManager = nullptr;
    QPointer<QQuickItem> m_taskSwitch;
    QList<qw_idle_inhibitor_v1 *> m_idleInhibitors;

    SurfaceWrapper *m_activatedSurface = nullptr;
    LockScreen *m_lockScreen = nullptr;
    float m_animationSpeed = 1.0;
    OutputMode m_mode = OutputMode::Extension;
    std::optional<QPointF> m_fakelastPressedPosition;

    QPointer<CaptureSourceSelector> m_captureSelector;

    QPropertyAnimation *m_workspaceScaleAnimation{ nullptr };
    QPropertyAnimation *m_workspaceOpacityAnimation{ nullptr };

    bool m_singleMetaKeyPendingPressed{ false };

    IMultitaskView *m_multitaskView{ nullptr };
    UserModel *m_userModel{ nullptr };

    bool m_blockActivateSurface{ false };

    bool m_noAnimation{ false };

    struct PendingOutputConfig {
        qw_output_configuration_v1 *config = nullptr;
        QList<WOutputState> states;
        int pendingCommits = 0;
        bool allSuccess = true;
    };
    PendingOutputConfig m_pendingOutputConfig;

    void onOutputCommitFinished(qw_output_configuration_v1 *config, bool success);
};
