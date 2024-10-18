// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include <wsurfaceitem.h>
#include <wtoplevelsurface.h>

#include <QQuickItem>

Q_MOC_INCLUDE(<woutput.h>)

WAYLIB_SERVER_USE_NAMESPACE

class QmlEngine;
class Output;
class SurfaceContainer;

class SurfaceWrapper : public QQuickItem
{
    friend class Helper;
    friend class SurfaceContainer;
    friend class SurfaceProxy;
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("SurfaceWrapper objects are created by c++")
    Q_PROPERTY(Type type READ type CONSTANT)
    // make to read only
    Q_PROPERTY(qreal implicitWidth READ implicitWidth NOTIFY implicitWidthChanged FINAL)
    Q_PROPERTY(qreal implicitHeight READ implicitHeight NOTIFY implicitHeightChanged FINAL)
    Q_PROPERTY(WAYLIB_SERVER_NAMESPACE::WSurface* surface READ surface CONSTANT)
    Q_PROPERTY(WAYLIB_SERVER_NAMESPACE::WToplevelSurface* shellSurface READ shellSurface CONSTANT)
    Q_PROPERTY(WAYLIB_SERVER_NAMESPACE::WSurfaceItem* surfaceItem READ surfaceItem CONSTANT)
    Q_PROPERTY(QRectF boundingRect READ boundingRect NOTIFY boundingRectChanged)
    Q_PROPERTY(QRectF geometry READ geometry NOTIFY geometryChanged FINAL)
    Q_PROPERTY(QRectF normalGeometry READ normalGeometry NOTIFY normalGeometryChanged FINAL)
    Q_PROPERTY(QRectF maximizedGeometry READ maximizedGeometry NOTIFY maximizedGeometryChanged FINAL)
    Q_PROPERTY(QRectF fullscreenGeometry READ fullscreenGeometry NOTIFY fullscreenGeometryChanged FINAL)
    Q_PROPERTY(QRectF tilingGeometry READ tilingGeometry NOTIFY tilingGeometryChanged FINAL)
    Q_PROPERTY(Output* ownsOutput READ ownsOutput NOTIFY ownsOutputChanged FINAL)
    Q_PROPERTY(bool positionAutomatic READ positionAutomatic WRITE setPositionAutomatic NOTIFY positionAutomaticChanged FINAL)
    Q_PROPERTY(State previousSurfaceState READ previousSurfaceState NOTIFY previousSurfaceStateChanged FINAL)
    Q_PROPERTY(State surfaceState READ surfaceState NOTIFY surfaceStateChanged BINDABLE bindableSurfaceState FINAL)
    Q_PROPERTY(qreal radius READ radius WRITE setRadius NOTIFY radiusChanged FINAL)
    Q_PROPERTY(SurfaceContainer* container READ container NOTIFY containerChanged FINAL)
    Q_PROPERTY(QQuickItem* titleBar READ titleBar NOTIFY noTitleBarChanged FINAL)
    Q_PROPERTY(QQuickItem* decoration READ decoration NOTIFY noDecorationChanged FINAL)
    Q_PROPERTY(bool visibleDecoration READ visibleDecoration NOTIFY visibleDecorationChanged FINAL)
    Q_PROPERTY(bool clipInOutput READ clipInOutput WRITE setClipInOutput NOTIFY clipInOutputChanged FINAL)
    Q_PROPERTY(bool noTitleBar READ noTitleBar RESET resetNoTitleBar NOTIFY noTitleBarChanged FINAL)
    Q_PROPERTY(bool noCornerRadius READ noCornerRadius NOTIFY noCornerRadiusChanged FINAL)
    Q_PROPERTY(int workspaceId READ workspaceId NOTIFY workspaceIdChanged FINAL)
    Q_PROPERTY(bool alwaysOnTop READ alwaysOnTop WRITE setAlwaysOnTop NOTIFY alwaysOnTopChanged FINAL)
    Q_PROPERTY(bool showOnAllWorkspace READ showOnAllWorkspace NOTIFY showOnAllWorkspaceChanged FINAL)
    Q_PROPERTY(bool skipSwitcher READ skipSwitcher WRITE setSkipSwitcher NOTIFY skipSwitcherChanged FINAL)
    Q_PROPERTY(bool skipDockPreView READ skipDockPreView WRITE setSkipDockPreView NOTIFY skipDockPreViewChanged FINAL)
    Q_PROPERTY(bool skipMutiTaskView READ skipMutiTaskView WRITE setSkipMutiTaskView NOTIFY skipMutiTaskViewChanged FINAL)
    Q_PROPERTY(bool isDdeShellSurface READ isDdeShellSurface WRITE setIsDdeShellSurface NOTIFY isDdeShellSurfaceChanged FINAL)
    Q_PROPERTY(SurfaceWrapper::SurfaceRole surfaceRole READ surfaceRole WRITE setSurfaceRole NOTIFY surfaceRoleChanged FINAL)
    // y-axis offset distance, set the vertical alignment of the surface within the cursor width. if autoPlaceYOffset > 0,
    // preventing SurfaceWrapper from being displayed beyond the edge of the output.
    Q_PROPERTY(quint32 autoPlaceYOffset READ autoPlaceYOffset WRITE setAutoPlaceYOffset NOTIFY autoPlaceYOffsetChanged FINAL)
    // wayland client can control the position of SurfaceWrapper on the output through treeland_dde_shell_surface_v1.set_surface_position
    Q_PROPERTY(QPoint clientRequstPos READ clientRequstPos WRITE setClientRequstPos NOTIFY clientRequstPosChanged FINAL)
    Q_PROPERTY(bool blur READ blur NOTIFY blurChanged FINAL)

public:
    enum class Type {
        XdgToplevel,
        XdgPopup,
        XWayland,
        Layer,
        InputPopup,
    };
    Q_ENUM(Type)

