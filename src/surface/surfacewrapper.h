// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include <wsurfaceitem.h>
#include <wtoplevelsurface.h>

#include <QQuickItem>
#include <QPointer>
#include <QString>

Q_MOC_INCLUDE(<woutput.h>)
Q_MOC_INCLUDE(<output / output.h>)

WAYLIB_SERVER_USE_NAMESPACE

class QmlEngine;
class Output;
class SurfaceContainer;

class SurfaceWrapper : public QQuickItem
{
    friend class Helper;
    friend class SurfaceContainer;
    friend class SurfaceProxy;
    friend class ShellHandler;
    friend class LockScreen;
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("SurfaceWrapper objects are created by c++")
    Q_PROPERTY(Type type READ type NOTIFY surfaceItemChanged)
    Q_PROPERTY(QString appId READ appId CONSTANT)
    // make to read only
    Q_PROPERTY(qreal implicitWidth READ implicitWidth NOTIFY implicitWidthChanged FINAL)
    Q_PROPERTY(qreal implicitHeight READ implicitHeight NOTIFY implicitHeightChanged FINAL)
    Q_PROPERTY(WAYLIB_SERVER_NAMESPACE::WSurface* surface READ surface CONSTANT)
    Q_PROPERTY(WAYLIB_SERVER_NAMESPACE::WToplevelSurface* shellSurface READ shellSurface CONSTANT)
    Q_PROPERTY(WAYLIB_SERVER_NAMESPACE::WSurfaceItem* surfaceItem READ surfaceItem NOTIFY surfaceItemChanged)
    Q_PROPERTY(QQuickItem* prelaunchSplash READ prelaunchSplash NOTIFY prelaunchSplashChanged)
    Q_PROPERTY(QRectF boundingRect READ boundingRect NOTIFY boundingRectChanged)
    Q_PROPERTY(QRectF geometry READ geometry NOTIFY geometryChanged FINAL)
    Q_PROPERTY(QRectF normalGeometry READ normalGeometry NOTIFY normalGeometryChanged FINAL)
    Q_PROPERTY(QRectF maximizedGeometry READ maximizedGeometry NOTIFY maximizedGeometryChanged FINAL)
    Q_PROPERTY(QRectF fullscreenGeometry READ fullscreenGeometry NOTIFY fullscreenGeometryChanged FINAL)
    Q_PROPERTY(QRectF tilingGeometry READ tilingGeometry NOTIFY tilingGeometryChanged FINAL)
    Q_PROPERTY(Output* ownsOutput READ ownsOutput NOTIFY ownsOutputChanged FINAL)
    Q_PROPERTY(bool positionAutomatic READ positionAutomatic NOTIFY positionAutomaticChanged FINAL)
    Q_PROPERTY(State previousSurfaceState READ previousSurfaceState NOTIFY previousSurfaceStateChanged FINAL)
    Q_PROPERTY(State surfaceState READ surfaceState NOTIFY surfaceStateChanged BINDABLE bindableSurfaceState FINAL)
    Q_PROPERTY(qreal radius READ radius NOTIFY radiusChanged FINAL)
    Q_PROPERTY(SurfaceContainer* container READ container NOTIFY containerChanged FINAL)
    Q_PROPERTY(QQuickItem* titleBar READ titleBar NOTIFY noTitleBarChanged FINAL)
    Q_PROPERTY(QQuickItem* decoration READ decoration NOTIFY noDecorationChanged FINAL)
    Q_PROPERTY(bool visibleDecoration READ visibleDecoration NOTIFY visibleDecorationChanged FINAL)
    Q_PROPERTY(bool clipInOutput READ clipInOutput NOTIFY clipInOutputChanged FINAL)
    Q_PROPERTY(bool noTitleBar READ noTitleBar RESET resetNoTitleBar NOTIFY noTitleBarChanged FINAL)
    Q_PROPERTY(bool noCornerRadius READ noCornerRadius NOTIFY noCornerRadiusChanged FINAL)
    Q_PROPERTY(int workspaceId READ workspaceId NOTIFY workspaceIdChanged FINAL)
    Q_PROPERTY(bool alwaysOnTop READ alwaysOnTop WRITE setAlwaysOnTop NOTIFY alwaysOnTopChanged FINAL)
    Q_PROPERTY(bool showOnAllWorkspace READ showOnAllWorkspace NOTIFY showOnAllWorkspaceChanged FINAL)
    Q_PROPERTY(bool skipSwitcher READ skipSwitcher NOTIFY skipSwitcherChanged FINAL)
    Q_PROPERTY(bool skipDockPreView READ skipDockPreView NOTIFY skipDockPreViewChanged FINAL)
    Q_PROPERTY(bool skipMutiTaskView READ skipMutiTaskView NOTIFY skipMutiTaskViewChanged FINAL)
    Q_PROPERTY(bool isDDEShellSurface READ isDDEShellSurface NOTIFY isDDEShellSurfaceChanged FINAL)
    Q_PROPERTY(SurfaceWrapper::SurfaceRole surfaceRole READ surfaceRole NOTIFY surfaceRoleChanged FINAL)
    // y-axis offset distance, set the vertical alignment of the surface within
    // the cursor width. if autoPlaceYOffset > 0, preventing SurfaceWrapper from
    // being displayed beyond the edge of the output.
    Q_PROPERTY(quint32 autoPlaceYOffset READ autoPlaceYOffset NOTIFY autoPlaceYOffsetChanged FINAL)
    // wayland client can control the position of SurfaceWrapper on the output
    // through treeland_dde_shell_surface_v1.set_surface_position
    Q_PROPERTY(QPoint clientRequstPos READ clientRequstPos NOTIFY clientRequstPosChanged FINAL)
    Q_PROPERTY(bool blur READ blur NOTIFY blurChanged FINAL)
    Q_PROPERTY(bool isWindowAnimationRunning READ isWindowAnimationRunning NOTIFY windowAnimationRunningChanged FINAL)
    Q_PROPERTY(bool coverEnabled READ coverEnabled NOTIFY coverEnabledChanged FINAL)
    Q_PROPERTY(bool acceptKeyboardFocus READ acceptKeyboardFocus NOTIFY acceptKeyboardFocusChanged FINAL)

public:
    enum class Type
    {
        XdgToplevel,
        XdgPopup,
        XWayland,
        Layer,
        InputPopup,
        LockScreen,
        Undetermined,  // Used for pre-launch splash screen
    };
    Q_ENUM(Type)

