// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwlrootscreen.h"
#include "woutput.h"

#include <qwoutput.h>

extern "C" {
#include <wlr/types/wlr_output.h>
}

#include <drm_fourcc.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

QWlrootsScreen::QWlrootsScreen(WOutput *output)
    : m_output(output)
{
    init();
}

WOutput *QWlrootsScreen::output() const
{
    return m_output.get();
}

QRect QWlrootsScreen::geometry() const
{
    return QRect(m_output->position(), m_output->effectiveSize());
}

int QWlrootsScreen::depth() const
{
    return QImage::toPixelFormat(m_format).bitsPerPixel();
}

QImage::Format QWlrootsScreen::format() const
{
    return m_format;
}

QSizeF QWlrootsScreen::physicalSize() const
{
    return QSizeF(handle()->phys_width, handle()->phys_height);
}

qreal QWlrootsScreen::devicePixelRatio() const
{
    return handle()->scale;
}

qreal QWlrootsScreen::refreshRate() const
{
    if (!handle()->current_mode)
        return 60;
    return handle()->current_mode->refresh;
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
        return isPortrait ? Qt::PortraitOrientation : Qt::LandscapeOrientation;;
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
    return nullptr;
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

void QWlrootsScreen::init()
{
    m_format = getFormat();
}

QImage::Format QWlrootsScreen::getFormat() const
{
    auto f = m_output->nativeInterface<QWOutput>()->preferredReadFormat();

    switch (f) {
    case DRM_FORMAT_C8:
        return QImage::Format_Indexed8;
    case DRM_FORMAT_XRGB4444:
        return QImage::Format_RGB444;
    case DRM_FORMAT_ARGB4444:
        return QImage::Format_ARGB4444_Premultiplied;
    case DRM_FORMAT_XRGB1555:
        return QImage::Format_RGB555;
    case DRM_FORMAT_ARGB1555:
        return QImage::Format_ARGB8555_Premultiplied;
    case DRM_FORMAT_RGB565:
        return QImage::Format_RGB16;
    case DRM_FORMAT_RGB888:
        return QImage::Format_RGB888;
    case DRM_FORMAT_BGR888:
        return QImage::Format_BGR888;
    case DRM_FORMAT_XRGB8888:
        return QImage::Format_RGB32;
    case DRM_FORMAT_RGBX8888:
        return QImage::Format_RGBX8888;
    case DRM_FORMAT_ARGB8888:
        return QImage::Format_ARGB32_Premultiplied;
    case DRM_FORMAT_RGBA8888:
        return QImage::Format_RGBA8888;
    case DRM_FORMAT_XRGB2101010:
        return QImage::Format_RGB30;
    case DRM_FORMAT_BGRX1010102:
        return QImage::Format_BGR30;
    case DRM_FORMAT_ARGB2101010:
        return QImage::Format_A2RGB30_Premultiplied;
    case DRM_FORMAT_BGRA1010102:
        return QImage::Format_A2BGR30_Premultiplied;
    default:
        return QImage::Format_Invalid;
    }
}

wlr_output *QWlrootsScreen::handle() const
{
    return m_output->nativeInterface<QWOutput>()->handle();
}

WAYLIB_SERVER_END_NAMESPACE
