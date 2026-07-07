// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QTest>
#include <QPointF>
#include <QRectF>
#include <QSizeF>

#include "output/output.h"

class TestOutputScale : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testCalcPreferredScale_StandardDisplay();
    void testCalcPreferredScale_HighDPI();
    void testCalcPreferredScale_ZeroDimensions();
    void testCalcPreferredScale_NegativeDimensions();
    void testCalcPreferredScale_4KDisplay();
    void testCalcPreferredScale_SmallDisplay();
    
    void testConstrainToValidArea_Centered();
    void testConstrainToValidArea_TopLeft();
    void testConstrainToValidArea_BottomRight();
    void testConstrainToValidArea_ExceedLeft();
    void testConstrainToValidArea_ExceedRight();
    void testConstrainToValidArea_ExceedTop();
    void testConstrainToValidArea_ExceedBottom();
    void testConstrainToValidArea_ExactFit();
    void testConstrainToValidArea_LargerThanValid();
};

void TestOutputScale::testCalcPreferredScale_StandardDisplay()
{
    double scale = Output::calcPreferredScale(1920, 1080, 477, 268);
    QCOMPARE(scale, 1.0);
}

void TestOutputScale::testCalcPreferredScale_HighDPI()
{
    double scale = Output::calcPreferredScale(2560, 1440, 310, 174);
    QVERIFY(scale >= 1.25);
    QVERIFY(scale <= 2.0);
}

void TestOutputScale::testCalcPreferredScale_ZeroDimensions()
{
    QCOMPARE(Output::calcPreferredScale(0, 1080, 477, 268), 1.0);
    QCOMPARE(Output::calcPreferredScale(1920, 0, 477, 268), 1.0);
    QCOMPARE(Output::calcPreferredScale(1920, 1080, 0, 268), 1.0);
    QCOMPARE(Output::calcPreferredScale(1920, 1080, 477, 0), 1.0);
}

void TestOutputScale::testCalcPreferredScale_NegativeDimensions()
{
    QCOMPARE(Output::calcPreferredScale(-1920, 1080, 477, 268), 1.0);
    QCOMPARE(Output::calcPreferredScale(1920, 1080, -477, 268), 1.0);
}

void TestOutputScale::testCalcPreferredScale_4KDisplay()
{
    double scale = Output::calcPreferredScale(3840, 2160, 600, 340);
    QVERIFY(scale >= 1.5);
    QVERIFY(scale <= 2.5);
}

void TestOutputScale::testCalcPreferredScale_SmallDisplay()
{
    double scale = Output::calcPreferredScale(1366, 768, 344, 194);
    QVERIFY(scale >= 0.75);
    QVERIFY(scale <= 1.25);
}

void TestOutputScale::testConstrainToValidArea_Centered()
{
    QPointF pos(100, 100);
    QSizeF windowSize(200, 150);
    QRectF validGeo(0, 0, 500, 400);
    
    QPointF result = Output::constrainToValidArea(pos, windowSize, validGeo);
    QCOMPARE(result, QPointF(100, 100));
}

void TestOutputScale::testConstrainToValidArea_TopLeft()
{
    QPointF pos(0, 0);
    QSizeF windowSize(200, 150);
    QRectF validGeo(0, 0, 500, 400);
    
    QPointF result = Output::constrainToValidArea(pos, windowSize, validGeo);
    QCOMPARE(result, QPointF(0, 0));
}

void TestOutputScale::testConstrainToValidArea_BottomRight()
{
    QPointF pos(300, 250);
    QSizeF windowSize(200, 150);
    QRectF validGeo(0, 0, 500, 400);
    
    QPointF result = Output::constrainToValidArea(pos, windowSize, validGeo);
    QCOMPARE(result, QPointF(300, 250));
}

void TestOutputScale::testConstrainToValidArea_ExceedLeft()
{
    QPointF pos(-50, 100);
    QSizeF windowSize(200, 150);
    QRectF validGeo(0, 0, 500, 400);
    
    QPointF result = Output::constrainToValidArea(pos, windowSize, validGeo);
    QCOMPARE(result.x(), 0.0);
}

void TestOutputScale::testConstrainToValidArea_ExceedRight()
{
    QPointF pos(400, 100);
    QSizeF windowSize(200, 150);
    QRectF validGeo(0, 0, 500, 400);
    
    QPointF result = Output::constrainToValidArea(pos, windowSize, validGeo);
    QCOMPARE(result.x(), 300.0);
}

void TestOutputScale::testConstrainToValidArea_ExceedTop()
{
    QPointF pos(100, -50);
    QSizeF windowSize(200, 150);
    QRectF validGeo(0, 0, 500, 400);
    
    QPointF result = Output::constrainToValidArea(pos, windowSize, validGeo);
    QCOMPARE(result.y(), 0.0);
}

void TestOutputScale::testConstrainToValidArea_ExceedBottom()
{
    QPointF pos(100, 350);
    QSizeF windowSize(200, 150);
    QRectF validGeo(0, 0, 500, 400);
    
    QPointF result = Output::constrainToValidArea(pos, windowSize, validGeo);
    QCOMPARE(result.y(), 250.0);
}

void TestOutputScale::testConstrainToValidArea_ExactFit()
{
    QPointF pos(300, 250);
    QSizeF windowSize(200, 150);
    QRectF validGeo(0, 0, 500, 400);
    
    QPointF result = Output::constrainToValidArea(pos, windowSize, validGeo);
    QCOMPARE(result.x(), 300.0);
    QCOMPARE(result.y(), 250.0);
}

void TestOutputScale::testConstrainToValidArea_LargerThanValid()
{
    QPointF pos(100, 100);
    QSizeF windowSize(600, 500);
    QRectF validGeo(0, 0, 500, 400);
    
    QPointF result = Output::constrainToValidArea(pos, windowSize, validGeo);
    QVERIFY(result.x() <= 0.0);
    QVERIFY(result.y() <= 0.0);
}

QTEST_MAIN(TestOutputScale)
#include "main.moc"
