// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "input/gestures.h"

#include <QSignalSpy>
#include <QTest>

class GestureTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void swipeGestureReportsProgressFromMinimumDelta()
    {
        SwipeGesture gesture;
        gesture.setDirection(SwipeGesture::Right);
        gesture.setMinimumDelta(QPointF(10, 0));

        QCOMPARE(gesture.deltaToProgress(QPointF(0, 0)), 0.0);
        QCOMPARE(gesture.deltaToProgress(QPointF(5, 0)), 0.5);
        QCOMPARE(gesture.deltaToProgress(QPointF(15, 0)), 1.0);
        QVERIFY(!gesture.minimumDeltaReached(QPointF(9, 0)));
        QVERIFY(gesture.minimumDeltaReached(QPointF(10, 0)));
    }

    void recognizerStartsAndTriggersMatchingSwipe()
    {
        GestureRecognizer recognizer;
        auto *gesture = new SwipeGesture;
        gesture->setMinimumFingerCount(3);
        gesture->setMaximumFingerCount(3);
        gesture->setDirection(SwipeGesture::Right);
        gesture->setMinimumDelta(QPointF(10, 0));

        QSignalSpy startedSpy(gesture, &SwipeGesture::started);
        QSignalSpy progressSpy(gesture, &SwipeGesture::progress);
        QSignalSpy triggeredSpy(gesture, &SwipeGesture::triggered);
        QSignalSpy cancelledSpy(gesture, &SwipeGesture::cancelled);

        recognizer.registerSwipeGesture(gesture);

        QCOMPARE(recognizer.startSwipeGesture(2), 0);
        QCOMPARE(startedSpy.count(), 0);

        QCOMPARE(recognizer.startSwipeGesture(3), 1);
        QCOMPARE(startedSpy.count(), 1);

        recognizer.updateSwipeGesture(QPointF(5, 0));
        QCOMPARE(progressSpy.count(), 1);
        QCOMPARE(progressSpy.takeFirst().at(0).toReal(), 0.5);

        recognizer.updateSwipeGesture(QPointF(5, 0));
        recognizer.endSwipeGesture();

        QCOMPARE(triggeredSpy.count(), 1);
        QCOMPARE(cancelledSpy.count(), 0);
        recognizer.unregisterSwipeGesture(gesture);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    void recognizerCancelsSwipeBelowMinimumDelta()
    {
        GestureRecognizer recognizer;
        auto *gesture = new SwipeGesture;
        gesture->setDirection(SwipeGesture::Down);
        gesture->setMinimumDelta(QPointF(0, 20));

        QSignalSpy triggeredSpy(gesture, &SwipeGesture::triggered);
        QSignalSpy cancelledSpy(gesture, &SwipeGesture::cancelled);

        recognizer.registerSwipeGesture(gesture);
        QCOMPARE(recognizer.startSwipeGesture(1), 1);
        recognizer.updateSwipeGesture(QPointF(0, 10));
        recognizer.endSwipeGesture();

        QCOMPARE(triggeredSpy.count(), 0);
        QCOMPARE(cancelledSpy.count(), 1);
        recognizer.unregisterSwipeGesture(gesture);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
};

QTEST_MAIN(GestureTest)
#include "main.moc"
