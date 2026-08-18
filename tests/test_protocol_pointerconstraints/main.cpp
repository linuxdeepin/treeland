// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <wpointerconstraintsv1.h>
#include <wserver.h>

#include <QObject>
#include <QTest>

using namespace WAYLIB_SERVER_NAMESPACE;

class PointerConstraintsTest : public QObject
{
    Q_OBJECT

    WServer *m_server = nullptr;

public:
    PointerConstraintsTest(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

private Q_SLOTS:
    void initTestCase()
    {
        m_server = new WServer();
    }

    void cleanupTestCase()
    {
        delete m_server;
        m_server = nullptr;
    }

    // The WPointerConstraintsV1 wrapper can be attached via the server
    // and exposes its interface name correctly.
    void testCreateInterface()
    {
        auto *constraints = m_server->attach<WPointerConstraintsV1>();
        QVERIFY(constraints != nullptr);
        QCOMPARE(constraints->interfaceName(), QByteArrayView("zwp_pointer_constraints_v1"));
        QVERIFY(m_server->findInterface<WPointerConstraintsV1>() == constraints);
    }

    // Without any surface/seat there is nothing to constrain.
    void testConstraintForSurfaceReturnsNullWithoutSeat()
    {
        auto *constraints = m_server->findInterface<WPointerConstraintsV1>();
        QVERIFY(constraints != nullptr);
        QVERIFY(constraints->constraintForSurface(nullptr, nullptr) == nullptr);
    }
};

QTEST_MAIN(PointerConstraintsTest)
#include "main.moc"
