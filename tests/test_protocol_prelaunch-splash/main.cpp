// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "modules/prelaunch-splash/prelaunchsplash.h"

#include <wserver.h>

#include <QObject>
#include <QSignalSpy>
#include <QTest>

class PrelaunchSplashTest : public QObject
{
    Q_OBJECT

    WAYLIB_SERVER_NAMESPACE::WServer *m_server = nullptr;
    PrelaunchSplash *m_protocol = nullptr;

public:
    PrelaunchSplashTest(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

private Q_SLOTS:

    void initTestCase()
    {
        m_server = new WAYLIB_SERVER_NAMESPACE::WServer();
    }

    void testCreate()
    {
        m_protocol = m_server->attach<PrelaunchSplash>();
        QVERIFY(m_protocol != nullptr);
    }

    void verifyPrelaunchSplash()
    {
        QVERIFY(m_protocol != nullptr);
    }

    void testSignals()
    {
        QVERIFY(m_protocol != nullptr);

        QSignalSpy splashRequestedSpy(m_protocol, &PrelaunchSplash::splashRequested);
        QVERIFY(splashRequestedSpy.isValid());

        QSignalSpy splashCloseSpy(m_protocol, &PrelaunchSplash::splashCloseRequested);
        QVERIFY(splashCloseSpy.isValid());
    }

    void cleanupTestCase()
    {
        m_server->deleteLater();
        m_server = nullptr;
        m_protocol = nullptr;
    }
};

QTEST_MAIN(PrelaunchSplashTest)
#include "main.moc"
