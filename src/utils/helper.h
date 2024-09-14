// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "togglablegesture.h"
#include "wglobal.h"

#include <WCursor>
#include <WLayerSurface>
#include <WOutput>
#include <WSeat>
#include <WSurfaceItem>
#include <wquickoutputlayout.h>
#include <wtoplevelsurface.h>
#include <wxdgdecorationmanager.h>

#include <QList>
#include <QQmlApplicationEngine>

Q_MOC_INCLUDE(<woutputrenderwindow.h>)
Q_MOC_INCLUDE(<wqmlcreator.h>)
Q_MOC_INCLUDE(<qwcompositor.h>)

Q_DECLARE_OPAQUE_POINTER(QWindow *)

WAYLIB_SERVER_BEGIN_NAMESPACE
class WQuickCursor;
class WQmlCreator;
class WOutputRenderWindow;
class WInputMethodHelper;
class WXdgOutputManager;
class WXWayland;
class WForeignToplevel;
WAYLIB_SERVER_END_NAMESPACE

QW_BEGIN_NAMESPACE
class qw_compositor;
QW_END_NAMESPACE

class ForeignToplevelV1;
struct wlr_output_event_request_state;
QW_USE_NAMESPACE
WAYLIB_SERVER_USE_NAMESPACE

struct OutputInfo;

struct Margins
{
    Q_GADGET
    Q_PROPERTY(uint32_t left MEMBER left)
    Q_PROPERTY(uint32_t top MEMBER top)
    Q_PROPERTY(uint32_t right MEMBER right)
    Q_PROPERTY(uint32_t bottom MEMBER bottom)
public:
    uint32_t left = 0, top = 0, right = 0, bottom = 0;
};

class Helper : public WSeatEventFilter
{
    Q_OBJECT
    Q_PROPERTY(WToplevelSurface* activatedSurface READ activatedSurface WRITE setActivateSurface NOTIFY activatedSurfaceChanged FINAL)
    Q_PROPERTY(WSurfaceItem* resizingItem READ resizingItem NOTIFY resizingItemChanged FINAL)
    Q_PROPERTY(WSurfaceItem *movingItem READ movingItem WRITE setMovingItem NOTIFY movingItemChanged FINAL)
    Q_PROPERTY(QString waylandSocket READ waylandSocket WRITE setWaylandSocket NOTIFY socketFileChanged FINAL)
    Q_PROPERTY(QString xwaylandSocket READ xwaylandSocket WRITE setXWaylandSocket NOTIFY socketFileChanged FINAL)
    Q_PROPERTY(QString currentUser READ currentUser WRITE setCurrentUser NOTIFY currentUserChanged FINAL)
    Q_PROPERTY(bool switcherOn MEMBER m_switcherOn NOTIFY switcherOnChanged FINAL)
    Q_PROPERTY(bool switcherEnabled MEMBER m_switcherEnabled FINAL)
    Q_PROPERTY(WQuickOutputLayout* outputLayout READ outputLayout CONSTANT)
    Q_PROPERTY(WSeat* seat READ seat CONSTANT)
    Q_PROPERTY(WCursor* cursor READ cursor CONSTANT)
    Q_PROPERTY(QW_NAMESPACE::qw_compositor* compositor READ compositor NOTIFY compositorChanged FINAL)
    Q_PROPERTY(WXdgDecorationManager *xdgDecorationManager READ xdgDecorationManager NOTIFY xdgDecorationManagerChanged)
    Q_PROPERTY(WQmlCreator* outputCreator READ outputCreator CONSTANT)
    Q_PROPERTY(WQmlCreator* surfaceCreator READ surfaceCreator CONSTANT)
    Q_PROPERTY(TogglableGesture* multiTaskViewGesture READ multiTaskViewGesture CONSTANT)
    Q_PROPERTY(TogglableGesture* windowGesture READ windowGesture CONSTANT)

    // TODO: move to workspace
    Q_PROPERTY(int currentWorkspaceId READ currentWorkspaceId WRITE setCurrentWorkspaceId NOTIFY currentWorkspaceIdChanged FINAL)

