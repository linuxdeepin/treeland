// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QTest>
#include <QSignalSpy>

#include "greeter/greeterproxy.h"

class TestGreeterProxy : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testProperties();
    void testSetShowShutdownView();
    void testIsConnected();
    void testLock();
    void testSwitchUser();
};

void TestGreeterProxy::testProperties()
{
    GreeterProxy proxy(true);
    
    QCOMPARE(proxy.hostName(), QString());
    QCOMPARE(proxy.canPowerOff(), false);
    QCOMPARE(proxy.canReboot(), false);
    QCOMPARE(proxy.canSuspend(), false);
    QCOMPARE(proxy.canHibernate(), false);
    QCOMPARE(proxy.canHybridSleep(), false);
    QCOMPARE(proxy.isLocked(), false);
    QCOMPARE(proxy.failedAttempts(), 0);
    QCOMPARE(proxy.showShutdownView(), false);
    QCOMPARE(proxy.showAnimation(), true);
    QCOMPARE(proxy.hasActiveSession(), false);
}

void TestGreeterProxy::testSetShowShutdownView()
{
    GreeterProxy proxy(true);
    QSignalSpy spy(&proxy, &GreeterProxy::showShutdownViewChanged);
    
    proxy.setShowShutdownView(true);
    QVERIFY(proxy.showShutdownView());
    QCOMPARE(spy.count(), 1);
    
    proxy.setShowShutdownView(true);
    QCOMPARE(spy.count(), 1);
    
    proxy.setShowShutdownView(false);
    QVERIFY(!proxy.showShutdownView());
    QCOMPARE(spy.count(), 2);
}

void TestGreeterProxy::testIsConnected()
{
    GreeterProxy proxy(true);
    QVERIFY(!proxy.isConnected());
}

void TestGreeterProxy::testLock()
{
    GreeterProxy proxy(true);
    QSignalSpy spy(&proxy, &GreeterProxy::lockChanged);
    
    proxy.lock();
    QVERIFY(proxy.isLocked());
    QCOMPARE(spy.count(), 1);
}

void TestGreeterProxy::testSwitchUser()
{
    GreeterProxy proxy(true);
    QSignalSpy spy(&proxy, &GreeterProxy::switchUser);
    
    proxy.switchUser();
    QCOMPARE(spy.count(), 1);
}

QTEST_MAIN(TestGreeterProxy)
#include "main.moc"
