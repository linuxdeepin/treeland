// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "modules/foreign-toplevel/foreigntoplevelmanagerv1.h"
#include "core/qmlengine.h"
#include "input/togglablegesture.h"
#include "modules/virtual-output/virtualoutputmanager.h"
#include "modules/window-management/windowmanagement.h"

#include <wglobal.h>
#include <wqmlcreator.h>
#include <wseat.h>
#include <wxdgdecorationmanager.h>
#include <wseatmanager.h>

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
class WOutputManagerV1;
class WLayerSurface;
class WSeatManager;
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
class ShortcutV1;
class PersonalizationV1;
class WallpaperColorV1;
class WindowManagementV1;
class Multitaskview;
class DDEShellManagerInterfaceV1;
class WindowPickerInterface;
class VirtualOutputV1;
class ShellHandler;
class PrimaryOutputV1;
class CaptureSourceSelector;
class treeland_window_picker_v1;
class IMultitaskView;
class LockScreenInterface;
class ILockScreen;
class UserModel;
class FpsDisplayManager;
struct wlr_idle_inhibitor_v1;
struct wlr_output_power_v1_set_mode_event;

class Helper : public WSeatEventFilter
{
    friend class RootSurfaceContainer;
    Q_OBJECT
    Q_PROPERTY(bool socketEnabled READ socketEnabled WRITE setSocketEnabled NOTIFY socketEnabledChanged FINAL)
    Q_PROPERTY(RootSurfaceContainer* rootContainer READ rootContainer CONSTANT FINAL)
    Q_PROPERTY(float animationSpeed READ animationSpeed WRITE setAnimationSpeed NOTIFY animationSpeedChanged FINAL)
    Q_PROPERTY(OutputMode outputMode READ outputMode WRITE setOutputMode NOTIFY outputModeChanged FINAL)
    Q_PROPERTY(QString cursorTheme READ cursorTheme NOTIFY cursorThemeChanged FINAL)
    Q_PROPERTY(QSize cursorSize READ cursorSize NOTIFY cursorSizeChanged FINAL)
    Q_PROPERTY(TogglableGesture* multiTaskViewGesture READ multiTaskViewGesture CONSTANT)
    Q_PROPERTY(TogglableGesture* windowGesture READ windowGesture CONSTANT)
    Q_PROPERTY(SurfaceWrapper* activatedSurface READ activatedSurface NOTIFY activatedSurfaceChanged FINAL)
    Q_PROPERTY(Workspace* workspace READ workspace CONSTANT FINAL)
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

    static bool getSeatMoveResizeInfo(SurfaceWrapper *surface, Qt::Edges &edges, QRectF &startGeometry);
    static bool isSettingSeatPosition();
    static void setSeatPositionFlag(bool setting);

    QmlEngine *qmlEngine() const;
    WOutputRenderWindow *window() const;
    ShellHandler *shellHandler() const;
    Workspace *workspace() const;

    void init();

    TogglableGesture *multiTaskViewGesture() const
    {
        return m_multiTaskViewGesture;
    }

    TogglableGesture *windowGesture() const
    {
        return m_windowGesture;
    }

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
    WXWayland *createXWayland();
    void removeXWayland(WXWayland *xwayland);

    WSocket *defaultWaylandSocket() const;
    WXWayland *defaultXWaylandSocket() const;

    PersonalizationV1 *personalization() const;

    WSeat *seat() const;

    bool toggleDebugMenuBar();

    QString cursorTheme() const;
    QSize cursorSize() const;
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

    void showLockScreen();

    Output* getOutputAtCursor() const;

    WSeatManager *seatManager() const;
    void initMultiSeat();
    void loadSeatConfig();
    void saveSeatConfig();
    bool validateSeatConfig(const QJsonObject &config) const;

    void checkAndFixSeatDevices();
    void assignDeviceToSeat(WInputDevice *device);