    enum class State {
        Normal,
        Maximized,
        Minimized,
        Fullscreen,
        Tiling,
    };
    Q_ENUM(State)

    enum class ActiveControlState : quint16 {
        Mapped = 1,
        UnMinimized = 2,
        HasInitializeContainer = 8, // when not in Container, we can't stackToLast
        Full = Mapped | UnMinimized | HasInitializeContainer,
    };
    Q_ENUM(ActiveControlState);
    Q_DECLARE_FLAGS(ActiveControlStates, ActiveControlState);

    enum class SurfaceRole {
        Normal,
        Overlay,
    };
    Q_ENUM(SurfaceRole)

    explicit SurfaceWrapper(QmlEngine *qmlEngine,
                            WToplevelSurface *shellSurface,
                            Type type,
                            QQuickItem *parent = nullptr);
    ~SurfaceWrapper();

    void setFocus(bool focus, Qt::FocusReason reason);

    WSurface *surface() const;
    WToplevelSurface *shellSurface() const;
    WSurfaceItem *surfaceItem() const;
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
    bool isCloseAnimationRunning() const;
    void setRemoveWrapper(bool remove);

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

    bool alwaysOnTop() const;
    void setAlwaysOnTop(bool alwaysOnTop);

    bool showOnAllWorkspace() const;
    bool showOnWorkspace(int workspaceIndex) const;

    bool hasActiveCapability() const;

    bool skipSwitcher() const;
    void setSkipSwitcher(bool skip);

    bool skipDockPreView() const;
    void setSkipDockPreView(bool skip);

    bool skipMutiTaskView() const;
    void setSkipMutiTaskView(bool skip);

    bool isDdeShellSurface() const;
    void setIsDdeShellSurface(bool value);

    enum SurfaceRole surfaceRole() const;
    void setSurfaceRole(enum SurfaceRole role);

    quint32 autoPlaceYOffset() const;
    void setAutoPlaceYOffset(quint32 offset);

    QPoint clientRequstPos() const;
    void setClientRequstPos(QPoint pos);
    bool blur() const;
    void setBlur(bool blur);

public Q_SLOTS:
    // for titlebar
    void requestMinimize();
    void requestCancelMinimize();
    void requestMaximize();
    void requestCancelMaximize();
    void requestToggleMaximize();
    void requestFullscreen();
    void requestCancelFullscreen();
    void requestClose();
    void onMappedChanged();

    bool stackBefore(QQuickItem *item);
    bool stackAfter(QQuickItem *item);
    void stackToLast();

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
    void requestShowWindowMenu(QPoint pos);
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
    void requestDeactive();
    void requestForceActive();
    void skipSwitcherChanged();
    void skipDockPreViewChanged();
    void skipMutiTaskViewChanged();
    void isDdeShellSurfaceChanged();
    void surfaceRoleChanged();
    void autoPlaceYOffsetChanged();
    void clientRequstPosChanged();
    void blurChanged();

private:
    using QQuickItem::setParentItem;
    using QQuickItem::stackAfter;
    using QQuickItem::stackBefore;
    void setParent(QQuickItem *item);
    void setActivate(bool activate);
    void setNormalGeometry(const QRectF &newNormalGeometry);
    void setNoDecoration(bool newNoDecoration);
    void updateTitleBar();
    void setBoundedRect(const QRectF &newBoundedRect);
    void setContainer(SurfaceContainer *newContainer);
    void setVisibleDecoration(bool newVisibleDecoration);
    void updateBoundingRect();
    void updateVisible();
    void updateSubSurfaceStacking();
    void updateClipRect();
    void geometryChange(const QRectF &newGeo, const QRectF &oldGeometry) override;
    void createNewOrClose(uint direction);

    void doSetSurfaceState(State newSurfaceState);
    Q_SLOT void onAnimationReady();
    Q_SLOT void onAnimationFinished();
    bool startStateChangeAnimation(SurfaceWrapper::State targetState, const QRectF &targetGeometry);
    Q_SLOT void onNewAnimationFinished();
    void updateExplicitAlwaysOnTop();
    void startMinimizeAnimation(const QRectF &iconGeometry, uint direction);
    Q_SLOT void onMinimizeAnimationFinished();
    void startShowAnimation(bool show);
    Q_SLOT void onShowAnimationFinished();
    void updateHasActiveCapability(ActiveControlState state, bool value);

    QmlEngine *m_engine;
    QPointer<SurfaceContainer> m_container;
    QList<SurfaceWrapper *> m_subSurfaces;
    SurfaceWrapper *m_parentSurface = nullptr;

    WToplevelSurface *m_shellSurface = nullptr;
    WSurfaceItem *m_surfaceItem = nullptr;
    QPointer<QQuickItem> m_titleBar;
    QPointer<QQuickItem> m_decoration;
    QPointer<QQuickItem> m_blurContent;
    QPointer<QQuickItem> m_geometryAnimation;
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
    QPointer<QQuickItem> m_newAnimation;
    QPointer<QQuickItem> m_minimizeAnimation;
    QPointer<QQuickItem> m_showAnimation;
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
    uint m_removeWrapperEndOfAnimation : 1;
    uint m_skipSwitcher : 1;
    uint m_skipDockPreView : 1;
    uint m_skipMutiTaskView : 1;
    uint m_isDdeShellSurface : 1;
    SurfaceRole m_surfaceRole = SurfaceRole::Normal;
    quint32 m_autoPlaceYOffset = 0;
    QPoint m_clientRequstPos;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(SurfaceWrapper::ActiveControlStates)

