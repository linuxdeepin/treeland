// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "modules/personalization/personalizationmanagerinterfacev1.h"

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

    void testCreate()
    {
        QVERIFY(m_server->attach<PersonalizationManagerInterfaceV1>(m_server) != nullptr);
    }

    void verifyPersonalization()
    {
        auto protocol = m_server->findChild<PersonalizationManagerInterfaceV1 *>();
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