    enum class State
    {
        Normal,
        Maximized,
        Minimized,
        Fullscreen,
        Tiling,
    };
    Q_ENUM(State)

    enum class ActiveControlState : quint16
    {
        Mapped = 1 << 0,
        UnMinimized = 1 << 1,
        HasInitializeContainer = 1 << 2, // when not in Container, we can't stackToLast
        Full = Mapped | UnMinimized | HasInitializeContainer,
    };
    Q_ENUM(ActiveControlState);
    Q_DECLARE_FLAGS(ActiveControlStates, ActiveControlState)

    enum class SurfaceRole
    {
        Normal,
        Overlay,
    };
    Q_ENUM(SurfaceRole)

    explicit SurfaceWrapper(QmlEngine *qmlEngine,
                            WToplevelSurface *shellSurface,
                            Type type,
                            QQuickItem *parent = nullptr,
                            bool isProxy = false);
    
    // Constructor for pre-launch splash; allows passing an initial window size to stabilize UI early
    explicit SurfaceWrapper(QmlEngine *qmlEngine,
                            QQuickItem *parent,
                            const QSize &initialSize,
                            const QString &appId);

    void setFocus(bool focus, Qt::FocusReason reason);

    WSurface *surface() const;
    WToplevelSurface *shellSurface() const;
    WSurfaceItem *surfaceItem() const;
    QQuickItem *prelaunchSplash() const;
    QString appId() const;
    bool resize(const QSizeF &size);

