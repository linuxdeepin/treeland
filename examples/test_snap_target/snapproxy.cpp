#include "snapproxy.h"

#include <any>
#include <unistd.h>
#include <utility>

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QGuiApplication>
#include <QScreen>
#include <QWindow>

#include <qnativeinterface.h>
#include <wayland-client.h>

#include <QtWaylandClient/private/qwaylandwindow_p.h>
#include <QtWaylandClient/private/qwaylandshellsurface_p.h>
#include <qlogging.h>

namespace {

constexpr auto SnapMaskTag = "org.deepin.treeland.snap-mask";

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

void seatRegistryGlobal(void *data, wl_registry *registry, uint32_t name,
                        const char *interface, uint32_t version)
{
    auto **seat = static_cast<wl_seat **>(data);
    if (*seat || qstrcmp(interface, wl_seat_interface.name) != 0)
        return;
    *seat = static_cast<wl_seat *>(
        wl_registry_bind(registry, name, &wl_seat_interface, qMin<uint32_t>(version, 9)));
}

void seatRegistryGlobalRemove(void *data, wl_registry *registry, uint32_t name)
{
    Q_UNUSED(data);
    Q_UNUSED(registry);
    Q_UNUSED(name);
}

const wl_registry_listener seatRegistryListener = {
    seatRegistryGlobal,
    seatRegistryGlobalRemove,
};

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

SnapSession::SnapSession(QObject *parent)
    : QObject(parent)
{
    if (auto *waylandApp =
            qGuiApp ? qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>() : nullptr) {
        if (auto *display = waylandApp->display()) {
            m_seatRegistry = wl_display_get_registry(display);
            wl_registry_add_listener(m_seatRegistry, &seatRegistryListener, &m_seat);
            wl_display_roundtrip(display);
        }
    }

    connect(&m_tagManager, &QWaylandClientExtension::activeChanged,
            this, &SnapSession::maybeInitialize);
    connect(&m_snap, &QWaylandClientExtension::activeChanged,
            this, &SnapSession::maybeInitialize);
}

SnapSession::~SnapSession()
{
    clearPendingPidfd();
    if (m_seat) {
        wl_seat_destroy(m_seat);
        m_seat = nullptr;
    }
    if (m_seatRegistry) {
        wl_registry_destroy(m_seatRegistry);
        m_seatRegistry = nullptr;
    }
}

void SnapSession::registerWindow(QWindow *window)
{
    if (!window || m_windows.contains(window))
        return;
    m_windows.append(window);
    if (m_tagManager.isActive())
        tagWindow(window);
}

void SnapSession::start()
{
    m_startRequested = true;
    if (!m_initialized || m_started)
        return;
    m_started = true;
    m_snap.start(m_seat, TREELAND_SNAP_TARGET_V1_EVENT_TYPE_PIDFD);
}

void SnapSession::maybeInitialize()
{
    if (m_initialized)
        return;
    if (!m_tagManager.isActive() || !m_snap.isActive())
        return;

    m_initialized = true;

    for (const auto &window : std::as_const(m_windows)) {
        if (window)
            tagWindow(window);
    }

    connect(&m_snap, &Snap::snapRegion, this, &SnapSession::onSnapRegion);
    connect(&m_snap, &Snap::pidfd, this, &SnapSession::onPidfd);
    connect(&m_snap, &Snap::failed, this, &SnapSession::onFailed);

    if (m_startRequested)
        start();
}

void SnapSession::tagWindow(QWindow *window)
{
    auto *toplevel = getXdgToplevel(window);
    if (!toplevel) {
        qWarning() << "Could not obtain xdg_toplevel from window surface role";
        return;
    }
    m_tagManager.set_toplevel_tag(toplevel, SnapMaskTag);
}

void SnapSession::onPidfd(int pidfd)
{
    clearPendingPidfd();
    if (pidfd < 0)
        return;

    // The fd is owned by the connection and invalidated after the event is
    // dispatched, so keep our own descriptor until the committing
    // snap_region event arrives.
    m_pendingPidfd = ::dup(pidfd);
}

void SnapSession::onSnapRegion(int x, int y, quint32 width, quint32 height)
{
    // snap_region commits the target: consume the pidfd received before it.
    if (m_pendingPidfd >= 0)
        setProcessName(processNameForPidfd(m_pendingPidfd));
    else
        setProcessName(QString());
    clearPendingPidfd();

    if (width != 0 && height != 0)
        m_region = QRect(x, y, static_cast<int>(width), static_cast<int>(height));
    else
        m_region = QRect();

    Q_EMIT regionChanged();
}

void SnapSession::onFailed(quint32 reason)
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

void SnapSession::confirmSelection()
{
    if (m_confirmed)
        return;
    m_confirmed = true;
    Q_EMIT confirmedChanged();

    m_snap.stop();
}

void SnapSession::quit()
{
    QCoreApplication::quit();
}

void SnapSession::setProcessName(const QString &name)
{
    if (m_processName == name)
        return;
    m_processName = name;
    Q_EMIT processNameChanged();
}

void SnapSession::clearPendingPidfd()
{
    if (m_pendingPidfd < 0)
        return;
    ::close(m_pendingPidfd);
    m_pendingPidfd = -1;
}

SnapController::SnapController(SnapSession *session, QObject *parent)
    : QObject(parent)
    , m_session(session)
{
    connect(m_session, &SnapSession::regionChanged,
            this, &SnapController::onSessionRegionChanged);
    connect(m_session, &SnapSession::processNameChanged,
            this, &SnapController::onSessionProcessNameChanged);
    connect(m_session, &SnapSession::confirmedChanged,
            this, &SnapController::onSessionConfirmedChanged);
}

SnapController::~SnapController() = default;

void SnapController::setup(QWindow *window, QScreen *screen)
{
    m_window = window;
    m_screen = screen;

    if (m_window) {
        if (m_screen) {
            m_window->setScreen(m_screen);
            m_window->setVisibility(QWindow::FullScreen);
        }
        m_session->registerWindow(m_window);
    }

    Q_EMIT screenGeometryChanged();
}

void SnapController::confirmSelection()
{
    m_session->confirmSelection();
}

void SnapController::quit()
{
    m_session->quit();
}

QString SnapController::processName() const
{
    return m_session->processName();
}

bool SnapController::confirmed() const
{
    return m_session->confirmed();
}

void SnapController::setBackgroundEnabled(bool enabled)
{
    if (m_backgroundEnabled == enabled)
        return;
    m_backgroundEnabled = enabled;
    Q_EMIT backgroundEnabledChanged();
}

void SnapController::setBackgroundSource(const QString &source)
{
    if (m_backgroundSource == source)
        return;
    m_backgroundSource = source;
    Q_EMIT backgroundSourceChanged();
}

int SnapController::screenX() const
{
    return m_screen ? m_screen->geometry().x() : 0;
}

int SnapController::screenY() const
{
    return m_screen ? m_screen->geometry().y() : 0;
}

int SnapController::screenWidth() const
{
    return m_screen ? m_screen->geometry().width() : 0;
}

int SnapController::screenHeight() const
{
    return m_screen ? m_screen->geometry().height() : 0;
}

void SnapController::onSessionRegionChanged()
{
    const QRect global = m_session->region();
    const QPoint origin = m_screen ? m_screen->geometry().topLeft() : QPoint(0, 0);

    // Only remember the geometry of non-empty snap targets so that the
    // frozen selection / toolbar after confirmation still reflects the last
    // valid region the user hovered over.
    if (global.isValid() && !global.isEmpty()) {
        m_x = global.x() - origin.x();
        m_y = global.y() - origin.y();
        m_w = global.width();
        m_h = global.height();
    }
    m_visible = global.isValid() && !global.isEmpty();

    Q_EMIT snapRegionChanged();
}

void SnapController::onSessionProcessNameChanged()
{
    Q_EMIT processNameChanged();
}

void SnapController::onSessionConfirmedChanged()
{
    Q_EMIT confirmedChanged();
}
