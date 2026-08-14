#include "snapproxy.h"

#include <any>
#include <unistd.h>

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QWindow>

#include <QtWaylandClient/private/qwaylandwindow_p.h>
#include <QtWaylandClient/private/qwaylandshellsurface_p.h>
#include <qlogging.h>

namespace {

QString processNameForPidfd(int pidfd)
{
    QFile fdinfo(QStringLiteral("/proc/self/fdinfo/%1").arg(pidfd));
    if (!fdinfo.open(QIODevice::ReadOnly))
        return {};

    qint64 pid = 0;
    const QByteArray content = fdinfo.readAll();
    const QList<QByteArray> lines = content.split('\n');
    for (const QByteArray &line : lines) {
        if (line.startsWith("Pid:")) {
            pid = line.mid(4).trimmed().toLongLong();
            break;
        }
    }
    if (pid <= 0)
        return {};

    QFile comm(QStringLiteral("/proc/%1/comm").arg(pid));
    if (!comm.open(QIODevice::ReadOnly))
        return {};

    return QString::fromLocal8Bit(comm.readAll()).trimmed();
}

} // namespace

ToplevelTagManager::ToplevelTagManager()
    : QWaylandClientExtensionTemplate<ToplevelTagManager>(1)
{
}

ToplevelTagManager::~ToplevelTagManager()
{
    if (isActive())
        destroy();
}

Snap::Snap()
    : QWaylandClientExtensionTemplate<Snap>(1)
{
}

Snap::~Snap()
{
    if (isActive())
        destroy();
}

void Snap::treeland_snap_target_v1_snap_region(int32_t x, int32_t y,
                                               uint32_t width, uint32_t height)
{
    emit snapRegion(static_cast<int>(x), static_cast<int>(y),
                    static_cast<quint32>(width), static_cast<quint32>(height));
}

void Snap::treeland_snap_target_v1_pidfd(int32_t pidfd)
{
    emit this->pidfd(static_cast<int>(pidfd));
}

void Snap::treeland_snap_target_v1_failed(uint32_t reason)
{
    emit failed(static_cast<quint32>(reason));
}

struct ::xdg_toplevel *getXdgToplevel(QWindow *window)
{
    if (!window || !window->handle())
        return nullptr;

    auto *ww = static_cast<QtWaylandClient::QWaylandWindow *>(window->handle());
    if (!ww)
        return nullptr;

    auto *ss = ww->shellSurface();
    if (!ss)
        return nullptr;

    std::any role = ss->surfaceRole();
    try {
        return std::any_cast<struct ::xdg_toplevel *>(role);
    } catch (const std::bad_any_cast &) {
    }
    return nullptr;
}

SnapController::SnapController(QObject *parent)
    : QObject(parent)
{
    connect(&m_tagManager, &QWaylandClientExtension::activeChanged,
            this, &SnapController::maybeInitialize);
    connect(&m_snap, &QWaylandClientExtension::activeChanged,
            this, &SnapController::maybeInitialize);
}

SnapController::~SnapController()
{
    clearPendingPidfd();
}

void SnapController::setBackgroundEnabled(bool enabled)
{
    if (m_backgroundEnabled == enabled)
        return;
    m_backgroundEnabled = enabled;
    emit backgroundEnabledChanged();
}

void SnapController::setCanvasWidth(int w)
{
    if (m_canvasWidth == w)
        return;
    m_canvasWidth = w;
    emit canvasSizeChanged();
}

void SnapController::setCanvasHeight(int h)
{
    if (m_canvasHeight == h)
        return;
    m_canvasHeight = h;
    emit canvasSizeChanged();
}

void SnapController::setCanvasX(int x)
{
    if (m_canvasX == x)
        return;
    m_canvasX = x;
    emit canvasSizeChanged();
}

void SnapController::setCanvasY(int y)
{
    if (m_canvasY == y)
        return;
    m_canvasY = y;
    emit canvasSizeChanged();
}

void SnapController::setup(QWindow *window)
{
    m_window = window;
    maybeInitialize();
}

void SnapController::maybeInitialize()
{
    if (m_initialized)
        return;
    if (!m_window || !m_window->handle())
        return;
    if (!m_tagManager.isActive() || !m_snap.isActive())
        return;

    m_initialized = true;

    auto *toplevel = getXdgToplevel(m_window);
    if (toplevel)
        m_tagManager.set_toplevel_tag(toplevel, "org.deepin.treeland.snap-mask");
    else
        qWarning() << "Could not obtain xdg_toplevel from window surface role";

    connect(&m_snap, &Snap::snapRegion, this, &SnapController::onSnapRegion);
    connect(&m_snap, &Snap::pidfd, this, &SnapController::onPidfd);
    connect(&m_snap, &Snap::failed, this, &SnapController::onFailed);
    m_snap.start(TREELAND_SNAP_TARGET_V1_EVENT_TYPE_PIDFD);
}

void SnapController::onPidfd(int pidfd)
{
    clearPendingPidfd();
    if (pidfd < 0)
        return;

    // The fd is owned by the connection and invalidated after the event is
    // dispatched, so keep our own descriptor until the committing
    // snap_region event arrives.
    m_pendingPidfd = ::dup(pidfd);
}

void SnapController::onSnapRegion(int x, int y, quint32 width, quint32 height)
{
    // snap_region commits the target: consume the pidfd received before it.
    if (m_pendingPidfd >= 0)
        setProcessName(processNameForPidfd(m_pendingPidfd));
    else
        setProcessName(QString());
    clearPendingPidfd();

    // The compositor reports the snap target in global scene coordinates;
    // translate it into this window's local space by subtracting the canvas
    // (union of all output logical geometries) origin.
    const int localX = x - m_canvasX;
    const int localY = y - m_canvasY;

    // Only remember the geometry of non-empty snap targets so that the
    // frozen selection / toolbar after confirmation still reflects the last
    // valid region the user hovered over. Empty regions (cursor over blank
    // desktop) just hide the live highlight without clobbering coordinates.
    if (width != 0 && height != 0) {
        m_x = localX;
        m_y = localY;
        m_w = width;
        m_h = height;
    }
    m_visible = (width != 0 && height != 0);
    emit snapRegionChanged();
}

void SnapController::onFailed(quint32 reason)
{
    const char *reasonStr = "unknown";
    switch (reason) {
    case QtWayland::treeland_snap_target_v1::failure_reason_snap_busy:
        reasonStr = "snap_busy";
        break;
    }
    qWarning() << "Snap failed:" << reasonStr;
    QCoreApplication::quit();
}

void SnapController::setProcessName(const QString &name)
{
    if (m_processName == name)
        return;
    m_processName = name;
    emit processNameChanged();
}

void SnapController::clearPendingPidfd()
{
    if (m_pendingPidfd < 0)
        return;
    ::close(m_pendingPidfd);
    m_pendingPidfd = -1;
}

void SnapController::confirmSelection()
{
    if (m_confirmed)
        return;
    m_confirmed = true;
    emit confirmedChanged();

    m_snap.stop();
}

void SnapController::quit()
{
    QCoreApplication::quit();
}
