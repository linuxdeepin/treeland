#ifndef SNAPPROXY_H
#define SNAPPROXY_H

#include <QObject>
#include <QPointer>
#include <QWaylandClientExtension>

#include "qwayland-treeland-capture-snap-unstable-v1.h"
#include "qwayland-xdg-toplevel-tag-v1.h"
#include "qwayland-xdg-shell.h"

class QWindow;

class ToplevelTagManager
    : public QWaylandClientExtensionTemplate<ToplevelTagManager>
    , public QtWayland::xdg_toplevel_tag_manager_v1
{
    Q_OBJECT
public:
    ToplevelTagManager();
    ~ToplevelTagManager() override;
};

class Snap
    : public QWaylandClientExtensionTemplate<Snap>
    , public QtWayland::treeland_capture_snap_v1
{
    Q_OBJECT
public:
    Snap();
    ~Snap() override;

signals:
    void snapRegion(int x, int y, quint32 width, quint32 height);
    void failed(quint32 reason);

protected:
    void treeland_capture_snap_v1_snap_region(int32_t x, int32_t y,
                                               uint32_t width, uint32_t height) override;
    void treeland_capture_snap_v1_failed(uint32_t reason) override;
};

struct ::xdg_toplevel *getXdgToplevel(QWindow *window);

class SnapController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int snapX READ snapX NOTIFY snapRegionChanged)
    Q_PROPERTY(int snapY READ snapY NOTIFY snapRegionChanged)
    Q_PROPERTY(int snapWidth READ snapWidth NOTIFY snapRegionChanged)
    Q_PROPERTY(int snapHeight READ snapHeight NOTIFY snapRegionChanged)
    Q_PROPERTY(bool snapVisible READ snapVisible NOTIFY snapRegionChanged)
    Q_PROPERTY(bool confirmed READ confirmed NOTIFY confirmedChanged)
    Q_PROPERTY(bool backgroundEnabled READ backgroundEnabled WRITE setBackgroundEnabled
                   NOTIFY backgroundEnabledChanged)
    Q_PROPERTY(int canvasX READ canvasX WRITE setCanvasX NOTIFY canvasSizeChanged)
    Q_PROPERTY(int canvasY READ canvasY WRITE setCanvasY NOTIFY canvasSizeChanged)
    Q_PROPERTY(int canvasWidth READ canvasWidth WRITE setCanvasWidth NOTIFY canvasSizeChanged)
    Q_PROPERTY(int canvasHeight READ canvasHeight WRITE setCanvasHeight NOTIFY canvasSizeChanged)

public:
    explicit SnapController(QObject *parent = nullptr);

    void setup(QWindow *window);
    Q_INVOKABLE void confirmSelection();
    Q_INVOKABLE void quit();

    int snapX() const { return m_x; }
    int snapY() const { return m_y; }
    int snapWidth() const { return static_cast<int>(m_w); }
    int snapHeight() const { return static_cast<int>(m_h); }
    bool snapVisible() const { return m_visible; }
    bool confirmed() const { return m_confirmed; }
    bool backgroundEnabled() const { return m_backgroundEnabled; }
    void setBackgroundEnabled(bool enabled);
    int canvasX() const { return m_canvasX; }
    void setCanvasX(int x);
    int canvasY() const { return m_canvasY; }
    void setCanvasY(int y);
    int canvasWidth() const { return m_canvasWidth; }
    void setCanvasWidth(int w);
    int canvasHeight() const { return m_canvasHeight; }
    void setCanvasHeight(int h);
signals:
    void snapRegionChanged();
    void confirmedChanged();
    void backgroundEnabledChanged();
    void canvasSizeChanged();

private:
    void maybeInitialize();
    void onSnapRegion(int x, int y, quint32 width, quint32 height);
    void onFailed(quint32 reason);

    ToplevelTagManager m_tagManager;
    Snap m_snap;
    QPointer<QWindow> m_window;

    int m_x = 0;
    int m_y = 0;
    quint32 m_w = 0;
    quint32 m_h = 0;
    bool m_visible = false;
    bool m_initialized = false;
    bool m_confirmed = false;
    bool m_backgroundEnabled = false;
    int m_canvasX = 0;
    int m_canvasY = 0;
    int m_canvasWidth = 0;
    int m_canvasHeight = 0;
};

#endif // SNAPPROXY_H
