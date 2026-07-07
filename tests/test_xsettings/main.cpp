// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QTest>
#include <QByteArray>
#include <QColor>
#include <QVariant>

#include "xsettings/xsettings.h"

class TestableXSettings : public XSettings
{
public:
    explicit TestableXSettings(QObject *parent = nullptr) : XSettings(parent) {}
    using XSettings::depopulateSettings;
    using XSettings::populateSettings;
};

class TestXSettings : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testRoundTrip_Integer();
    void testRoundTrip_String();
    void testRoundTrip_Color();
    void testRoundTrip_Multiple();
    void testRoundTrip_Empty();
    void testPropertyValueUpdate();
    void testToByteArray();
};

void TestXSettings::testRoundTrip_Integer()
{
    TestableXSettings settings1(static_cast<QObject*>(nullptr));
    settings1.setPropertyValue("Net/CursorBlinkTime", 1000);
    
    QByteArray data = settings1.depopulateSettings();
    QVERIFY(!data.isEmpty());
    
    TestableXSettings settings2(static_cast<QObject*>(nullptr));
    settings2.populateSettings(data);
    
    QVERIFY(settings2.contains("Net/CursorBlinkTime"));
    QCOMPARE(settings2.getPropertyValue("Net/CursorBlinkTime").toInt(), 1000);
}

void TestXSettings::testRoundTrip_String()
{
    TestableXSettings settings1(static_cast<QObject*>(nullptr));
    settings1.setPropertyValue("Gtk/FontName", QByteArray("Noto Sans 10"));
    
    QByteArray data = settings1.depopulateSettings();
    QVERIFY(!data.isEmpty());
    
    TestableXSettings settings2(static_cast<QObject*>(nullptr));
    settings2.populateSettings(data);
    
    QVERIFY(settings2.contains("Gtk/FontName"));
    QCOMPARE(settings2.getPropertyValue("Gtk/FontName").toByteArray(), QByteArray("Noto Sans 10"));
}

void TestXSettings::testRoundTrip_Color()
{
    TestableXSettings settings1(static_cast<QObject*>(nullptr));
    QColor color(255, 128, 64, 200);
    settings1.setPropertyValue("Gtk/ColorPalette", color);
    
    QByteArray data = settings1.depopulateSettings();
    QVERIFY(!data.isEmpty());
    
    TestableXSettings settings2(static_cast<QObject*>(nullptr));
    settings2.populateSettings(data);
    
    QVERIFY(settings2.contains("Gtk/ColorPalette"));
    QColor result = qvariant_cast<QColor>(settings2.getPropertyValue("Gtk/ColorPalette"));
    QCOMPARE(result.red(), 255);
    QCOMPARE(result.green(), 128);
    QCOMPARE(result.blue(), 64);
    QCOMPARE(result.alpha(), 200);
}

void TestXSettings::testRoundTrip_Multiple()
{
    TestableXSettings settings1(static_cast<QObject*>(nullptr));
    settings1.setPropertyValue("Net/CursorBlinkTime", 500);
    settings1.setPropertyValue("Gtk/FontName", QByteArray("DejaVu Sans 12"));
    settings1.setPropertyValue("Gtk/EnableAnimations", 1);
    
    QByteArray data = settings1.depopulateSettings();
    QVERIFY(!data.isEmpty());
    
    TestableXSettings settings2(static_cast<QObject*>(nullptr));
    settings2.populateSettings(data);
    
    QCOMPARE(settings2.getPropertyValue("Net/CursorBlinkTime").toInt(), 500);
    QCOMPARE(settings2.getPropertyValue("Gtk/FontName").toByteArray(), QByteArray("DejaVu Sans 12"));
    QCOMPARE(settings2.getPropertyValue("Gtk/EnableAnimations").toInt(), 1);
}

void TestXSettings::testRoundTrip_Empty()
{
    TestableXSettings settings1(static_cast<QObject*>(nullptr));
    
    QByteArray data = settings1.depopulateSettings();
    QVERIFY(data.isEmpty());
}

void TestXSettings::testPropertyValueUpdate()
{
    TestableXSettings settings(static_cast<QObject*>(nullptr));
    settings.setPropertyValue("test/key", 100);
    QCOMPARE(settings.getPropertyValue("test/key").toInt(), 100);
    
    settings.setPropertyValue("test/key", 200);
    QCOMPARE(settings.getPropertyValue("test/key").toInt(), 200);
}

void TestXSettings::testToByteArray()
{
    QCOMPARE(XSettings::toByteArray(XSettings::Xft_DPI), QByteArray("Xft/DPI"));
    QCOMPARE(XSettings::toByteArray(XSettings::Gtk_FontName), QByteArray("Gtk/FontName"));
    QCOMPARE(XSettings::toByteArray(XSettings::Net_ThemeName), QByteArray("Net/ThemeName"));
    QCOMPARE(XSettings::toByteArray(XSettings::Unknown), QByteArray(""));
}

QTEST_MAIN(TestXSettings)
#include "main.moc"
