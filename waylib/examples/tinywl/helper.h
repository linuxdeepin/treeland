// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <WSeat>
#include <wxdgdecorationmanager.h>
#include <WCursor>
#include <WSurfaceItem>
#include <WOutput>
#include <WLayerSurface>
#include <wtoplevelsurface.h>
#include <wquickoutputlayout.h>
#include <wxwayland.h>
#include <winputmethodhelper.h>
#include <wsocket.h>
#include <wqmlcreator.h>

#include <QList>

Q_DECLARE_OPAQUE_POINTER(QWindow*)

WAYLIB_SERVER_BEGIN_NAMESPACE
class WQuickCursor;
class WOutputRenderWindow;
class WXdgOutputManager;
class WXdgDecorationManager;
class WInputMethodHelper;
class WCursorShapeManagerV1;
class WOutputManagerV1;
WAYLIB_SERVER_END_NAMESPACE

QW_BEGIN_NAMESPACE
class qw_compositor;
class qw_gamma_control_manager_v1;
class qw_fractional_scale_manager_v1;
QW_END_NAMESPACE

struct wlr_output_event_request_state;
QW_USE_NAMESPACE
WAYLIB_SERVER_USE_NAMESPACE

struct OutputInfo;

class Q_DECL_HIDDEN Helper : public WSeatEventFilter {
    Q_OBJECT
    Q_PROPERTY(WToplevelSurface* activatedSurface READ activatedSurface WRITE setActivateSurface NOTIFY activatedSurfaceChanged FINAL)
    Q_PROPERTY(WSurfaceItem* resizingItem READ resizingItem NOTIFY resizingItemChanged FINAL)
    Q_PROPERTY(WSurfaceItem* movingItem READ movingItem NOTIFY movingItemChanged)
    Q_PROPERTY(WQuickOutputLayout* outputLayout READ outputLayout CONSTANT)
    Q_PROPERTY(WSeat* seat READ seat CONSTANT)
    Q_PROPERTY(WXdgDecorationManager* xdgDecorationManager READ xdgDecorationManager NOTIFY xdgDecorationManagerChanged)
    Q_PROPERTY(QW_NAMESPACE::qw_compositor* compositor READ compositor NOTIFY compositorChanged FINAL)
    Q_PROPERTY(WQmlCreator* outputCreator READ outputCreator CONSTANT)
    Q_PROPERTY(WQmlCreator* xdgShellCreator READ xdgShellCreator CONSTANT)
    Q_PROPERTY(WQmlCreator* xwaylandCreator READ xwaylandCreator CONSTANT)
    Q_PROPERTY(WQmlCreator* layerShellCreator READ layerShellCreator CONSTANT)
    Q_PROPERTY(WQmlCreator* inputPopupCreator READ inputPopupCreator CONSTANT)
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit Helper(QObject *parent = nullptr);
    ~Helper() override;

    void initProtocols(WOutputRenderWindow *window, QQmlEngine *qmlEngine);
    WQuickOutputLayout *outputLayout() const;
    WSeat *seat() const;
    qw_compositor *compositor() const;

    WQmlCreator *outputCreator() const;
    WQmlCreator *xdgShellCreator() const;
    WQmlCreator *xwaylandCreator() const;
    WQmlCreator *layerShellCreator() const;
    WQmlCreator *inputPopupCreator() const;

    void stopMoveResize();

    WToplevelSurface *activatedSurface() const;
    WSurfaceItem *resizingItem() const;
    WSurfaceItem *movingItem() const;
    WXdgDecorationManager *xdgDecorationManager() const;

    Q_INVOKABLE bool registerExclusiveZone(WLayerSurface *layerSurface);
    Q_INVOKABLE bool unregisterExclusiveZone(WLayerSurface *layerSurface);
    Q_INVOKABLE QJSValue getExclusiveMargins(WLayerSurface *layerSurface);
    Q_INVOKABLE quint32 getTopExclusiveMargin(WToplevelSurface *layerSurface);
    Q_INVOKABLE quint32 getBottomExclusiveMargin(WToplevelSurface *layerSurface);
    Q_INVOKABLE quint32 getLeftExclusiveMargin(WToplevelSurface *layerSurface);
    Q_INVOKABLE quint32 getRightExclusiveMargin(WToplevelSurface *layerSurface);

    // Output
    Q_INVOKABLE void onSurfaceEnterOutput(WToplevelSurface *surface, WSurfaceItem *surfaceItem, WOutput *output);
    Q_INVOKABLE void onSurfaceLeaveOutput(WToplevelSurface *surface, WSurfaceItem *surfaceItem, WOutput *output);
    std::pair<WOutput*,OutputInfo*> getFirstOutputOfSurface(WToplevelSurface *surface);