    WSeat *getSeatForDevice(WInputDevice *device) const;
    WSeat *getSeatForEvent(QInputEvent *event) const;
    WSeat *findSeatForSurface(SurfaceWrapper *wrapper) const;
    void setActivatedSurfaceForSeat(WSeat *seat, SurfaceWrapper *surface);
    SurfaceWrapper *getActivatedSurfaceForSeat(WSeat *seat) const;
    void toggleFpsDisplay();

public Q_SLOTS:
    void activateSurface(SurfaceWrapper *wrapper, Qt::FocusReason reason = Qt::OtherFocusReason);
    void forceActivateSurface(SurfaceWrapper *wrapper,
                              Qt::FocusReason reason = Qt::OtherFocusReason);
    void fakePressSurfaceBottomRightToReszie(SurfaceWrapper *surface);
    bool surfaceBelongsToCurrentUser(SurfaceWrapper *wrapper);

Q_SIGNALS:
    void socketEnabledChanged();
    void primaryOutputChanged();
    void activatedSurfaceChanged();

    void animationSpeedChanged();
    void socketFileChanged();
    void outputModeChanged();
    void cursorThemeChanged();
    void cursorSizeChanged();

    void currentModeChanged();

private Q_SLOTS:
    void onShowDesktop();
    void deleteTaskSwitch();

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
    void onSessionNew(const QString &sessionId, const QDBusObjectPath &sessionPath);
    void onSessionLock();
    void onSessionUnlock();

    void allowNonDrmOutputAutoChangeMode(WOutput *output);

    int indexOfOutput(WOutput *output) const;

    void setOutputProxy(Output *output);

    SurfaceWrapper *keyboardFocusSurface() const;
    void requestKeyboardFocusForSurfaceForSeat(WSeat *seat, SurfaceWrapper *newActivate, Qt::FocusReason reason);
    SurfaceWrapper *getKeyboardFocusSurfaceForSeat(WSeat *seat) const;
    SurfaceWrapper *activatedSurface() const;
    void setActivatedSurface(SurfaceWrapper *newActivateSurface);

    void setCursorPosition(const QPointF &position);

    bool beforeDisposeEvent(WSeat *seat, QWindow *window, QInputEvent *event) override;
    bool afterHandleEvent(WSeat *seat, WSurface *watched, QObject *shellObject,
                         QObject *eventObject, QInputEvent *event) override;
    bool unacceptedEvent(WSeat *seat, QWindow *window, QInputEvent *event) override;

    void handleLeftButtonStateChanged(const QInputEvent *event);
    void handleWhellValueChanged(const QInputEvent *event);
    bool doGesture(QInputEvent *event);
    Output *createNormalOutput(WOutput *output);
    Output *createCopyOutput(WOutput *output, Output *proxy);
    QList<SurfaceWrapper *> getWorkspaceSurfaces(Output *filterOutput = nullptr);
    void moveSurfacesToOutput(const QList<SurfaceWrapper *> &surfaces,
                              Output *targetOutput,
                              Output *sourceOutput);
    bool isNvidiaCardPresent();
    void setWorkspaceVisible(bool visible);
    void restoreFromShowDesktop(SurfaceWrapper *activeSurface = nullptr);
    void updateIdleInhibitor();

    void setupSeatsConfiguration();
    void connectDeviceSignals();
    void assignExistingDevices();

    void beginMoveResizeForSeat(WSeat *seat, SurfaceWrapper *surface, Qt::Edges edges);
    void endMoveResizeForSeat(WSeat *seat);
    SurfaceWrapper *getMoveResizeSurfaceForSeat(WSeat *seat) const;
    void doMoveResizeForSeat(WSeat *seat, const QPointF &delta);

    WSeat *getLastInteractingSeat(SurfaceWrapper *surface) const;
    void updateSurfaceSeatInteraction(SurfaceWrapper *surface, WSeat *seat);

    void switchWorkspaceForSeat(WSeat *seat, int index);
    void handleRequestDragForSeat(WSeat *seat, WSurface *surface);

    bool getSingleMetaKeyPendingPressed(WSeat *seat) const;
    void setSingleMetaKeyPendingPressed(WSeat *seat, bool pressed);

