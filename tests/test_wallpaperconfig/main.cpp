// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wallpaper/wallpaperconfig.h"

#include <QObject>
#include <QTest>
#include <QJsonObject>
#include <QJsonArray>

class WallpaperConfigTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void testWorkspaceConfigImageRoundTrip()
    {
        WallpaperWorkspaceConfig original;
        original.workspaceId = 1;
        original.desktopWallpaper = "/usr/share/wallpapers/test.jpg";
        original.desktopWallpapertype = TreelandWallpaperInterfaceV1::Image;
        original.enable = true;

        QJsonObject json = original.toJson();
        QVERIFY(json.contains("workspaceIndex"));
        QVERIFY(json.contains("desktopWallpaper"));
        QVERIFY(json.contains("desktopWallpaperType"));
        QVERIFY(json.contains("enable"));
        QCOMPARE(json["workspaceIndex"].toInt(), 1);
        QCOMPARE(json["desktopWallpaper"].toString(), QString("/usr/share/wallpapers/test.jpg"));
        QCOMPARE(json["desktopWallpaperType"].toInt(), 0);
        QCOMPARE(json["enable"].toBool(), true);

        WallpaperWorkspaceConfig restored = WallpaperWorkspaceConfig::fromJson(json);

        QCOMPARE(restored.workspaceId, original.workspaceId);
        QCOMPARE(restored.desktopWallpaper, original.desktopWallpaper);
        QCOMPARE(restored.desktopWallpapertype, original.desktopWallpapertype);
        QCOMPARE(restored.enable, original.enable);
    }

    void testWorkspaceConfigVideoRoundTrip()
    {
        WallpaperWorkspaceConfig original;
        original.workspaceId = 2;
        original.desktopWallpaper = "/usr/share/wallpapers/test.mp4";
        original.desktopWallpapertype = TreelandWallpaperInterfaceV1::Video;
        original.enable = false;

        QJsonObject json = original.toJson();
        WallpaperWorkspaceConfig restored = WallpaperWorkspaceConfig::fromJson(json);

        QCOMPARE(restored.workspaceId, original.workspaceId);
        QCOMPARE(restored.desktopWallpaper, original.desktopWallpaper);
        QCOMPARE(restored.desktopWallpapertype, original.desktopWallpapertype);
        QCOMPARE(restored.enable, original.enable);
    }

    void testOutputConfigRoundTrip()
    {
        WallpaperOutputConfig original;
        original.outputName = "HDMI-1";
        original.lockscreenWallpaper = "/usr/share/lock/bg.jpg";
        original.lockScreenWallpapertype = TreelandWallpaperInterfaceV1::Image;
        original.enable = true;

        WallpaperWorkspaceConfig ws1;
        ws1.workspaceId = 1;
        ws1.desktopWallpaper = "/usr/share/wallpapers/ws1.jpg";
        ws1.desktopWallpapertype = TreelandWallpaperInterfaceV1::Image;
        ws1.enable = true;

        WallpaperWorkspaceConfig ws2;
        ws2.workspaceId = 2;
        ws2.desktopWallpaper = "/usr/share/wallpapers/ws2.mp4";
        ws2.desktopWallpapertype = TreelandWallpaperInterfaceV1::Video;
        ws2.enable = false;

        original.workspaces = {ws1, ws2};

        QJsonObject json = original.toJson();
        QVERIFY(json.contains("outputName"));
        QVERIFY(json.contains("lockScreenWallpaper"));
        QVERIFY(json.contains("lockScreenWallpaperType"));
        QVERIFY(json.contains("enable"));
        QVERIFY(json.contains("workspaces"));
        QCOMPARE(json["outputName"].toString(), QString("HDMI-1"));
        QCOMPARE(json["lockScreenWallpaper"].toString(), QString("/usr/share/lock/bg.jpg"));
        QCOMPARE(json["lockScreenWallpaperType"].toInt(), 0);
        QCOMPARE(json["enable"].toBool(), true);

        WallpaperOutputConfig restored = WallpaperOutputConfig::fromJson(json);

        QCOMPARE(restored.outputName, original.outputName);
        QCOMPARE(restored.lockscreenWallpaper, original.lockscreenWallpaper);
        QCOMPARE(restored.lockScreenWallpapertype, original.lockScreenWallpapertype);
        QCOMPARE(restored.enable, original.enable);
        QCOMPARE(restored.workspaces.size(), 2);
        QCOMPARE(restored.workspaces[0].workspaceId, 1);
        QCOMPARE(restored.workspaces[1].desktopWallpapertype, TreelandWallpaperInterfaceV1::Video);
    }

    void testContainsWorkspaceFound()
    {
        WallpaperOutputConfig config;
        WallpaperWorkspaceConfig ws;
        ws.workspaceId = 3;
        config.workspaces = {ws};
        QVERIFY(config.containsWorkspace(3));
    }

    void testContainsWorkspaceNotFound()
    {
        WallpaperOutputConfig config;
        WallpaperWorkspaceConfig ws;
        ws.workspaceId = 1;
        config.workspaces = {ws};
        QVERIFY(!config.containsWorkspace(99));
    }

    void testContainsWorkspaceEmpty()
    {
        WallpaperOutputConfig config;
        QVERIFY(!config.containsWorkspace(1));
    }
};

QTEST_MAIN(WallpaperConfigTest)
#include "main.moc"
