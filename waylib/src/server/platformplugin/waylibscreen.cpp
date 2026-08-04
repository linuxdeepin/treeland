// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "waylibscreen.h"
#include "waylibcursor.h"
#include "waylibintegration.h"
#include "woutput.h"
#include "woutputlayout.h"
#include "wtools.h"

#include <qpa/qwindowsysteminterface.h>
#include <qpa/qwindowsysteminterface_p.h>
#include <private/qguiapplication_p.h>
#include <private/qhighdpiscaling_p.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

WaylibScreen::WaylibScreen(WOutput *output)
    : m_output(output)
{

}

WOutput *WaylibScreen::output() const
{
    return m_output.get();
}

QRect WaylibScreen::geometry() const
{
    return QRect(m_output->position(), m_output->transformedSize());
}

void WaylibScreen::move(const QPoint &pos)
{
    auto layout = m_output->layout();
    if (layout)
        layout->move(m_output, pos);
}

int WaylibScreen::depth() const
{
    return QImage::toPixelFormat(format()).bitsPerPixel();
}

QImage::Format WaylibScreen::format() const
{
    auto format = WTools::toImageFormat(m_output->handle()->render_format);
    if (format != QImage::Format_Invalid)
        return format;

    return QImage::Format_RGB32;
}

QSizeF WaylibScreen::physicalSize() const
{
    return QSizeF(handle()->phys_width, handle()->phys_height);
}

qreal WaylibScreen::devicePixelRatio() const
{
    return 1.0;
}

qreal WaylibScreen::refreshRate() const
{
    if (!handle()->current_mode)
        return 60;
    return handle()->current_mode->refresh / 1000.f;
}

QDpi WaylibScreen::logicalBaseDpi() const
{
    return QDpi(96, 96);
}

QDpi WaylibScreen::logicalDpi() const
{
    return logicalBaseDpi();
}

Qt::ScreenOrientation WaylibScreen::nativeOrientation() const
{
    return handle()->phys_width > handle()->phys_height ?
               Qt::LandscapeOrientation : Qt::PortraitOrientation;
}

Qt::ScreenOrientation WaylibScreen::orientation() const
{
    bool isPortrait = nativeOrientation() == Qt::PortraitOrientation;
    switch (handle()->transform) {
    case WL_OUTPUT_TRANSFORM_NORMAL:
        return isPortrait ? Qt::PortraitOrientation : Qt::LandscapeOrientation;
    case WL_OUTPUT_TRANSFORM_90:
        return isPortrait ? Qt::InvertedLandscapeOrientation : Qt::PortraitOrientation;
    case WL_OUTPUT_TRANSFORM_180:
        return isPortrait ? Qt::InvertedPortraitOrientation : Qt::InvertedLandscapeOrientation;
    case WL_OUTPUT_TRANSFORM_270:
        return isPortrait ? Qt::LandscapeOrientation : Qt::InvertedPortraitOrientation;
    default:
        break;
    }

    return Qt::PrimaryOrientation;
}

QWindow *WaylibScreen::topLevelAt(const QPoint &) const
{
    return nullptr;
}

QList<QPlatformScreen *> WaylibScreen::virtualSiblings() const
{
    QList<QPlatformScreen*> siblings;
    for (auto s : std::as_const(WaylibIntegration::instance()->m_screens)) {
        if (s != this)
            siblings.append(s);
    }

    return siblings;
}

QString WaylibScreen::name() const
{
    return QString::fromUtf8(handle()->name);
}

QString WaylibScreen::manufacturer() const
{
    return QString::fromUtf8(handle()->make);
}

QString WaylibScreen::model() const
{
    return QString::fromUtf8(handle()->model);
}

QString WaylibScreen::serialNumber() const
{
    return QString::fromUtf8(handle()->serial);
}

QPlatformCursor *WaylibScreen::cursor() const
{
    if (!m_cursor)
        m_cursor.reset(new WaylibCursor());
    return m_cursor.get();
}

QPlatformScreen::SubpixelAntialiasingType WaylibScreen::subpixelAntialiasingTypeHint() const
{
    switch (handle()->subpixel) {
    case WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB:
        return Subpixel_RGB;
    case WL_OUTPUT_SUBPIXEL_HORIZONTAL_BGR:
        return Subpixel_BGR;
    case WL_OUTPUT_SUBPIXEL_VERTICAL_RGB:
        return Subpixel_VRGB;
    case WL_OUTPUT_SUBPIXEL_VERTICAL_BGR:
        return Subpixel_VBGR;
    default:
        break;
    }

    return Subpixel_None;
}

QPlatformScreen::PowerState WaylibScreen::powerState() const
{
    return handle()->enabled ? PowerStateOn : PowerStateOff;
}

void WaylibScreen::setPowerState(PowerState)
{

}

QVector<QPlatformScreen::Mode> WaylibScreen::modes() const
{
    QVector<Mode> modes;
    struct wlr_output_mode *mode;
    wl_list_for_each(mode, &handle()->modes, link) {
        modes << Mode {QSize(mode->width, mode->height), static_cast<qreal>(mode->refresh)};
    }

    return modes;
}

int WaylibScreen::currentMode() const
{
    int index = 0;
    struct wlr_output_mode *current = handle()->current_mode;
    struct wlr_output_mode *mode;
    wl_list_for_each(mode, &handle()->modes, link) {
        if (current == mode)
            return index;
        ++index;
    }

    return 0;
}

int WaylibScreen::preferredMode() const
{
    int index = 0;
    struct wlr_output_mode *mode;
    wl_list_for_each(mode, &handle()->modes, link) {
        if (mode->preferred)
            return index;
        ++index;
    }

    return 0;
}

void WaylibScreen::initialize()
{
    auto updateGeometry = [this] {
        const QRect newGeo = geometry();
        QWindowSystemInterface::handleScreenGeometryChange(screen(), newGeo, newGeo);
    };

    m_output->safeConnect(&WOutput::transformedSizeChanged, screen(), updateGeometry);

    auto updateDpi = [this] {
        const auto dpi = logicalDpi();
        QWindowSystemInterface::handleScreenLogicalDotsPerInchChange(screen(), dpi.first, dpi.second);
    };

    m_output->safeConnect(&WOutput::scaleChanged, screen(), updateDpi);
    updateDpi();
}

wlr_output *WaylibScreen::handle() const
{
    return m_output->handle();
}

WAYLIB_SERVER_END_NAMESPACE
