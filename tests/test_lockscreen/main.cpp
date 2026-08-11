// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QTest>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>

#include "core/lockscreen.h"
#include "interfaces/lockscreeninterface.h"
#include "greeter/greeterproxy.h"
#include "surface/surfacecontainer.h"

class MockLockScreen : public QObject, public ILockScreen
{
    Q_OBJECT
    Q_INTERFACES(ILockScreen)

public:
    mutable int createCallCount = 0;
    mutable Output *lastOutput = nullptr;
    mutable QQuickItem *lastParent = nullptr;

    QQuickItem *createLockScreen(Output *output, QQuickItem *parent) override
    {
        ++createCallCount;
        lastOutput = output;
        lastParent = parent;
        auto *item = new QQuickItem(parent);
        item->setObjectName("mockLockScreenItem");
        return item;
    }
};

class MockGreeterProxy : public GreeterProxy
{
    Q_OBJECT

public:
    bool m_isLocked = false;
    bool m_showShutdownView = false;
    int lockCallCount = 0;

    explicit MockGreeterProxy(QObject *parent = nullptr)
        : GreeterProxy(true, parent)
    {
    }

    bool isLocked() const override
    {
        return m_isLocked;
    }

    bool showShutdownView() const override
    {
        return m_showShutdownView;
    }

    void setShowShutdownView(bool show) override
    {
        if (m_showShutdownView != show) {
            m_showShutdownView = show;
            Q_EMIT showShutdownViewChanged(show);
        }
    }

    void lock() override
    {
        ++lockCallCount;
        m_isLocked = true;
        Q_EMIT lockChanged(true);
    }
};

class TestLockScreen : public QObject
{
    Q_OBJECT

private:
    QQuickWindow *m_window = nullptr;
    MockLockScreen *m_mockImpl = nullptr;
    MockGreeterProxy *m_mockGreeter = nullptr;
    SurfaceContainer *m_parentContainer = nullptr;
    LockScreen *m_lockScreen = nullptr;

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void testAvailable();
    void testLock();
    void testLockWhenAlreadyLocked();
    void testShutdown();
    void testShutdownWhenAlreadyVisible();
    void testSwitchUser();
    void testSwitchUserWhenAlreadyVisible();
    void testIsLocked();
    void testSetPrimaryOutputName();
};

void TestLockScreen::initTestCase()
{
    m_window = new QQuickWindow;
    m_mockImpl = new MockLockScreen();
    m_mockGreeter = new MockGreeterProxy();
    // Use SurfaceContainer(QQuickItem*) to avoid triggering ensureQmlContext()
    m_parentContainer = new SurfaceContainer(static_cast<QQuickItem *>(nullptr));
    m_parentContainer->setParentItem(m_window->contentItem());
    m_lockScreen = new LockScreen(m_mockImpl, m_parentContainer, m_mockGreeter);
}

void TestLockScreen::cleanupTestCase()
{
    delete m_lockScreen;
    delete m_parentContainer;
    delete m_mockGreeter;
    delete m_mockImpl;
    delete m_window;
}

void TestLockScreen::testAvailable()
{
    QVERIFY(m_lockScreen->available());
}

void TestLockScreen::testLock()
{
    m_mockGreeter->m_isLocked = false;
    m_lockScreen->setVisible(false);

    QSignalSpy unlockSpy(m_lockScreen, &LockScreen::unlock);

    m_lockScreen->lock();

    QVERIFY(m_lockScreen->isVisible());
    QCOMPARE(m_mockGreeter->lockCallCount, 1);
}

void TestLockScreen::testLockWhenAlreadyLocked()
{
    m_lockScreen->setVisible(true);
    int prevLockCount = m_mockGreeter->lockCallCount;

    m_lockScreen->lock();

    QCOMPARE(m_mockGreeter->lockCallCount, prevLockCount);
}

void TestLockScreen::testShutdown()
{
    m_lockScreen->setVisible(false);
    m_mockGreeter->m_showShutdownView = false;

    m_lockScreen->shutdown();

    QVERIFY(m_lockScreen->isVisible());
    QVERIFY(m_mockGreeter->m_showShutdownView);
}

void TestLockScreen::testShutdownWhenAlreadyVisible()
{
    m_lockScreen->setVisible(true);
    bool prevShow = m_mockGreeter->m_showShutdownView;

    m_lockScreen->shutdown();

    QCOMPARE(m_mockGreeter->m_showShutdownView, prevShow);
}

void TestLockScreen::testSwitchUser()
{
    m_lockScreen->setVisible(false);
    QSignalSpy spy(m_mockGreeter, &GreeterProxy::switchUser);

    m_lockScreen->switchUser();

    QVERIFY(m_lockScreen->isVisible());
    QCOMPARE(spy.count(), 1);
}

void TestLockScreen::testSwitchUserWhenAlreadyVisible()
{
    m_lockScreen->setVisible(true);
    QSignalSpy spy(m_mockGreeter, &GreeterProxy::switchUser);

    m_lockScreen->switchUser();

    QCOMPARE(spy.count(), 0);
}

void TestLockScreen::testIsLocked()
{
    m_lockScreen->setVisible(false);
    QVERIFY(!m_lockScreen->isLocked());

    m_lockScreen->setVisible(true);
    QVERIFY(m_lockScreen->isLocked());
}

void TestLockScreen::testSetPrimaryOutputName()
{
    m_lockScreen->setPrimaryOutputName("HDMI-1");
    QCOMPARE(m_lockScreen->primaryOutputName(), QString("HDMI-1"));
}

QTEST_MAIN(TestLockScreen)
#include "main.moc"