    QRectF titlebarGeometry() const;
    QRectF boundingRect() const override;

    Type type() const;
    SurfaceWrapper *parentSurface() const;

    Output *ownsOutput() const;
    void setOwnsOutput(Output *newOwnsOutput);
    void setOutputs(const QList<WOutput *> &outputs);

    QRectF geometry() const;
    QRectF normalGeometry() const;
    void moveNormalGeometryInOutput(const QPointF &position);
    QPointF alignToPixelGrid(const QPointF &pos) const;
    QRectF alignGeometryToPixelGrid(const QRectF &geometry) const;
    qreal getOutputDevicePixelRatio(const QPointF &pos) const;

    QRectF maximizedGeometry() const;
    void setMaximizedGeometry(const QRectF &newMaximizedGeometry);

    QRectF fullscreenGeometry() const;
    void setFullscreenGeometry(const QRectF &newFullscreenGeometry);

    QRectF tilingGeometry() const;
    void setTilingGeometry(const QRectF &newTilingGeometry);

    bool positionAutomatic() const;
    void setPositionAutomatic(bool newPositionAutomatic);

    void resetWidth();
    void resetHeight();

    State previousSurfaceState() const;
    State surfaceState() const;
    void setSurfaceState(State newSurfaceState);
    QBindable<State> bindableSurfaceState();
    bool isNormal() const;
    bool isMaximized() const;
    bool isMinimized() const;
    bool isTiling() const;
    bool isAnimationRunning() const;
    bool isWindowAnimationRunning() const;

    qreal radius() const;
    void setRadius(qreal newRadius);

    SurfaceContainer *container() const;

    void addSubSurface(SurfaceWrapper *surface);
    void removeSubSurface(SurfaceWrapper *surface);
    const QList<SurfaceWrapper *> &subSurfaces() const;
    SurfaceWrapper *stackFirstSurface() const;
    SurfaceWrapper *stackLastSurface() const;
    bool hasChild(SurfaceWrapper *child) const;

    QQuickItem *titleBar() const;
    QQuickItem *decoration() const;

    bool noDecoration() const;
    bool visibleDecoration() const;
    void setNoDecoration(bool newNoDecoration);

    bool clipInOutput() const;
    void setClipInOutput(bool newClipInOutput);
    QRectF clipRect() const override;

    bool noTitleBar() const;
    void setNoTitleBar(bool newNoTitleBar);
    void resetNoTitleBar();

    bool noCornerRadius() const;
    void setNoCornerRadius(bool newNoCornerRadius);

    QRect iconGeometry() const;
    void setIconGeometry(QRect newIconGeometry);

    int workspaceId() const;
    void setWorkspaceId(int newWorkspaceId);
    void setHideByWorkspace(bool hide);

    bool alwaysOnTop() const;
    void setAlwaysOnTop(bool alwaysOnTop);

    bool showOnAllWorkspace() const;
    bool showOnWorkspace(int workspaceIndex) const;

    bool hasActiveCapability() const;

    bool skipSwitcher() const;
    bool skipDockPreView() const;
    bool skipMutiTaskView() const;

    bool isDDEShellSurface() const;
    void setIsDDEShellSurface(bool value);

    enum SurfaceRole surfaceRole() const;
    void setSurfaceRole(enum SurfaceRole role);

    quint32 autoPlaceYOffset() const;
    void setAutoPlaceYOffset(quint32 offset);

    QPoint clientRequstPos() const;
    void setClientRequstPos(QPoint pos);

    bool blur() const;
    void setBlur(bool blur);

    bool coverEnabled() const;
    void setCoverEnabled(bool enabled);

    bool socketEnabled() const;
    void setXwaylandPositionFromSurface(bool value);

