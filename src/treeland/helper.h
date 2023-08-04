#pragma once

#include <WXdgSurface>
#include <WSeat>
#include <WCursor>
#include <WSurfaceItem>
#include <WOutput>

struct wlr_output_event_request_state;
QW_USE_NAMESPACE
WAYLIB_SERVER_USE_NAMESPACE

class Helper : public WSeatEventFilter {
    Q_OBJECT
    Q_PROPERTY(WXdgSurface* activatedSurface READ activatedSurface WRITE setActivateSurface NOTIFY activatedSurfaceChanged FINAL)
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit Helper(QObject *parent = nullptr);
    void stop();

    WXdgSurface *activatedSurface() const;

public Q_SLOTS:
    void startMove(WXdgSurface *surface, QQuickItem *shell, QQuickItem *event, WSeat *seat, int serial);
    void startResize(WXdgSurface *surface, QQuickItem *shell, QQuickItem *event, WSeat *seat, Qt::Edges edge, int serial);
    bool startDemoClient(const QString &socket);
    WSurface *getFocusSurfaceFrom(QObject *object);

    void allowNonDrmOutputAutoChangeMode(WOutput *output);

signals:
    void activatedSurfaceChanged();

private:
    bool eventFilter(WSeat *seat, QWindow *watched, QInputEvent *event) override;
    bool eventFilter(WSeat *seat, WSurface *watched, QObject *surfaceItem, QInputEvent *event) override;
    bool ignoredEventFilter(WSeat *seat, QWindow *watched, QInputEvent *event) override;

    void setActivateSurface(WXdgSurface *newActivate);
    void onOutputRequeseState(wlr_output_event_request_state *newState);

    QPointer<WXdgSurface> m_activateSurface;

    // for move resize
    QPointer<WXdgSurface> surface;
    QPointer<QQuickItem> surfaceShellItem;
    QPointer<QQuickItem> eventItem;
    WSeat *seat = nullptr;
    QPointF surfacePosOfStartMoveResize;
    QSizeF surfaceSizeOfstartMoveResize;
    Qt::Edges resizeEdgets;
};
