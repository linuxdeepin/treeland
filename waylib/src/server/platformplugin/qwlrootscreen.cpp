// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwlrootscreen.h"
#include "qwlrootscursor.h"
#include "qwlrootsintegration.h"
#include "woutput.h"
#include "woutputlayout.h"
#include "wtools.h"

#include <qwoutput.h>

#include <qpa/qwindowsysteminterface.h>
#include <qpa/qwindowsysteminterface_p.h>
#include <private/qguiapplication_p.h>
#include <private/qhighdpiscaling_p.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

QWlrootsScreen::QWlrootsScreen(WOutput *output)
    : m_output(output)
{

}

WOutput *QWlrootsScreen::output() const
{
    return m_output.get();
}

QRect QWlrootsScreen::geometry() const
{
    return QRect(m_output->position(), m_output->transformedSize());
}

void QWlrootsScreen::move(const QPoint &pos)
{
    auto layout = m_output->layout();
    if (layout)
        layout->move(m_output, pos);
}

int QWlrootsScreen::depth() const
{
    return QImage::toPixelFormat(format()).bitsPerPixel();
}

QImage::Format QWlrootsScreen::format() const
{
    auto format = WTools::toImageFormat(m_output->nativeHandle()->render_format);
    if (format != QImage::Format_Invalid)
        return format;

    return QImage::Format_RGB32;
}

QSizeF QWlrootsScreen::physicalSize() const
{
    return QSizeF(handle()->phys_width, handle()->phys_height);
}

qreal QWlrootsScreen::devicePixelRatio() const
{
    return 1.0;
}

qreal QWlrootsScreen::refreshRate() const
{
    if (!handle()->current_mode)
        return 60;
    return handle()->current_mode->refresh / 1000.f;
}

QDpi QWlrootsScreen::logicalBaseDpi() const
{
    return QDpi(96, 96);
}

QDpi QWlrootsScreen::logicalDpi() const
{
    return logicalBaseDpi();
}

Qt::ScreenOrientation QWlrootsScreen::nativeOrientation() const
{
    return handle()->phys_width > handle()->phys_height ?
               Qt::LandscapeOrientation : Qt::PortraitOrientation;
}

Qt::ScreenOrientation QWlrootsScreen::orientation() const
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

QWindow *QWlrootsScreen::topLevelAt(const QPoint &) const
{
    return nullptr;
}

QList<QPlatformScreen *> QWlrootsScreen::virtualSiblings() const
{
    QList<QPlatformScreen*> siblings;
    for (auto s : std::as_const(QWlrootsIntegration::instance()->m_screens)) {
        if (s != this)
            siblings.append(s);
    }

    return siblings;
}

QString QWlrootsScreen::name() const
{
    return QString::fromUtf8(handle()->name);
}

QString QWlrootsScreen::manufacturer() const
{
    return QString::fromUtf8(handle()->make);
}

QString QWlrootsScreen::model() const
{
    return QString::fromUtf8(handle()->model);
}

QString QWlrootsScreen::serialNumber() const
{
    return QString::fromUtf8(handle()->serial);
}

QPlatformCursor *QWlrootsScreen::cursor() const
{
    if (!m_cursor)
        m_cursor.reset(new QWlrootsCursor());
    return m_cursor.get();
}

QPlatformScreen::SubpixelAntialiasingType QWlrootsScreen::subpixelAntialiasingTypeHint() const
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

QPlatformScreen::PowerState QWlrootsScreen::powerState() const
{
    return handle()->enabled ? PowerStateOn : PowerStateOff;
}

void QWlrootsScreen::setPowerState(PowerState)
{

}

QVector<QPlatformScreen::Mode> QWlrootsScreen::modes() const
{
    QVector<Mode> modes;
    struct wlr_output_mode *mode;
    wl_list_for_each(mode, &handle()->modes, link) {
        modes << Mode {QSize(mode->width, mode->height), static_cast<qreal>(mode->refresh)};
    }

    return modes;
}

int QWlrootsScreen::currentMode() const
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

int QWlrootsScreen::preferredMode() const
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

void QWlrootsScreen::initialize()
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

wlr_output *QWlrootsScreen::handle() const
{
    return m_output->handle()->handle();
}

WAYLIB_SERVER_END_NAMESPACE
