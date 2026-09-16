// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "modules/appearance/appearanceinterfacev1.h"
#include "modules/appearance/appearancemanagerinterfacev1.h"

#include <wserver.h>

#include <QObject>
#include <QTest>

class AppearanceTest : public QObject
{
    Q_OBJECT

    WAYLIB_SERVER_NAMESPACE::WServer *m_server = nullptr;

public:
    AppearanceTest(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

private Q_SLOTS:

    void initTestCase()
    {
        m_server = new WAYLIB_SERVER_NAMESPACE::WServer();
    }

    // --- AppearanceInterfaceV1 (read-only push) ---

    void testCreateAppearance()
    {
        QVERIFY(m_server->attach<AppearanceInterfaceV1>(m_server) != nullptr);
    }

    void verifyAppearance()
    {
        auto protocol = m_server->findChild<AppearanceInterfaceV1 *>();
        QVERIFY(protocol != nullptr);
    }

    // --- AppearanceManagerInterfaceV1 (privileged setter) ---

    void testCreateAppearanceManager()
    {
        QVERIFY(m_server->attach<AppearanceManagerInterfaceV1>(m_server) != nullptr);
    }

    void verifyAppearanceManager()
    {
        auto protocol = m_server->findChild<AppearanceManagerInterfaceV1 *>();
        QVERIFY(protocol != nullptr);
    }

    // --- interfaceName sanity ---

    void testAppearanceInterfaceName()
    {
        auto *appearance = m_server->findChild<AppearanceInterfaceV1 *>();
        QVERIFY(appearance != nullptr);
        QVERIFY(!appearance->interfaceName().isEmpty());
    }

    void testAppearanceManagerInterfaceName()
    {
        auto *manager = m_server->findChild<AppearanceManagerInterfaceV1 *>();
        QVERIFY(manager != nullptr);
        QVERIFY(!manager->interfaceName().isEmpty());
    }

    void cleanupTestCase()
    {
        m_server->deleteLater();
        m_server = nullptr;
    }
};

QTEST_MAIN(AppearanceTest)
#include "main.moc"
