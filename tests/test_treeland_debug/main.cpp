// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "debughelpers.h"

#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QTemporaryDir>
#include <QTest>

// Unit tests for the pure-logic helpers shared by the treeland-debug tool
// (stateName, buttonCode, keyCode, saveCapture, pointToJson, rectToJson).
// These do not require a running compositor or a Wayland connection.
class TreelandDebugTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testStateNameKnownStates();
    void testStateNameUnknown();
    void testButtonCodeNamed();
    void testButtonCodeRaw();
    void testButtonCodeUnknown();
    void testKeyCodeNamed();
    void testKeyCodeRaw();
    void testKeyCodeUnknown();
    void testKeyCodeCaseInsensitive();
    void testSaveCaptureExplicitPath();
    void testSaveCaptureAddsPngSuffix();
    void testSaveCaptureEmptyData();
    void testSaveCaptureAutoPath();
    void testPointToJson();
    void testRectToJson();
};

void TreelandDebugTest::testStateNameKnownStates()
{
    QCOMPARE(stateName(0), QStringLiteral("Normal"));
    QCOMPARE(stateName(1), QStringLiteral("Maximized"));
    QCOMPARE(stateName(2), QStringLiteral("Minimized"));
    QCOMPARE(stateName(3), QStringLiteral("Fullscreen"));
    QCOMPARE(stateName(4), QStringLiteral("Tiling"));
}

void TreelandDebugTest::testStateNameUnknown()
{
    QCOMPARE(stateName(99), QStringLiteral("Unknown(99)"));
    QCOMPARE(stateName(-1), QStringLiteral("Unknown(-1)"));
}

void TreelandDebugTest::testButtonCodeNamed()
{
    bool ok = false;
    QCOMPARE(buttonCode(QStringLiteral("left"), &ok), 0x110);
    QVERIFY(ok);
    QCOMPARE(buttonCode(QStringLiteral("right"), &ok), 0x111);
    QVERIFY(ok);
    QCOMPARE(buttonCode(QStringLiteral("middle"), &ok), 0x112);
    QVERIFY(ok);
    // Names are case-insensitive.
    QCOMPARE(buttonCode(QStringLiteral("LEFT"), &ok), 0x110);
    QVERIFY(ok);
}

void TreelandDebugTest::testButtonCodeRaw()
{
    bool ok = false;
    // buttonCode passes raw codes through QString::toInt (base 10), so use
    // decimal values; hex prefixes like "0x113" are not recognized here.
    QCOMPARE(buttonCode(QStringLiteral("275"), &ok), 275); // 0x113 == 275
    QVERIFY(ok);
    QCOMPARE(buttonCode(QStringLiteral("277"), &ok), 277);
    QVERIFY(ok);
}

void TreelandDebugTest::testButtonCodeUnknown()
{
    bool ok = true;
    QCOMPARE(buttonCode(QStringLiteral("foobar"), &ok), 0);
    QVERIFY(!ok);
}

void TreelandDebugTest::testKeyCodeNamed()
{
    bool ok = false;
    QCOMPARE(keyCode(QStringLiteral("esc"), &ok), 1);
    QVERIFY(ok);
    QCOMPARE(keyCode(QStringLiteral("enter"), &ok), 28);
    QVERIFY(ok);
    QCOMPARE(keyCode(QStringLiteral("space"), &ok), 57);
    QVERIFY(ok);
    QCOMPARE(keyCode(QStringLiteral("up"), &ok), 103);
    QVERIFY(ok);
    QCOMPARE(keyCode(QStringLiteral("f12"), &ok), 88);
    QVERIFY(ok);
}

void TreelandDebugTest::testKeyCodeRaw()
{
    bool ok = false;
    QCOMPARE(keyCode(QStringLiteral("200"), &ok), 200);
    QVERIFY(ok);
}

void TreelandDebugTest::testKeyCodeUnknown()
{
    bool ok = true;
    QCOMPARE(keyCode(QStringLiteral("nonexistent"), &ok), 0);
    QVERIFY(!ok);
}

void TreelandDebugTest::testKeyCodeCaseInsensitive()
{
    bool ok = false;
    QCOMPARE(keyCode(QStringLiteral("ESC"), &ok), 1);
    QVERIFY(ok);
    QCOMPARE(keyCode(QStringLiteral("Enter"), &ok), 28);
    QVERIFY(ok);
}

void TreelandDebugTest::testSaveCaptureExplicitPath()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("capture.png");
    const QByteArray data("PNG-fake-bytes");
    const QString saved = saveCapture(data, path);
    QCOMPARE(saved, path);
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.readAll(), data);
}

void TreelandDebugTest::testSaveCaptureAddsPngSuffix()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString base = dir.filePath("noext");
    const QByteArray data("data");
    const QString saved = saveCapture(data, base);
    QVERIFY(saved.endsWith(QStringLiteral(".png")));
    QVERIFY(QFile::exists(saved));
}

void TreelandDebugTest::testSaveCaptureEmptyData()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("empty.png");
    const QString saved = saveCapture(QByteArray(), path);
    QVERIFY(saved.isEmpty());
    QVERIFY(!QFile::exists(path));
}

void TreelandDebugTest::testSaveCaptureAutoPath()
{
    const QByteArray data("autodata");
    const QString saved = saveCapture(data, QString());
    QVERIFY(!saved.isEmpty());
    QVERIFY(saved.startsWith(QStringLiteral("/tmp/treeland-debug-")));
    QVERIFY(saved.endsWith(QStringLiteral(".png")));
    QVERIFY(QFile::exists(saved));
    QFile::remove(saved);
}

void TreelandDebugTest::testPointToJson()
{
    const auto obj = pointToJson(QPointF(1.5, -2.0));
    QCOMPARE(obj.value("x").toDouble(), 1.5);
    QCOMPARE(obj.value("y").toDouble(), -2.0);
}

void TreelandDebugTest::testRectToJson()
{
    const auto obj = rectToJson(QRectF(10, 20, 300, 400));
    QCOMPARE(obj.value("x").toDouble(), 10.0);
    QCOMPARE(obj.value("y").toDouble(), 20.0);
    QCOMPARE(obj.value("width").toDouble(), 300.0);
    QCOMPARE(obj.value("height").toDouble(), 400.0);
}

QTEST_MAIN(TreelandDebugTest)
#include "main.moc"
