// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wtools.h"

#include <QTest>

#include <drm_fourcc.h>

using namespace Waylib::Server;

class WShmTextureTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void mapsArgb8888ToPremultiplied()
    {
        QCOMPARE(WTools::toImageFormat(DRM_FORMAT_ARGB8888),
                 QImage::Format_ARGB32_Premultiplied);
    }

    void mapsRgba8888ToPremultipliedForShm()
    {
        QCOMPARE(WTools::convertToDrmSupportedFormat(
                     WTools::toImageFormat(DRM_FORMAT_RGBA8888)),
                 QImage::Format_RGBA8888_Premultiplied);
    }

    void mapsBgraAndBgrx8888()
    {
        QCOMPARE(WTools::convertToDrmSupportedFormat(
                     WTools::toImageFormat(DRM_FORMAT_BGRA8888)),
                 QImage::Format_ARGB32_Premultiplied);
        QCOMPARE(WTools::toImageFormat(DRM_FORMAT_BGRX8888),
                 QImage::Format_RGB32);
    }

    void mapsRgbx8888()
    {
        QCOMPARE(WTools::toImageFormat(DRM_FORMAT_RGBX8888),
                 QImage::Format_RGBX8888);
    }

    void rejectsUnsupportedFormat()
    {
        QCOMPARE(WTools::toImageFormat(DRM_FORMAT_R8),
                 QImage::Format_Invalid);
    }
};

QTEST_MAIN(WShmTextureTest)
#include "main.moc"
