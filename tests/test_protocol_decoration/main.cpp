// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "modules/decoration/decorationmanagerinterfacev1.h"

#include <wserver.h>

#include <QObject>
#include <QTest>

class DecorationTest : public QObject
{
    Q_OBJECT

    WAYLIB_SERVER_NAMESPACE::WServer *m_server = nullptr;

public:
    DecorationTest(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

private Q_SLOTS:

    void initTestCase()
    {
        m_server = new WAYLIB_SERVER_NAMESPACE::WServer();
    }

    void testCreateDecorationManager()
    {
        QVERIFY(m_server->attach<DecorationManagerInterfaceV1>(m_server) != nullptr);
    }

    void verifyDecorationManager()
    {
        auto *manager = m_server->findChild<DecorationManagerInterfaceV1 *>();
        QVERIFY(manager != nullptr);
    }

    void testDecorationInterfaceName()
    {
        auto *manager = m_server->findChild<DecorationManagerInterfaceV1 *>();
        QVERIFY(manager != nullptr);
        QVERIFY(!manager->interfaceName().isEmpty());
    }

    void cleanupTestCase()
    {
        m_server->deleteLater();
        m_server = nullptr;
    }
};

QTEST_MAIN(DecorationTest)
#include "main.moc"