    void setHasInitializeContainer(bool value);
    void disableWindowAnimation(bool disable = true);
    void setHideByShowDesk(bool show);
    void setHideByLockScreen(bool hide);

    void markWrapperToRemoved();

    bool acceptKeyboardFocus() const;
    void setAcceptKeyboardFocus(bool accept);

public Q_SLOTS:
    // for titlebar
    void requestMinimize(bool onAnimation = true);
    void requestCancelMinimize(bool onAnimation = true);
    void requestMaximize();
    void requestCancelMaximize();
    void requestToggleMaximize();
    void requestFullscreen();
    void requestCancelFullscreen();
    void requestClose();
    void onMappedChanged();
    void onSocketEnabledChanged();

    bool stackBefore(QQuickItem *item);
    bool stackAfter(QQuickItem *item);
    void stackToLast();

    void updateSurfaceSizeRatio();

Q_SIGNALS:
    void boundingRectChanged();
    void ownsOutputChanged();
    void normalGeometryChanged();
    void maximizedGeometryChanged();
    void fullscreenGeometryChanged();
    void tilingGeometryChanged();
    void positionAutomaticChanged();
    void previousSurfaceStateChanged();
    void surfaceStateChanged();
    void radiusChanged();
    void requestMove(); // for titlebar
    void requestResize(Qt::Edges edges);
    void requestShowWindowMenu(QPointF pos);
    void geometryChanged();
    void containerChanged();
    void visibleDecorationChanged();
    void clipInOutputChanged();
    void noDecorationChanged();
    void noTitleBarChanged();
    void noCornerRadiusChanged();
    void iconGeometryChanged();
    void workspaceIdChanged();
    void alwaysOnTopChanged();
    void showOnAllWorkspaceChanged();
    void requestActive();
    void requestInactive();
    void skipSwitcherChanged();
    void skipDockPreViewChanged();
    void skipMutiTaskViewChanged();
    void isDDEShellSurfaceChanged();
    void surfaceRoleChanged();
    void autoPlaceYOffsetChanged();
    void clientRequstPosChanged();
    void blurChanged();
    void windowAnimationRunningChanged();
    void coverEnabledChanged();
    void aboutToBeInvalidated();
    void acceptKeyboardFocusChanged();
    void surfaceItemChanged();
    void prelaunchSplashChanged();

private:
    ~SurfaceWrapper() override;
    using QQuickItem::setParentItem;
    using QQuickItem::setVisible;
    using QQuickItem::stackAfter;
    using QQuickItem::stackBefore;
    void setParent(QQuickItem *item);
    void setActivate(bool activate);
    void setNormalGeometry(const QRectF &newNormalGeometry);
    void updateTitleBar();
    void setBoundedRect(const QRectF &newBoundedRect);
    void setContainer(SurfaceContainer *newContainer);
    void setVisibleDecoration(bool newVisibleDecoration);
    
    void setup(); // Initialize m_surfaceItem related features
    void convertToNormalSurface(WToplevelSurface *shellSurface, Type type); // Transition from pre-launch mode to normal mode
    void updateBoundingRect();
    void updateVisible();
    void updateSubSurfaceStacking();
    void updateClipRect();
    void geometryChange(const QRectF &newGeo, const QRectF &oldGeometry) override;
    void createNewOrClose(uint direction);
    void itemChange(ItemChange change, const ItemChangeData &data) override;

    void doSetSurfaceState(State newSurfaceState);
    Q_SLOT void onAnimationReady();
    Q_SLOT void onAnimationFinished();
    Q_SLOT void onPrelaunchSplashDestroyRequested();
    bool startStateChangeAnimation(SurfaceWrapper::State targetState, const QRectF &targetGeometry);
    void onWindowAnimationFinished();
    Q_SLOT void onShowAnimationFinished();
    Q_SLOT void onHideAnimationFinished();
    void updateExplicitAlwaysOnTop();
    void startMinimizeAnimation(const QRectF &iconGeometry, uint direction);
    Q_SLOT void onMinimizeAnimationFinished();
    void startShowDesktopAnimation(bool show);
    Q_SLOT void onShowDesktopAnimationFinished();
    void updateHasActiveCapability(ActiveControlState state, bool value);

