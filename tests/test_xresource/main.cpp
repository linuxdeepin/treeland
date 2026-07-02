// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QTest>
#include <QPair>
#include <QByteArray>

#include "xsettings/xresource.h"

class TestXResource : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testSplitSimpleKeyValue();
    void testSplitKeyOnly();
    void testSplitEmptyLine();
    void testSplitWhitespace();
    void testSplitEscapedColon();
    void testSplitMultipleColons();
    void testSplitValueWithColon();
    void testSplitLeadingTrailingWhitespace();
    void testToByteArray();
};

void TestXResource::testSplitSimpleKeyValue()
{
    auto [key, value] = XResource::splitXResourceLine("Xft.dpi: 96");
    QCOMPARE(key, QByteArray("Xft.dpi"));
    QCOMPARE(value, QByteArray("96"));
}

void TestXResource::testSplitKeyOnly()
{
    auto [key, value] = XResource::splitXResourceLine("some.key");
    QCOMPARE(key, QByteArray("some.key"));
    QVERIFY(value.isEmpty());
}

void TestXResource::testSplitEmptyLine()
{
    auto [key, value] = XResource::splitXResourceLine("");
    QVERIFY(key.isEmpty());
    QVERIFY(value.isEmpty());
}

void TestXResource::testSplitWhitespace()
{
    auto [key, value] = XResource::splitXResourceLine("  Xft.dpi  :  96  ");
    QCOMPARE(key, QByteArray("Xft.dpi"));
    QCOMPARE(value, QByteArray("96"));
}

void TestXResource::testSplitEscapedColon()
{
    auto [key, value] = XResource::splitXResourceLine("key\\:with\\:colons: value");
    QCOMPARE(key, QByteArray("key\\:with\\:colons"));
    QCOMPARE(value, QByteArray("value"));
}

void TestXResource::testSplitMultipleColons()
{
    auto [key, value] = XResource::splitXResourceLine("key: value:with:colons");
    QCOMPARE(key, QByteArray("key"));
    QCOMPARE(value, QByteArray("value:with:colons"));
}

void TestXResource::testSplitValueWithColon()
{
    auto [key, value] = XResource::splitXResourceLine("Xcursor.theme: Adwaita");
    QCOMPARE(key, QByteArray("Xcursor.theme"));
    QCOMPARE(value, QByteArray("Adwaita"));
}

void TestXResource::testSplitLeadingTrailingWhitespace()
{
    auto [key, value] = XResource::splitXResourceLine("\t  Xft.hinting\t:\t1\t");
    QCOMPARE(key, QByteArray("Xft.hinting"));
    QCOMPARE(value, QByteArray("1"));
}

void TestXResource::testToByteArray()
{
    QCOMPARE(XResource::toByteArray(XResource::Xft_DPI), QByteArray("Xft.dpi"));
    QCOMPARE(XResource::toByteArray(XResource::Xft_Antialias), QByteArray("Xft.antialias"));
    QCOMPARE(XResource::toByteArray(XResource::Gtk_FontName), QByteArray("Gtk/FontName"));
    QCOMPARE(XResource::toByteArray(XResource::Net_ThemeName), QByteArray("Net/ThemeName"));
    QCOMPARE(XResource::toByteArray(XResource::Unknown), QByteArray(""));
}

QTEST_MAIN(TestXResource)
#include "main.moc"