    // Socket
    Q_INVOKABLE void setSocketEnabled(bool newEnabled);
public Q_SLOTS:
    void startMove(WToplevelSurface *surface, WSurfaceItem *shell, WSeat *seat, int serial);
    void startResize(WToplevelSurface *surface, WSurfaceItem *shell, WSeat *seat, Qt::Edges edge, int serial);
    void cancelMoveResize(WSurfaceItem *shell);
    bool startDemoClient(const QString &socket);
    WSurface *getFocusSurfaceFrom(QObject *object);

    void allowNonDrmOutputAutoChangeMode(WOutput *output);
    void enableOutput(WOutput *output);

Q_SIGNALS:
    void activatedSurfaceChanged();
    void resizingItemChanged();
    void movingItemChanged();
    void topExclusiveMarginChanged();
    void bottomExclusiveMarginChanged();
    void leftExclusiveMarginChanged();
    void rightExclusiveMarginChanged();
    void compositorChanged();
    void xdgDecorationManagerChanged();

    // 键盘信号
    void resizePane(int size, int direction);
    void choosePane(int id);
    void swapPane();
    void removePane();

private:
    bool beforeDisposeEvent(WSeat *seat, QWindow *watched, QInputEvent *event) override;
    bool afterHandleEvent(WSeat *seat, WSurface *watched, QObject *surfaceItem, QObject *, QInputEvent *event) override;
    bool unacceptedEvent(WSeat *seat, QWindow *watched, QInputEvent *event) override;

    void setActivateSurface(WToplevelSurface *newActivate);
    void setResizingItem(WSurfaceItem *newResizingItem);
    void setMovingItem(WSurfaceItem *newMovingItem);
    void onOutputRequeseState(wlr_output_event_request_state *newState);
    OutputInfo* getOutputInfo(WOutput *output);

    WQuickOutputLayout *m_outputLayout = nullptr;
    WCursor *m_cursor = nullptr;

    WServer *m_server = nullptr;
    qw_renderer *m_renderer = nullptr;
    qw_allocator *m_allocator = nullptr;
    qw_compositor *m_compositor = nullptr;
    WSeat *m_seat = nullptr;
    WXWayland *m_xwayland = nullptr;
    WXdgDecorationManager *m_xdgDecorationManager = nullptr;
    WInputMethodHelper *m_inputMethodHelper = nullptr;
    WSocket *m_socket = nullptr;
    qw_fractional_scale_manager_v1 *m_fractionalScaleManagerV1 = nullptr;
    WCursorShapeManagerV1 *m_cursorShapeManager = nullptr;
    qw_gamma_control_manager_v1 *m_gammaControlManager = nullptr;
    WOutputManagerV1 *m_wOutputManager = nullptr;

    WQmlCreator *m_outputCreator = nullptr;
    WQmlCreator *m_xdgShellCreator = nullptr;
    WQmlCreator *m_xwaylandCreator = nullptr;
    WQmlCreator *m_layerShellCreator = nullptr;
    WQmlCreator *m_inputPopupCreator = nullptr;

    QPointer<WToplevelSurface> m_activateSurface;
    QList<std::pair<WOutput*,OutputInfo*>> m_outputExclusiveZoneInfo;

    // for move resize
    struct {
        QPointer<WToplevelSurface> surface;
        QPointer<WSurfaceItem> surfaceItem;
        WSeat *seat = nullptr;
        QPointF surfacePosOfStartMoveResize;
        QSizeF surfaceSizeOfStartMoveResize;
        Qt::Edges resizeEdgets;
        WSurfaceItem *resizingItem = nullptr;
        WSurfaceItem *movingItem = nullptr;
    } moveReiszeState;
};

struct OutputInfo {
    QList<WToplevelSurface*> surfaceList;
    QList<WSurfaceItem*> surfaceItemList;

    // for Exclusive Zone
    quint32 m_topExclusiveMargin = 0;
    quint32 m_bottomExclusiveMargin = 0;
    quint32 m_leftExclusiveMargin = 0;
    quint32 m_rightExclusiveMargin = 0;
    QList<std::tuple<WLayerSurface*, uint32_t, WLayerSurface::AnchorType>> registeredSurfaceList;
};

Q_DECLARE_OPAQUE_POINTER(WAYLIB_SERVER_NAMESPACE::WOutputRenderWindow*)
Q_DECLARE_OPAQUE_POINTER(QW_NAMESPACE::qw_compositor*)