    // wayland set by treeland-dde-shell, x11 set by bypassManager/windowTypes
    void setSkipDockPreView(bool skip);
    void setSkipSwitcher(bool skip);
    void setSkipMutiTaskView(bool skip);

    QmlEngine *m_engine;
    QPointer<SurfaceContainer> m_container;
    QList<SurfaceWrapper *> m_subSurfaces;
    SurfaceWrapper *m_parentSurface = nullptr;

    QPointer<WToplevelSurface> m_shellSurface;
    WSurfaceItem *m_surfaceItem = nullptr;
    QPointer<QQuickItem> m_titleBar;
    QPointer<QQuickItem> m_decoration;
    QPointer<QQuickItem> m_geometryAnimation;
    QPointer<QQuickItem> m_coverContent;
    QPointer<QQuickItem> m_prelaunchSplash; // Pre-launch splash item
    QList<WOutput *> m_prelaunchOutputs; // Outputs for pre-launch splash
    QRectF m_boundedRect;
    QRectF m_normalGeometry;
    QRectF m_maximizedGeometry;
    QRectF m_fullscreenGeometry;
    QRectF m_tilingGeometry;
    Type m_type;
    QPointer<Output> m_ownsOutput;
    QPointF m_positionInOwnsOutput;
    SurfaceWrapper::State m_pendingState;
    QRectF m_pendingGeometry;
    QPointer<QQuickItem> m_windowAnimation;
    QPointer<QQuickItem> m_minimizeAnimation;
    QPointer<QQuickItem> m_showDesktopAnimation;
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(SurfaceWrapper,
                                         SurfaceWrapper::State,
                                         m_previousSurfaceState,
                                         State::Normal,
                                         &SurfaceWrapper::previousSurfaceStateChanged)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(SurfaceWrapper,
                                         SurfaceWrapper::State,
                                         m_surfaceState,
                                         State::Normal,
                                         &SurfaceWrapper::surfaceStateChanged)
    int m_workspaceId = -1;
    int m_explicitAlwaysOnTop = 0;
    qreal m_radius = 0.0;
    QRect m_iconGeometry;
    ActiveControlStates m_hasActiveCapability = ActiveControlState::UnMinimized;

    struct TitleBarState
    {
        constexpr static uint Default = 0;
        constexpr static uint Visible = 1;
        constexpr static uint Hidden = 2;
    };

    uint m_positionAutomatic : 1;
    uint m_visibleDecoration : 1;
    uint m_clipInOutput : 1;
    uint m_noDecoration : 1;
    uint m_titleBarState : 2;
    uint m_noCornerRadius : 1;
    uint m_alwaysOnTop : 1;
    uint m_skipSwitcher : 1;
    uint m_skipDockPreView : 1;
    uint m_skipMutiTaskView : 1;
    uint m_isDdeShellSurface : 1;
    uint m_xwaylandPositionFromSurface : 1;
    uint m_wrapperAboutToRemove : 1;
    uint m_isProxy : 1;
    uint m_hideByWorkspace : 1;
    uint m_hideByshowDesk : 1;
    uint m_hideByLockScreen : 1;
    uint m_confirmHideByLockScreen : 1;
    uint m_blur : 1;
    SurfaceRole m_surfaceRole = SurfaceRole::Normal;
    quint32 m_autoPlaceYOffset = 0;
    QPoint m_clientRequstPos;

    bool m_socketEnabled{ false };
    bool m_windowAnimationEnabled{ true };
    bool m_acceptKeyboardFocus{ true };
    const QString m_appId;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(SurfaceWrapper::ActiveControlStates)