    Q_PROPERTY(bool lockScreen READ lockScreen WRITE setLockScreen NOTIFY lockScreenChanged FINAL)

public:
    explicit Helper(WServer *server);

    enum Switcher {
        Next,
        Previous,
    };
    Q_ENUM(Switcher)

    enum WallpaperType {
        Normal,
        Scale,
    };
    Q_ENUM(WallpaperType)

    enum MetaKeyCheck {
        ShortcutOverride = 0x1,
        KeyPress = 0x2,
        KeyRelease = 0x4,
    };

    void initProtocols(WOutputRenderWindow *window);
    WQuickOutputLayout *outputLayout() const;
    WSeat *seat() const;
    qw_compositor *compositor() const;
    TogglableGesture *multiTaskViewGesture() const;
    TogglableGesture *windowGesture() const;

    WCursor *cursor() const;

    WXdgDecorationManager *xdgDecorationManager() const;

    WQmlCreator *outputCreator() const;
    WQmlCreator *surfaceCreator() const;

    void setCurrentUser(const QString &currentUser);

    inline QString currentUser() const { return m_currentUser; }

    inline int currentWorkspaceId() const { return m_currentWorkspaceId; }

    void setCurrentWorkspaceId(int currentWorkspaceId);

    QString waylandSocket() const;
    QString xwaylandSocket() const;

    Q_INVOKABLE QString clientName(WAYLIB_SERVER_NAMESPACE::WSurface *surface) const;

    void stopMoveResize();

    WToplevelSurface *activatedSurface() const;
    WSurfaceItem *resizingItem() const;
    WSurfaceItem *movingItem() const;

    Q_INVOKABLE bool registerExclusiveZone(WLayerSurface *layerSurface);
    Q_INVOKABLE bool unregisterExclusiveZone(WLayerSurface *layerSurface);
    Q_INVOKABLE Margins getExclusiveMargins(WLayerSurface *layerSurface);
    Q_INVOKABLE quint32 getTopExclusiveMargin(WToplevelSurface *layerSurface);
    Q_INVOKABLE quint32 getBottomExclusiveMargin(WToplevelSurface *layerSurface);
    Q_INVOKABLE quint32 getLeftExclusiveMargin(WToplevelSurface *layerSurface);
    Q_INVOKABLE quint32 getRightExclusiveMargin(WToplevelSurface *layerSurface);

    // Output
    Q_INVOKABLE void onSurfaceEnterOutput(WToplevelSurface *surface,
                                          WSurfaceItem *surfaceItem,
                                          WOutput *output);
    Q_INVOKABLE void onSurfaceLeaveOutput(WToplevelSurface *surface,
                                          WSurfaceItem *surfaceItem,
                                          WOutput *output);
    Q_INVOKABLE Margins getOutputExclusiveMargins(WOutput *output);
    std::pair<WOutput *, OutputInfo *> getFirstOutputOfSurface(WToplevelSurface *surface);

    Q_INVOKABLE bool selectSurfaceToActivate(WToplevelSurface *surface) const;

    void setLockScreen(bool lockScreen);

    bool lockScreen() const { return m_isLockScreen; }

    Q_INVOKABLE void updateOutputsRegion();

    Q_INVOKABLE bool isLaunchpad(WLayerSurface *surface) const;

    WXWayland *createXWayland();
    void removeXWayland(WXWayland *xwayland);

public Q_SLOTS:
    void startMove(WToplevelSurface *surface, WSurfaceItem *shell, WSeat *seat, int serial);
    void startResize(
        WToplevelSurface *surface, WSurfaceItem *shell, WSeat *seat, Qt::Edges edge, int serial);
    void cancelMoveResize(WSurfaceItem *shell);
    void moveCursor(WSurfaceItem *shell, WSeat *seat);
    WSurface *getFocusSurfaceFrom(QObject *object);

