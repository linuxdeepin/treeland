#ifndef SNAPPROXY_H
#define SNAPPROXY_H

#include <QList>
#include <QObject>
#include <QPointer>
#include <QRect>
#include <QString>
#include <QWaylandClientExtension>

#include "qwayland-treeland-snap-target-unstable-v1.h"
#include "qwayland-xdg-toplevel-tag-v1.h"
#include "qwayland-xdg-shell.h"

class QScreen;
class QWindow;

struct wl_registry;
struct wl_seat;

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
    , public QtWayland::treeland_snap_target_v1
{
    Q_OBJECT
public:
    Snap();
    ~Snap() override;

signals:
    void snapRegion(int x, int y, quint32 width, quint32 height);
    void pidfd(int pidfd);
    void failed(quint32 reason);

protected:
    void treeland_snap_target_v1_snap_region(int32_t x, int32_t y,
                                             uint32_t width, uint32_t height) override;
    void treeland_snap_target_v1_pidfd(int32_t pidfd) override;
    void treeland_snap_target_v1_failed(uint32_t reason) override;
};

struct ::xdg_toplevel *getXdgToplevel(QWindow *window);

// Process-wide snap session. There is a single treeland_snap_target_v1 object
// per client, shared by every per-output mask window.
class SnapSession : public QObject
{
    Q_OBJECT
public:
    explicit SnapSession(QObject *parent = nullptr);
    ~SnapSession() override;

    void registerWindow(QWindow *window);
    void start();

    QRect region() const { return m_region; }
    QString processName() const { return m_processName; }
    bool confirmed() const { return m_confirmed; }

    void confirmSelection();
    void quit();

signals:
    void regionChanged();
    void processNameChanged();
    void confirmedChanged();

private:
    void maybeInitialize();
    void tagWindow(QWindow *window);
    void onSnapRegion(int x, int y, quint32 width, quint32 height);
    void onPidfd(int pidfd);
    void onFailed(quint32 reason);
    void setProcessName(const QString &name);
    void clearPendingPidfd();

    ToplevelTagManager m_tagManager;
    Snap m_snap;

    wl_registry *m_seatRegistry = nullptr;
    wl_seat *m_seat = nullptr;

    QList<QPointer<QWindow>> m_windows;
    bool m_initialized = false;
    bool m_startRequested = false;
    bool m_started = false;
    bool m_confirmed = false;
    QRect m_region;
    QString m_processName;
    int m_pendingPidfd = -1;
};

// Per-output mask window controller, exposed to QML as `snapController`.
class SnapController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int snapX READ snapX NOTIFY snapRegionChanged)
    Q_PROPERTY(int snapY READ snapY NOTIFY snapRegionChanged)
    Q_PROPERTY(int snapWidth READ snapWidth NOTIFY snapRegionChanged)
    Q_PROPERTY(int snapHeight READ snapHeight NOTIFY snapRegionChanged)
    Q_PROPERTY(bool snapVisible READ snapVisible NOTIFY snapRegionChanged)
    Q_PROPERTY(QString processName READ processName NOTIFY processNameChanged)
    Q_PROPERTY(bool confirmed READ confirmed NOTIFY confirmedChanged)
    Q_PROPERTY(bool backgroundEnabled READ backgroundEnabled WRITE setBackgroundEnabled
                   NOTIFY backgroundEnabledChanged)
    Q_PROPERTY(QString backgroundSource READ backgroundSource WRITE setBackgroundSource
                   NOTIFY backgroundSourceChanged)
    Q_PROPERTY(int screenX READ screenX NOTIFY screenGeometryChanged)
    Q_PROPERTY(int screenY READ screenY NOTIFY screenGeometryChanged)
    Q_PROPERTY(int screenWidth READ screenWidth NOTIFY screenGeometryChanged)
    Q_PROPERTY(int screenHeight READ screenHeight NOTIFY screenGeometryChanged)

public:
    explicit SnapController(SnapSession *session, QObject *parent = nullptr);
    ~SnapController() override;

    void setup(QWindow *window, QScreen *screen);
    Q_INVOKABLE void confirmSelection();
    Q_INVOKABLE void quit();

    int snapX() const { return m_x; }
    int snapY() const { return m_y; }
    int snapWidth() const { return m_w; }
    int snapHeight() const { return m_h; }
    bool snapVisible() const { return m_visible; }
    QString processName() const;
    bool confirmed() const;
    bool backgroundEnabled() const { return m_backgroundEnabled; }
    void setBackgroundEnabled(bool enabled);
    QString backgroundSource() const { return m_backgroundSource; }
    void setBackgroundSource(const QString &source);
    int screenX() const;
    int screenY() const;
    int screenWidth() const;
    int screenHeight() const;

signals:
    void snapRegionChanged();
    void processNameChanged();
    void confirmedChanged();
    void backgroundEnabledChanged();
    void backgroundSourceChanged();
    void screenGeometryChanged();

private:
    void onSessionRegionChanged();
    void onSessionProcessNameChanged();
    void onSessionConfirmedChanged();

    SnapSession *m_session = nullptr;
    QPointer<QWindow> m_window;
    QPointer<QScreen> m_screen;

    int m_x = 0;
    int m_y = 0;
    int m_w = 0;
    int m_h = 0;
    bool m_visible = false;
    bool m_backgroundEnabled = false;
    QString m_backgroundSource;
};

#endif // SNAPPROXY_H
