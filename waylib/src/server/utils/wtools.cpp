/*
 * Copyright (C) 2021 zkyd
 *
 * Author:     zkyd <zkyd@zjide.org>
 *
 * Maintainer: zkyd <zkyd@zjide.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "wtools.h"

#include <qcolorspace.h>
#include <QDebug>

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
    case PIXMAN_x8b8g8r8:
        return QImage::Format_BGR888;
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

WAYLIB_SERVER_END_NAMESPACE
