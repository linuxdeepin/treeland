// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "input/togglablegesture.h"

#include <QObject>
#include <QTest>
#include <QSignalSpy>

class TestableTogglableGesture : public TogglableGesture
{
public:
    using TogglableGesture::setProgress;
    using TogglableGesture::setRegress;
    using TogglableGesture::activeTriggered;
    using TogglableGesture::deactivateTriggered;
};

class TogglableGestureTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void testInitialState()
    {
        TestableTogglableGesture gesture;
        QCOMPARE(gesture.status(), TogglableGesture::Inactive);
        QVERIFY(!gesture.inProgress());
    }

    void testSetProgressFromInactive()
    {
        TestableTogglableGesture gesture;
        QSignalSpy factorSpy(&gesture, &TogglableGesture::partialGestureFactorChanged);
        gesture.setProgress(0.3);
        QCOMPARE(gesture.status(), TogglableGesture::Activating);
        QVERIFY(gesture.inProgress());
        QCOMPARE(factorSpy.count(), 1);
    }

    void testSetProgressFromActivating()
    {
        TestableTogglableGesture gesture;
        gesture.setProgress(0.3);
        gesture.setProgress(0.6);
        QCOMPARE(gesture.status(), TogglableGesture::Activating);
    }

    void testSetRegressFromActive()
    {
        TestableTogglableGesture gesture;
        gesture.activate();
        QSignalSpy factorSpy(&gesture, &TogglableGesture::partialGestureFactorChanged);
        gesture.setRegress(0.3);
        QCOMPARE(gesture.status(), TogglableGesture::Deactivating);
        QVERIFY(gesture.inProgress());
    }

    void testActiveTriggeredHighFactor()
    {
        TestableTogglableGesture gesture;
        gesture.setProgress(0.7);
        QSignalSpy activatedSpy(&gesture, &TogglableGesture::activated);
        gesture.activeTriggered();
        QCOMPARE(gesture.status(), TogglableGesture::Active);
        QCOMPARE(activatedSpy.count(), 1);
    }

    void testActiveTriggeredLowFactor()
    {
        TestableTogglableGesture gesture;
        gesture.setProgress(0.3);
        QSignalSpy deactivatedSpy(&gesture, &TogglableGesture::deactivated);
        gesture.activeTriggered();
        QCOMPARE(gesture.status(), TogglableGesture::Inactive);
        QCOMPARE(deactivatedSpy.count(), 1);
    }

    void testDeactivateTriggeredLowFactor()
    {
        TestableTogglableGesture gesture;
        gesture.activate();
        gesture.setRegress(0.7);
        QSignalSpy deactivatedSpy(&gesture, &TogglableGesture::deactivated);
        gesture.deactivateTriggered();
        QCOMPARE(gesture.status(), TogglableGesture::Inactive);
        QCOMPARE(deactivatedSpy.count(), 1);
    }

    void testDeactivateTriggeredHighFactor()
    {
        TestableTogglableGesture gesture;
        gesture.activate();
        gesture.setRegress(0.3);
        QSignalSpy activatedSpy(&gesture, &TogglableGesture::activated);
        gesture.deactivateTriggered();
        QCOMPARE(gesture.status(), TogglableGesture::Active);
        QCOMPARE(activatedSpy.count(), 1);
    }

    void testActivate()
    {
        TestableTogglableGesture gesture;
        QSignalSpy statusSpy(&gesture, &TogglableGesture::statusChanged);
        gesture.activate();
        QCOMPARE(gesture.status(), TogglableGesture::Active);
        QVERIFY(!gesture.inProgress());
        QCOMPARE(gesture.partialGestureFactor(), 1.0);
        QCOMPARE(statusSpy.count(), 1);
    }

    void testDeactivate()
    {
        TestableTogglableGesture gesture;
        gesture.activate();
        QSignalSpy statusSpy(&gesture, &TogglableGesture::statusChanged);
        gesture.deactivate();
        QCOMPARE(gesture.status(), TogglableGesture::Inactive);
        QVERIFY(!gesture.inProgress());
        QCOMPARE(gesture.partialGestureFactor(), 0.0);
        QCOMPARE(statusSpy.count(), 1);
    }

    void testToggleFromInactive()
    {
        TestableTogglableGesture gesture;
        QSignalSpy activatedSpy(&gesture, &TogglableGesture::activated);
        gesture.toggle();
        QCOMPARE(gesture.status(), TogglableGesture::Active);
        QCOMPARE(activatedSpy.count(), 1);
    }

    void testToggleFromActive()
    {
        TestableTogglableGesture gesture;
        gesture.activate();
        QSignalSpy deactivatedSpy(&gesture, &TogglableGesture::deactivated);
        gesture.toggle();
        QCOMPARE(gesture.status(), TogglableGesture::Inactive);
        QCOMPARE(deactivatedSpy.count(), 1);
    }

    void testStop()
    {
        TestableTogglableGesture gesture;
        gesture.activate();
        gesture.stop();
        QCOMPARE(gesture.status(), TogglableGesture::Stopped);
        QVERIFY(!gesture.inProgress());
        QCOMPARE(gesture.partialGestureFactor(), 0.0);
    }

    void testSetProgressWhileStopped()
    {
        TestableTogglableGesture gesture;
        gesture.activate();
        gesture.stop();
        gesture.setProgress(0.5);
        QCOMPARE(gesture.status(), TogglableGesture::Stopped);
    }

    void testSetRegressWhileStopped()
    {
        TestableTogglableGesture gesture;
        gesture.activate();
        gesture.stop();
        gesture.setRegress(0.5);
        QCOMPARE(gesture.status(), TogglableGesture::Stopped);
    }

    void testPartialGestureFactorSignal()
    {
        TestableTogglableGesture gesture;
        QSignalSpy factorSpy(&gesture, &TogglableGesture::partialGestureFactorChanged);
        gesture.setPartialGestureFactor(0.5);
        QCOMPARE(factorSpy.count(), 1);
        QCOMPARE(gesture.partialGestureFactor(), 0.5);
    }

    void testActiveTriggeredExactHalfFactor()
    {
        TestableTogglableGesture gesture;
        gesture.setProgress(0.5);
        QSignalSpy deactivatedSpy(&gesture, &TogglableGesture::deactivated);
        gesture.activeTriggered();
        QCOMPARE(gesture.status(), TogglableGesture::Inactive);
        QCOMPARE(deactivatedSpy.count(), 1);
    }

    void testDeactivateTriggeredExactHalfFactor()
    {
        TestableTogglableGesture gesture;
        gesture.activate();
        gesture.setRegress(0.5);
        QSignalSpy activatedSpy(&gesture, &TogglableGesture::activated);
        gesture.deactivateTriggered();
        QCOMPARE(gesture.status(), TogglableGesture::Active);
        QCOMPARE(activatedSpy.count(), 1);
    }
};

QTEST_MAIN(TogglableGestureTest)
#include "main.moc"
