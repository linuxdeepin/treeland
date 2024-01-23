// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <WSeat>
#include <WCursor>
#include <WSurfaceItem>
#include <WOutput>
#include <WLayerSurface>
#include <wtoplevelsurface.h>

#include <QList>

Q_DECLARE_OPAQUE_POINTER(QWindow*)

struct wlr_output_event_request_state;
QW_USE_NAMESPACE
WAYLIB_SERVER_USE_NAMESPACE

struct OutputInfo;

class Helper : public WSeatEventFilter {
    Q_OBJECT
    Q_PROPERTY(WToplevelSurface* activatedSurface READ activatedSurface WRITE setActivateSurface NOTIFY activatedSurfaceChanged FINAL)
    Q_PROPERTY(WSurfaceItem* resizingItem READ resizingItem NOTIFY resizingItemChanged FINAL)
    Q_PROPERTY(WSurfaceItem *movingItem READ movingItem NOTIFY movingItemChanged FINAL)
    Q_PROPERTY(QString socketFile READ socketFile WRITE setSocketFile NOTIFY socketFileChanged FINAL)
    Q_PROPERTY(QString currentUser WRITE setCurrentUser FINAL)
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit Helper(QObject *parent = nullptr);

    enum Switcher {
        Hide,
        Show,
        Next,
        Previous,
    };
    Q_ENUM(Switcher)

    void setCurrentUser(const QString &currentUser);

    QString socketFile() const;

    Q_INVOKABLE QString clientName(Waylib::Server::WSurface *surface) const;

    bool addAction(const QString &user, QAction *action);
    void removeAction(const QString &user, QAction *action);

    Q_INVOKABLE void closeSurface(Waylib::Server::WSurface *surface);

    void stopMoveResize();

    WToplevelSurface *activatedSurface() const;
    WSurfaceItem *resizingItem() const;
    WSurfaceItem *movingItem() const;

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

public Q_SLOTS:
    void startMove(WToplevelSurface *surface, WSurfaceItem *shell, WSeat *seat, int serial);
    void startResize(WToplevelSurface *surface, WSurfaceItem *shell, WSeat *seat, Qt::Edges edge, int serial);
    void cancelMoveResize(WSurfaceItem *shell);
    WSurface *getFocusSurfaceFrom(QObject *object);

    void allowNonDrmOutputAutoChangeMode(WOutput *output);

Q_SIGNALS:
    void activatedSurfaceChanged();
    void resizingItemChanged();
    void movingItemChanged();
    void backToNormal();
    void reboot();
    void greeterVisibleChanged();
    void topExclusiveMarginChanged();
    void bottomExclusiveMarginChanged();
    void leftExclusiveMarginChanged();
    void rightExclusiveMarginChanged();
    void socketFileChanged();
    void switcherChanged(Switcher mode);

protected:
    bool beforeDisposeEvent(WSeat *seat, QWindow *watched, QInputEvent *event) override;
    bool afterHandleEvent(WSeat *seat, WSurface *watched, QObject *surfaceItem, QObject *, QInputEvent *event) override;
    bool unacceptedEvent(WSeat *seat, QWindow *watched, QInputEvent *event) override;

    void setActivateSurface(WToplevelSurface *newActivate);
    void setResizingItem(WSurfaceItem *newResizingItem);
    void setMovingItem(WSurfaceItem *newMovingItem);
    void onOutputRequeseState(wlr_output_event_request_state *newState);
    OutputInfo* getOutputInfo(WOutput *output);

    QPointer<WToplevelSurface> m_activateSurface;

    // for move resize
    QPointer<WToplevelSurface> surface;
    QPointer<WSurfaceItem> surfaceItem;
    WSeat *seat = nullptr;
    QPointF surfacePosOfStartMoveResize;
    QSizeF surfaceSizeOfStartMoveResize;
    Qt::Edges resizeEdgets;
    WSurfaceItem *m_resizingItem = nullptr;
    WSurfaceItem *m_movingItem = nullptr;
    QList<std::pair<WOutput*,OutputInfo*>> m_outputExclusiveZoneInfo;

private:
    void setSocketFile(const QString &socketFile);

private:
    QString m_socketFile;
    QString m_currentUser;
    Switcher m_switcherCurrentMode = Switcher::Hide;
    std::map<QString, std::vector<QAction*>> m_actions;
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
