// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wtools.h"

#include <qwbox.h>

extern "C" {
#include <wlr/util/edges.h>
}

#include <qcolorspace.h>
#include <QDebug>
#include <QQuickItem>

#include <pixman.h>
#include <drm_fourcc.h>

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
    default:
        break;
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

QImage::Format WTools::toImageFormat(uint32_t drmFormat)
{
    switch (drmFormat) {
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
    case DRM_FORMAT_ABGR8888:
        return QImage::Format_RGBA8888_Premultiplied;
    case DRM_FORMAT_XRGB2101010:
        return QImage::Format_RGB30;
    case DRM_FORMAT_BGRX1010102:
        return QImage::Format_BGR30;
    case DRM_FORMAT_ARGB2101010:
        return QImage::Format_A2RGB30_Premultiplied;
    case DRM_FORMAT_BGRA1010102:
        return QImage::Format_A2BGR30_Premultiplied;
    default: break;
    }

    return QImage::Format_Invalid;
}

uint32_t WTools::toDrmFormat(QImage::Format format)
{
    switch (format) {
    case QImage::Format_Indexed8:
        return DRM_FORMAT_C8;
    case QImage::Format_RGB444:
        return DRM_FORMAT_XRGB4444;
    case QImage::Format_ARGB4444_Premultiplied:
        return DRM_FORMAT_ARGB4444;
    case QImage::Format_RGB555:
        return DRM_FORMAT_XRGB1555;
    case QImage::Format_ARGB8555_Premultiplied:
        return DRM_FORMAT_ARGB1555;
    case QImage::Format_RGB16:
        return DRM_FORMAT_RGB565;
    case QImage::Format_RGB888:
        return DRM_FORMAT_RGB888;
    case QImage::Format_BGR888:
        return DRM_FORMAT_BGR888;
    case QImage::Format_RGB32:
        return DRM_FORMAT_XRGB8888;
    case QImage::Format_RGBX8888:
        return DRM_FORMAT_RGBX8888;
    case QImage::Format_ARGB32_Premultiplied:
        return DRM_FORMAT_ARGB8888;
    case QImage::Format_RGBA8888:
        return DRM_FORMAT_RGBA8888;
    case QImage::Format_RGBA8888_Premultiplied:
        return DRM_FORMAT_ABGR8888;
    case QImage::Format_RGB30:
        return DRM_FORMAT_XRGB2101010;
    case QImage::Format_BGR30:
        return DRM_FORMAT_BGRX1010102;
    case QImage::Format_A2RGB30_Premultiplied:
        return DRM_FORMAT_ARGB2101010;
    case QImage::Format_A2BGR30_Premultiplied:
        return DRM_FORMAT_BGRA1010102;
    default: break;
    }

    return DRM_FORMAT_INVALID;
}

QImage::Format WTools::convertToDrmSupportedFormat(QImage::Format format)
{
    switch (format) {
    case QImage::Format_ARGB32:
        return QImage::Format_ARGB32_Premultiplied;
    case QImage::Format_RGBA8888:
        return QImage::Format_RGBA8888_Premultiplied;
    case QImage::Format_RGBA64:
        return QImage::Format_RGBA64_Premultiplied;
    case QImage::Format_RGBA16FPx4:
        return QImage::Format_RGBA16FPx4_Premultiplied;
    case QImage::Format_RGBA32FPx4:
        return QImage::Format_RGBA32FPx4_Premultiplied;
    default:
        break;
    }

    return format;
}

uint32_t WTools::shmToDrmFormat(wl_shm_format_t shmFmt)
{
    switch (shmFmt) {
    case WL_SHM_FORMAT_XRGB8888:
        return DRM_FORMAT_XRGB8888;
    case WL_SHM_FORMAT_ARGB8888:
        return DRM_FORMAT_ARGB8888;
    default:
        return static_cast<uint32_t>(shmFmt);
    }
}

wl_shm_format_t WTools::drmToShmFormat(uint32_t drmFmt)
{
    switch (drmFmt) {
    case DRM_FORMAT_XRGB8888:
        return WL_SHM_FORMAT_XRGB8888;
    case DRM_FORMAT_ARGB8888:
        return WL_SHM_FORMAT_ARGB8888;
    default:
        return static_cast<wl_shm_format>(drmFmt);
    }
}

QRegion WTools::fromPixmanRegion(pixman_region32 *region)
{
    int rectCount = 0;
    auto rects = pixman_region32_rectangles(region, &rectCount);

    if (!rectCount)
        return {};

    QVector<QRect> rectList;
    rectList.resize(rectCount);

    for (int i = 0; i < rectCount; ++i)
        rectList[i].setCoords(rects[i].x1, rects[i].y1, rects[i].x2 - 1, rects[i].y2 - 1);

    QRegion qregion;
    qregion.setRects(rectList.constData(), rectList.count());
    Q_ASSERT(qregion.rectCount() == rectList.count());
    return qregion;
}

bool WTools::toPixmanRegion(const QRegion &region, pixman_region32 *pixmanRegion)
{
    QVector<pixman_box32_t> rects;
    rects.resize(region.rectCount());

    int i = 0;
    for (const QRect &r : std::as_const(region)) {
        pixman_box32_t &box = rects[i++];
        box.x1 = r.x();
        box.y1 = r.y();
        box.x2 = r.right() + 1;
        box.y2 = r.bottom() + 1;
    }
    bool ok = pixman_region32_init_rects(pixmanRegion, rects.constData(), rects.count());
    Q_ASSERT(!ok || pixman_region32_n_rects(pixmanRegion) == rects.count());
    return ok;
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

Qt::Edges WTools::toQtEdge(uint32_t edges)
{
    Qt::Edges qedges = Qt::Edges();

    if (edges & WLR_EDGE_TOP) {
        qedges |= Qt::TopEdge;
    }

    if (edges & WLR_EDGE_BOTTOM) {
        qedges |= Qt::BottomEdge;
    }

    if (edges & WLR_EDGE_LEFT) {
        qedges |= Qt::LeftEdge;
    }

    if (edges & WLR_EDGE_RIGHT) {
        qedges |= Qt::RightEdge;
    }

    return qedges;
}

WAYLIB_SERVER_END_NAMESPACE
