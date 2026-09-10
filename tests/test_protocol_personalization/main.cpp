// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "modules/personalization/personalizationmanagerinterfacev1.h"
#include "modules/personalization/decorationmanagerinterfacev1.h"
#include "modules/personalization/appearanceinterfacev1.h"
#include "modules/personalization/appearancemanagerinterfacev1.h"

#include <wserver.h>

#include <QObject>
#include <QTest>

class PersonalizationTest : public QObject
{
    Q_OBJECT

    WAYLIB_SERVER_NAMESPACE::WServer *m_server = nullptr;

public:
    PersonalizationTest(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

private Q_SLOTS:

    void initTestCase()
    {
        m_server = new WAYLIB_SERVER_NAMESPACE::WServer();
    }

    // --- Old protocol (kept during transition) ---

    void testCreateOldProtocol()
    {
        QVERIFY(m_server->attach<PersonalizationManagerInterfaceV1>(m_server) != nullptr);
    }

    void verifyOldProtocol()
    {
        auto protocol = m_server->findChild<PersonalizationManagerInterfaceV1 *>();
        QVERIFY(protocol != nullptr);
    }

    // --- New protocols (treeland-protocols 0.6.0) ---

    void testCreateDecoration()
    {
        QVERIFY(m_server->attach<DecorationManagerInterfaceV1>(m_server) != nullptr);
    }

    void verifyDecoration()
    {
        auto protocol = m_server->findChild<DecorationManagerInterfaceV1 *>();
        QVERIFY(protocol != nullptr);
    }

    void testCreateAppearance()
    {
        QVERIFY(m_server->attach<AppearanceInterfaceV1>(m_server) != nullptr);
    }

    void verifyAppearance()
    {
        auto protocol = m_server->findChild<AppearanceInterfaceV1 *>();
        QVERIFY(protocol != nullptr);
    }

    void testCreateAppearanceManager()
    {
        QVERIFY(m_server->attach<AppearanceManagerInterfaceV1>(m_server) != nullptr);
    }

    void verifyAppearanceManager()
    {
        auto protocol = m_server->findChild<AppearanceManagerInterfaceV1 *>();
        QVERIFY(protocol != nullptr);
    }

    void cleanupTestCase()
    {
        m_server->deleteLater();
        m_server = nullptr;
    }
};

QTEST_MAIN(PersonalizationTest)
#include "main.moc"