    void allowNonDrmOutputAutoChangeMode(WOutput *output);
    void enableOutput(WOutput *output);

Q_SIGNALS:
    void activatedSurfaceChanged();
    void resizingItemChanged();
    void movingItemChanged();
    void greeterVisibleChanged();
    void topExclusiveMarginChanged();
    void bottomExclusiveMarginChanged();
    void leftExclusiveMarginChanged();
    void rightExclusiveMarginChanged();
    void socketFileChanged();
    void switcherChanged(Switcher action);
    void switcherActiveSwitch(Switcher action);
    void switcherOnChanged(bool on);
    void compositorChanged();
    void xdgDecorationManagerChanged();
    void currentWorkspaceIdChanged();
    void currentUserChanged(const QString &user);
    void lockScreenChanged();
    void metaKeyNotify();

protected:
    bool beforeDisposeEvent(WSeat *seat, QWindow *watched, QInputEvent *event) override;
    bool afterHandleEvent(WSeat *seat,
                          WSurface *watched,
                          QObject *surfaceItem,
                          QObject *,
                          QInputEvent *event) override;
    bool unacceptedEvent(WSeat *seat, QWindow *watched, QInputEvent *event) override;

    void setActivateSurface(WToplevelSurface *newActivate);
    void setResizingItem(WSurfaceItem *newResizingItem);
    void setMovingItem(WSurfaceItem *newMovingItem);
    void onOutputRequeseState(wlr_output_event_request_state *newState);
    OutputInfo *getOutputInfo(WOutput *output);

    WServer *m_server = nullptr;
    qw_renderer *m_renderer = nullptr;
    qw_allocator *m_allocator = nullptr;
    qw_compositor *m_compositor = nullptr;
    WQuickOutputLayout *m_outputLayout = nullptr;
    WCursor *m_cursor = nullptr;
    QPointer<WSeat> m_seat = nullptr;

    TogglableGesture *m_multiTaskViewGesture = nullptr;
    TogglableGesture *m_windowGesture = nullptr;

    WXdgDecorationManager *m_xdgDecorationManager;
    WXdgOutputManager *m_xdgOutputManager = nullptr;
    WXdgOutputManager *m_xwaylandOutputManager = nullptr;

    WForeignToplevel *m_foreignToplevel = nullptr;
    ForeignToplevelV1 *m_treelandForeignToplevel = nullptr;

    WQmlCreator *m_outputCreator = nullptr;
    WQmlCreator *m_surfaceCreator = nullptr;

    QPointer<WSocket> m_socket;

    QPointer<WToplevelSurface> m_activateSurface;

    QList<std::pair<WOutput *, OutputInfo *>> m_outputExclusiveZoneInfo;

    QRegion m_region;

    // for move resize
    struct
    {
        QPointer<WToplevelSurface> surface;
        QPointer<WSurfaceItem> surfaceItem;
        WSeat *seat = nullptr;
        QPointF cursorStartMovePosition;
        QPointF surfacePosOfStartMoveResize;
        QSizeF surfaceSizeOfStartMoveResize;
        Qt::Edges resizeEdgets;
        WSurfaceItem *resizingItem = nullptr;
        WSurfaceItem *movingItem = nullptr;
    } moveReiszeState;

private:
    void setWaylandSocket(const QString &socketFile);
    void setXWaylandSocket(const QString &socketFile);
    QVariant workspaceId(QQmlApplicationEngine *) const;
    bool doGesture(QInputEvent *event);

private:
    QString m_waylandSocket;
    QString m_xwaylandSocket;
    QString m_currentUser;
    bool m_switcherOn = false;
    bool m_switcherEnabled = true;

    bool m_isLockScreen{ false };

    int m_currentWorkspaceId{ 0 };
    QList<WXWayland *> m_xwaylands;
};

Q_DECLARE_FLAGS(MetaKeyChecks, Helper::MetaKeyCheck)
Q_DECLARE_OPERATORS_FOR_FLAGS(MetaKeyChecks)

struct OutputInfo
{
    QList<WToplevelSurface *> surfaceList;
    QList<WSurfaceItem *> surfaceItemList;

    // for Exclusive Zone
    Margins exclusiveMargins = { 0, 0, 0, 0 };
    QList<std::tuple<WLayerSurface *, uint32_t, WLayerSurface::AnchorType>> registeredSurfaceList;
};
