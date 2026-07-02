// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "input/gestures.h"

#include <QObject>
#include <QTest>
#include <QSignalSpy>
#include <QElapsedTimer>
#include <QThread>

class GesturesTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void testSwipeGestureDefaultDirection()
    {
        SwipeGesture gesture;
        QCOMPARE(gesture.direction(), SwipeGesture::Down);
    }

    void testSwipeGestureFingerCount()
    {
        SwipeGesture gesture;
        gesture.setMinimumFingerCount(3);
        gesture.setMaximumFingerCount(4);
        QCOMPARE(gesture.minimumFingerCount(), 3u);
        QCOMPARE(gesture.maximumFingerCount(), 4u);
        QVERIFY(gesture.minimumFingerCountIsRelevant());
        QVERIFY(gesture.maximumFingerCountIsRelevant());
    }

    void testSwipeGestureMaxFingerCount()
    {
        SwipeGesture gesture;
        gesture.setMaximumFingerCount(5);
        QCOMPARE(gesture.maximumFingerCount(), 5u);
    }

    void testSwipeGestureDirectionSet()
    {
        SwipeGesture gesture;
        gesture.setDirection(SwipeGesture::Left);
        QCOMPARE(gesture.direction(), SwipeGesture::Left);
    }

    void testSwipeGestureMinimumDelta()
    {
        SwipeGesture gesture;
        gesture.setMinimumDelta(QPointF(100, 0));
        QCOMPARE(gesture.minimumDelta(), QPointF(100, 0));
    }

    void testSwipeGestureDeltaToProgress()
    {
        SwipeGesture gesture;
        gesture.setDirection(SwipeGesture::Right);
        gesture.setMinimumDelta(QPointF(100, 0));
        QCOMPARE(gesture.deltaToProgress(QPointF(50, 0)), 0.5);
    }

    void testSwipeGestureMinimumDeltaReached()
    {
        SwipeGesture gesture;
        gesture.setDirection(SwipeGesture::Right);
        gesture.setMinimumDelta(QPointF(100, 0));
        QVERIFY(gesture.minimumDeltaReached(QPointF(100, 0)));
        QVERIFY(!gesture.minimumDeltaReached(QPointF(50, 0)));
    }

    void testSwipeGesturePositionConstraints()
    {
        SwipeGesture gesture;
        gesture.setStartGeometry(QRect(0, 0, 100, 100));
        QVERIFY(gesture.minimumXIsRelevant());
        QCOMPARE(gesture.minimumX(), 0);
        QVERIFY(gesture.maximumXIsRelevant());
        QCOMPARE(gesture.maximumX(), 99);
        QVERIFY(gesture.minimumYIsRelevant());
        QCOMPARE(gesture.minimumY(), 0);
        QVERIFY(gesture.maximumYIsRelevant());
        QCOMPARE(gesture.maximumY(), 99);
    }

    void testSwipeGestureOppositeDown()
    {
        QCOMPARE(SwipeGesture::opposite(SwipeGesture::Down), SwipeGesture::Up);
    }

    void testSwipeGestureOppositeLeft()
    {
        QCOMPARE(SwipeGesture::opposite(SwipeGesture::Left), SwipeGesture::Right);
    }

    void testSwipeGestureOppositeInvalid()
    {
        QCOMPARE(SwipeGesture::opposite(SwipeGesture::Invalid), SwipeGesture::Invalid);
    }

    void testGestureRecognizerStartByFingerCount()
    {
        GestureRecognizer recognizer;
        SwipeGesture gesture;
        gesture.setMinimumFingerCount(3);
        gesture.setMaximumFingerCount(3);
        gesture.setDirection(SwipeGesture::Up);
        QSignalSpy startedSpy(&gesture, &SwipeGesture::started);
        recognizer.registerSwipeGesture(&gesture);
        int count = recognizer.startSwipeGesture(3);
        QCOMPARE(count, 1);
        QCOMPARE(startedSpy.count(), 1);
    }

    void testGestureRecognizerFingerMismatch()
    {
        GestureRecognizer recognizer;
        SwipeGesture gesture;
        gesture.setMinimumFingerCount(3);
        gesture.setMaximumFingerCount(3);
        gesture.setDirection(SwipeGesture::Up);
        QSignalSpy startedSpy(&gesture, &SwipeGesture::started);
        recognizer.registerSwipeGesture(&gesture);
        int count = recognizer.startSwipeGesture(4);
        QCOMPARE(count, 0);
        QCOMPARE(startedSpy.count(), 0);
    }

    void testGestureRecognizerCancel()
    {
        GestureRecognizer recognizer;
        SwipeGesture gesture;
        gesture.setMinimumFingerCount(3);
        gesture.setDirection(SwipeGesture::Down);
        QSignalSpy cancelledSpy(&gesture, &SwipeGesture::cancelled);
        recognizer.registerSwipeGesture(&gesture);
        recognizer.startSwipeGesture(3);
        recognizer.cancelSwipeGesture();
        QCOMPARE(cancelledSpy.count(), 1);
    }

    void testGestureRecognizerEndTriggered()
    {
        GestureRecognizer recognizer;
        SwipeGesture gesture;
        gesture.setDirection(SwipeGesture::Right);
        gesture.setMinimumDelta(QPointF(100, 0));
        QSignalSpy triggeredSpy(&gesture, &SwipeGesture::triggered);
        recognizer.registerSwipeGesture(&gesture);
        recognizer.startSwipeGesture(1);
        recognizer.updateSwipeGesture(QPointF(100, 0));
        recognizer.endSwipeGesture();
        QCOMPARE(triggeredSpy.count(), 1);
    }

    void testGestureRecognizerEndCancelled()
    {
        GestureRecognizer recognizer;
        SwipeGesture gesture;
        gesture.setDirection(SwipeGesture::Right);
        gesture.setMinimumDelta(QPointF(100, 0));
        QSignalSpy cancelledSpy(&gesture, &SwipeGesture::cancelled);
        recognizer.registerSwipeGesture(&gesture);
        recognizer.startSwipeGesture(1);
        recognizer.updateSwipeGesture(QPointF(10, 0));
        recognizer.endSwipeGesture();
        QCOMPARE(cancelledSpy.count(), 1);
    }

    void testGestureRecognizerStartByPosition()
    {
        GestureRecognizer recognizer;
        SwipeGesture gesture;
        gesture.setDirection(SwipeGesture::Down);
        gesture.setStartGeometry(QRect(0, 0, 100, 100));
        QSignalSpy startedSpy(&gesture, &SwipeGesture::started);
        recognizer.registerSwipeGesture(&gesture);
        int count = recognizer.startSwipeGesture(QPointF(50, 50));
        QCOMPARE(count, 1);
        QCOMPARE(startedSpy.count(), 1);
    }

    void testGestureRecognizerHoldGesture()
    {
        GestureRecognizer recognizer;
        HoldGesture gesture;
        gesture.setInterval(100);
        QSignalSpy longPressedSpy(&gesture, &HoldGesture::longPressed);
        recognizer.registerHoldGesture(&gesture);
        recognizer.startHoldGesture(1);
        recognizer.endHoldGesture();
        QCOMPARE(longPressedSpy.count(), 0);
    }

    void testGestureRecognizerHoldGestureLongPressed()
    {
        GestureRecognizer recognizer;
        HoldGesture gesture;
        gesture.setInterval(10);
        QSignalSpy longPressedSpy(&gesture, &HoldGesture::longPressed);
        recognizer.registerHoldGesture(&gesture);
        recognizer.startHoldGesture(1);
        QElapsedTimer timer;
        timer.start();
        while (longPressedSpy.count() == 0 && timer.elapsed() < 1000) {
            QCoreApplication::processEvents();
            QThread::msleep(5);
        }
        QCOMPARE(longPressedSpy.count(), 1);
        recognizer.endHoldGesture();
    }

    void testGestureRecognizerAxisLockHorizontal()
    {
        GestureRecognizer recognizer;
        SwipeGesture gesture;
        gesture.setDirection(SwipeGesture::Right);
        gesture.setMinimumDelta(QPointF(100, 0));
        QSignalSpy triggeredSpy(&gesture, &SwipeGesture::triggered);
        recognizer.registerSwipeGesture(&gesture);
        recognizer.startSwipeGesture(1);
        recognizer.updateSwipeGesture(QPointF(50, 0));
        recognizer.updateSwipeGesture(QPointF(50, 30));
        recognizer.updateSwipeGesture(QPointF(50, -30));
        recognizer.endSwipeGesture();
        QCOMPARE(triggeredSpy.count(), 1);
    }

    void testGestureRecognizerAxisLockVertical()
    {
        GestureRecognizer recognizer;
        SwipeGesture gesture;
        gesture.setDirection(SwipeGesture::Down);
        gesture.setMinimumDelta(QPointF(0, 100));
        QSignalSpy triggeredSpy(&gesture, &SwipeGesture::triggered);
        recognizer.registerSwipeGesture(&gesture);
        recognizer.startSwipeGesture(1);
        recognizer.updateSwipeGesture(QPointF(0, 50));
        recognizer.updateSwipeGesture(QPointF(30, 50));
        recognizer.updateSwipeGesture(QPointF(-30, 50));
        recognizer.endSwipeGesture();
        QCOMPARE(triggeredSpy.count(), 1);
    }

    void testGestureRecognizerAxisLockRejectsOrthogonal()
    {
        GestureRecognizer recognizer;
        SwipeGesture horizontalGesture;
        horizontalGesture.setDirection(SwipeGesture::Right);
        horizontalGesture.setMinimumDelta(QPointF(100, 0));
        SwipeGesture verticalGesture;
        verticalGesture.setDirection(SwipeGesture::Down);
        verticalGesture.setMinimumDelta(QPointF(0, 100));
        QSignalSpy hTriggeredSpy(&horizontalGesture, &SwipeGesture::triggered);
        QSignalSpy vTriggeredSpy(&verticalGesture, &SwipeGesture::triggered);
        recognizer.registerSwipeGesture(&horizontalGesture);
        recognizer.registerSwipeGesture(&verticalGesture);
        recognizer.startSwipeGesture(1);
        recognizer.updateSwipeGesture(QPointF(100, 0));
        recognizer.endSwipeGesture();
        QCOMPARE(hTriggeredSpy.count(), 1);
        QCOMPARE(vTriggeredSpy.count(), 0);
    }
};

QTEST_MAIN(GesturesTest)
#include "main.moc"
