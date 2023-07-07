// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wtools.h"

#include <qcolorspace.h>
#include <QDebug>

#include <wlr/util/box.h>
#include <pixman.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

inline QImage::Format toQImageFormat(pixman_format_code_t format, bool &sRGB) {
    sRGB = false;
    switch (static_cast<int>(format)) {
    /* 32bpp formats */
    case PIXMAN_a8r8g8b8:
        return QImage::Format_ARGB32_Premultiplied;
    case PIXMAN_x8r8g8b8:
        return QImage::Format_RGB32;
//        case PIXMAN_a8b8g8r8:
//        case PIXMAN_x8b8g8r8:
//        case PIXMAN_b8g8r8a8:
//        case PIXMAN_b8g8r8x8:
    case PIXMAN_r8g8b8a8:
        return QImage::Format_RGBA8888_Premultiplied;
    case PIXMAN_r8g8b8x8:
        return QImage::Format_RGBX8888;
    case PIXMAN_x2r10g10b10:
        return QImage::Format_RGB30;
    case PIXMAN_a2r10g10b10:
        return QImage::Format_A2RGB30_Premultiplied;
    case PIXMAN_x2b10g10r10:
        return QImage::Format_BGR30;
    case PIXMAN_a2b10g10r10:
        return QImage::Format_A2BGR30_Premultiplied;

    /* sRGB formats */
    case PIXMAN_a8r8g8b8_sRGB:
        sRGB = true;
        return QImage::Format_ARGB32_Premultiplied;

    /* 24bpp formats */
    case PIXMAN_r8g8b8:
        return QImage::Format_RGB888;
    case PIXMAN_b8g8r8:
        return QImage::Format_BGR888;

    /* 16bpp formats */
    case PIXMAN_r5g6b5:
        return QImage::Format_RGB16;
//        case PIXMAN_b5g6r5:

//        case PIXMAN_a1r5g5b5:
    case PIXMAN_x1r5g5b5:
        return QImage::Format_RGB555;
//        case PIXMAN_a1b5g5r5:
//        case PIXMAN_x1b5g5r5:
    case PIXMAN_a4r4g4b4:
        return QImage::Format_ARGB4444_Premultiplied;
    case PIXMAN_x4r4g4b4:
        return QImage::Format_RGB444;
//        case PIXMAN_a4b4g4r4:
//        case PIXMAN_x4b4g4r4:

    /* 8bpp formats */
    case PIXMAN_a8:
        return QImage::Format_Alpha8;
    case PIXMAN_c8:
        return QImage::Format_Indexed8;
    case PIXMAN_g8:
        return QImage::Format_Grayscale8;
    }

    return QImage::Format_Invalid;
}

QImage WTools::fromPixmanImage(void *image, void *data)
{
    auto pixman_image = reinterpret_cast<pixman_image_t*>(image);
    if (!data) data = pixman_image_get_data(pixman_image);
    uchar *image_data = reinterpret_cast<uchar*>(data);
    auto width = pixman_image_get_width(pixman_image);
    auto height = pixman_image_get_height(pixman_image);
    auto stride = pixman_image_get_stride(pixman_image);
    bool isSRGB = false;
    auto format = toQImageFormat(pixman_image_get_format(pixman_image), isSRGB);
    QImage qimage(image_data, width, height, stride, format);

    if (isSRGB)
        qimage.setColorSpace(QColorSpace::SRgb);

    return qimage;
}

QRegion WTools::fromPixmanRegion(void *region)
{
    Q_ASSERT(sizeof(QRect) == sizeof(pixman_box32));
    auto typedRegion = reinterpret_cast<pixman_region32_t*>(region);
    int rectCount = 0;
    auto rects = pixman_region32_rectangles(typedRegion, &rectCount);
    QRegion qregion;
    qregion.setRects(reinterpret_cast<QRect*>(rects), rectCount);
    return qregion;
}

void WTools::toPixmanRegion(const QRegion &region, void *pixmanRegion)
{
    Q_ASSERT(sizeof(QRect) == sizeof(pixman_box32));
    auto typedRegion = reinterpret_cast<pixman_region32_t*>(pixmanRegion);
    auto rects = reinterpret_cast<const pixman_box32_t*>(region.begin());
    bool ok = pixman_region32_init_rects(typedRegion, rects, region.rectCount());
    Q_ASSERT(ok);
}

QRect WTools::fromWLRBox(void *box)
{
    auto wlrBox = reinterpret_cast<wlr_box*>(box);
    return QRect(wlrBox->x, wlrBox->y, wlrBox->width, wlrBox->height);
}

void WTools::toWLRBox(const QRect &rect, void *box)
{
    auto wlrBox = reinterpret_cast<wlr_box*>(box);
    wlrBox->x = rect.x();
    wlrBox->y = rect.y();
    wlrBox->width = rect.width();
    wlrBox->height = rect.height();
}

WAYLIB_SERVER_END_NAMESPACE
