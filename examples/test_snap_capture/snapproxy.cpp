#include "snapproxy.h"

#include <any>
#include <QCoreApplication>
#include <QDebug>
#include <QWindow>

#include <QtWaylandClient/private/qwaylandwindow_p.h>
#include <QtWaylandClient/private/qwaylandshellsurface_p.h>

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

void Snap::treeland_capture_snap_v1_snap_region(int32_t x, int32_t y,
                                                 uint32_t width, uint32_t height)
{
    emit snapRegion(static_cast<int>(x), static_cast<int>(y),
                    static_cast<quint32>(width), static_cast<quint32>(height));
}

void Snap::treeland_capture_snap_v1_failed(uint32_t reason)
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
        m_tagManager.set_toplevel_tag(toplevel, "org.deepin.treeland.capture-mask");
    else
        qWarning() << "Could not obtain xdg_toplevel from window surface role";

    connect(&m_snap, &Snap::snapRegion, this, &SnapController::onSnapRegion);
    connect(&m_snap, &Snap::failed, this, &SnapController::onFailed);
    m_snap.start();
}

void SnapController::onSnapRegion(int x, int y, quint32 width, quint32 height)
{
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
    case QtWayland::treeland_capture_snap_v1::failure_reason_snap_busy:
        reasonStr = "snap_busy";
        break;
    }
    qWarning() << "Snap failed:" << reasonStr;
    QCoreApplication::quit();
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