    WSeat *m_currentEventSeat = nullptr;

    static Helper *m_instance;

    CurrentMode m_currentMode{ CurrentMode::Normal };

    // qtquick helper
    WOutputRenderWindow *m_renderWindow = nullptr;
    QQuickItem *m_dockPreview = nullptr;

    // gesture
    TogglableGesture *m_multiTaskViewGesture = nullptr;
    TogglableGesture *m_windowGesture = nullptr;

    // wayland helper
    WServer *m_server = nullptr;
    WSocket *m_socket = nullptr;
    WSeat *m_seat = nullptr;
    WBackend *m_backend = nullptr;
    qw_renderer *m_renderer = nullptr;
    qw_allocator *m_allocator = nullptr;

    // protocols
    qw_compositor *m_compositor = nullptr;
    qw_idle_notifier_v1 *m_idleNotifier = nullptr;
    qw_idle_inhibit_manager_v1 *m_idleInhibitManager = nullptr;
    qw_output_power_manager_v1 *m_outputPowerManager = nullptr;
    ShellHandler *m_shellHandler = nullptr;
    WXWayland *m_defaultXWayland = nullptr;
    WXdgDecorationManager *m_xdgDecorationManager = nullptr;
    WForeignToplevel *m_foreignToplevel = nullptr;
    ForeignToplevelV1 *m_treelandForeignToplevel = nullptr;
    ShortcutV1 *m_shortcut = nullptr;
    PersonalizationV1 *m_personalization = nullptr;
    WallpaperColorV1 *m_wallpaperColorV1 = nullptr;
    WOutputManagerV1 *m_outputManager = nullptr;
    WindowManagementV1 *m_windowManagement = nullptr;
    WindowManagementV1::DesktopState m_showDesktop = WindowManagementV1::DesktopState::Normal;
    DDEShellManagerInterfaceV1 *m_ddeShellV1 = nullptr;
    VirtualOutputV1 *m_virtualOutput = nullptr;
    PrimaryOutputV1 *m_primaryOutputV1 = nullptr;

    // private data
    QList<Output *> m_outputList;
    QPointer<QQuickItem> m_taskSwitch;
    QList<qw_idle_inhibitor_v1 *> m_idleInhibitors;

    SurfaceWrapper *m_activatedSurface = nullptr;
    RootSurfaceContainer *m_rootSurfaceContainer = nullptr;
    LockScreen *m_lockScreen = nullptr;
    float m_animationSpeed = 1.0;
    quint64 m_taskAltTimestamp = 0;
    quint32 m_taskAltCount = 0;
    OutputMode m_mode = OutputMode::Extension;
    std::optional<QPointF> m_fakelastPressedPosition;

    QPointer<CaptureSourceSelector> m_captureSelector;

    QPropertyAnimation *m_workspaceScaleAnimation{ nullptr };
    QPropertyAnimation *m_workspaceOpacityAnimation{ nullptr };

    bool m_singleMetaKeyPendingPressed{ false };

    IMultitaskView *m_multitaskView{ nullptr };
    UserModel *m_userModel{ nullptr };
    FpsDisplayManager *m_fpsDisplayManager{ nullptr };

    quint32 m_atomDeepinNoTitlebar;

    WSeatManager *m_seatManager = nullptr;
    QString m_seatConfigPath;

    QMap<WSeat*, SurfaceWrapper*> m_seatActivatedSurfaces;
    struct SeatMoveResizeState {
        SurfaceWrapper *surface = nullptr;
        Qt::Edges edges;
        QRectF startGeometry;
        QPointF initialPosition;
    };
    QMap<WSeat*, SeatMoveResizeState> m_seatMoveResizeStates;
    QMap<WSeat*, QPointF> m_seatLastPressedPositions;
    QMap<WSeat*, bool> m_seatMetaKeyStates;
    QMap<WSeat*, SurfaceWrapper*> m_seatKeyboardFocusSurfaces;
};
