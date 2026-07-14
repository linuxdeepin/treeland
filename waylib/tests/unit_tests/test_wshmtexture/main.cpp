// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wtools.h"
#include "woutputlayer.h"
#include "woutputviewport.h"

#include <QQmlComponent>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QTest>

#include <drm_fourcc.h>

using namespace Waylib::Server;

namespace {
constexpr WGlobal::ColorContentsMode
resolveColorContentsMode(WGlobal::ColorContentsMode requested,
                         bool softwareRenderer) noexcept
{
    if (requested != WGlobal::ColorContentsMode::DontCare)
        return requested;
    return softwareRenderer ? WGlobal::ColorContentsMode::Preserve
                            : WGlobal::ColorContentsMode::Clear;
}
}

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


    void dontCareResolvesToClearForRhi()
    {
        QCOMPARE(resolveColorContentsMode(WGlobal::ColorContentsMode::DontCare, false),
                 WGlobal::ColorContentsMode::Clear);
    }

    void dontCareResolvesToPreserveForSoftware()
    {
        QCOMPARE(resolveColorContentsMode(WGlobal::ColorContentsMode::DontCare, true),
                 WGlobal::ColorContentsMode::Preserve);
    }

    void publicColorContentsModeProperties()
    {
        WOutputViewport viewport;
        QCOMPARE(viewport.colorContentsMode(), WGlobal::ColorContentsMode::DontCare);
        QSignalSpy viewportSpy(&viewport, &WOutputViewport::colorContentsModeChanged);
        viewport.setColorContentsMode(WGlobal::ColorContentsMode::Preserve);
        QCOMPARE(viewport.colorContentsMode(), WGlobal::ColorContentsMode::Preserve);
        QCOMPARE(viewportSpy.count(), 1);
        viewport.setColorContentsMode(WGlobal::ColorContentsMode::Preserve);
        QCOMPARE(viewportSpy.count(), 1);

        QQuickItem item;
        WOutputLayer layer(&item);
        QCOMPARE(layer.colorContentsMode(), WGlobal::ColorContentsMode::DontCare);
        QSignalSpy layerSpy(&layer, &WOutputLayer::colorContentsModeChanged);
        layer.setColorContentsMode(WGlobal::ColorContentsMode::Clear);
        QCOMPARE(layer.colorContentsMode(), WGlobal::ColorContentsMode::Clear);
        QCOMPARE(layerSpy.count(), 1);
        layer.setColorContentsMode(WGlobal::ColorContentsMode::Clear);
        QCOMPARE(layerSpy.count(), 1);
    }

    void qmlColorContentsModeNamespace()
    {
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(R"(
import QtQml
import Waylib.Server 1.0

QtObject {
    property int clearMode: ColorContentsMode.Clear
    property int preserveMode: ColorContentsMode.Preserve
}
)",
                          QUrl());
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> object(component.create());
        QVERIFY(object);
        QCOMPARE(object->property("clearMode").toInt(),
                 int(WGlobal::ColorContentsMode::Clear));
        QCOMPARE(object->property("preserveMode").toInt(),
                 int(WGlobal::ColorContentsMode::Preserve));
    }

    void rejectsUnsupportedFormat()
    {
        QCOMPARE(WTools::toImageFormat(DRM_FORMAT_R8),
                 QImage::Format_Invalid);
    }
};

QTEST_MAIN(WShmTextureTest)
#include "main.moc"
