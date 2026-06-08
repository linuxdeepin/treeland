// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef WTOOLS_H
#define WTOOLS_H

#include <wglobal.h>
#include <QImage>
#include <QRegion>
#include <QRect>

struct pixman_region32;
typedef int wl_shm_format_t;

QT_BEGIN_NAMESPACE
class QQuickItem;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WAYLIB_SERVER_EXPORT WTools
{
public:
    static QImage fromPixmanImage(void *image, void *data = nullptr);
    static QImage::Format toImageFormat(uint32_t drmFormat);
    static uint32_t toDrmFormat(QImage::Format format);
    static QImage::Format convertToDrmSupportedFormat(QImage::Format format);
    static uint32_t shmToDrmFormat(wl_shm_format_t shmFmt);
    static wl_shm_format_t drmToShmFormat(uint32_t drmFmt);
    static QRegion fromPixmanRegion(pixman_region32 *region);
    static bool toPixmanRegion(const QRegion &region, pixman_region32 *pixmanRegion);
    static QRect fromWLRBox(void *box);
    static void toWLRBox(const QRect &rect, void *box);
    static Qt::Edges toQtEdge(uint32_t edges);
};

WAYLIB_SERVER_END_NAMESPACE

#endif // WTOOLS_H
