// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef WTOOLS_H
#define WTOOLS_H

#include <wglobal.h>
#include <QImage>
#include <QRegion>
#include <QRect>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WTools
{
public:
    static QImage fromPixmanImage(void *image, void *data = nullptr);
    static QImage::Format toImageFormat(uint32_t drmFormat);
    static uint32_t toDrmFormat(QImage::Format format);
    static QImage::Format convertToDrmSupportedFormat(QImage::Format format);
    static QRegion fromPixmanRegion(void *region);
    static void toPixmanRegion(const QRegion &region, void *pixmanRegion);
    static QRect fromWLRBox(void *box);
    static void toWLRBox(const QRect &rect, void *box);
};

WAYLIB_SERVER_END_NAMESPACE

#endif // WTOOLS_H
