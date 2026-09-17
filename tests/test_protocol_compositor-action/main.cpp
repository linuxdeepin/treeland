// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "modules/compositor-action/compositoractioninterfacev1.h"

#include <wserver.h>

#include <QObject>
#include <QSignalSpy>
#include <QTest>

class CompositorActionTest : public QObject
{
    Q_OBJECT

    WAYLIB_SERVER_NAMESPACE::WServer *m_server = nullptr;
    CompositorActionInterfaceV1 *m_protocol = nullptr;

public:
    CompositorActionTest(QObject *parent = nullptr)
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
        QVERIFY(m_server->attach<CompositorActionInterfaceV1>(m_server) != nullptr);
    }

    void verifyProtocol()
    {
        m_protocol = m_server->findChild<CompositorActionInterfaceV1 *>();
        QVERIFY(m_protocol != nullptr);
    }

    void testInterfaceName()
    {
        QVERIFY(m_protocol != nullptr);
        QVERIFY(!m_protocol->interfaceName().isEmpty());
        QCOMPARE(m_protocol->interfaceName(),
                 QByteArrayView("treeland_compositor_action_v1"));
    }

    // The trigger request is fire-and-forget: the interface only relays the
    // raw action value, it never routes or validates it itself.
    void testTriggerRelaysAction()
    {
        QVERIFY(m_protocol != nullptr);
        QSignalSpy spy(m_protocol, &CompositorActionInterfaceV1::triggered);
        QVERIFY(spy.isValid());

        // Simulate what the generated resource does on a trigger request.
        QMetaObject::invokeMethod(m_protocol, [this] {
            Q_EMIT m_protocol->triggered(18); // toggle_multitask_view
        });
        QMetaObject::invokeMethod(m_protocol, [this] {
            Q_EMIT m_protocol->triggered(999); // unknown future action
        });

        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(0).at(0).toUInt(), 18u);
        QCOMPARE(spy.at(1).at(0).toUInt(), 999u);
    }

    void cleanupTestCase()
    {
        m_server->deleteLater();
        m_server = nullptr;
    }
};

QTEST_MAIN(CompositorActionTest)
#include "main.moc"
