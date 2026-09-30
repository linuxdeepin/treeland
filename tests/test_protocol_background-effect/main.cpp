// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <wbackgroundeffectmanagerv1.h>
#include <wserver.h>

#include <QObject>
#include <QTest>

class BackgroundEffectTest : public QObject
{
    Q_OBJECT

    WAYLIB_SERVER_NAMESPACE::WServer *m_server = nullptr;
    WAYLIB_SERVER_NAMESPACE::WBackgroundEffectManagerV1 *m_protocol = nullptr;

public:
    BackgroundEffectTest(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

private Q_SLOTS:

    void initTestCase()
    {
        m_server = new WAYLIB_SERVER_NAMESPACE::WServer();
    }

    void testAttach()
    {
        m_protocol = m_server->attach<WAYLIB_SERVER_NAMESPACE::WBackgroundEffectManagerV1>();
        QVERIFY(m_protocol != nullptr);
        QVERIFY(m_server->interfaceList().contains(m_protocol));
        QCOMPARE(m_protocol->interfaceName(), QByteArray("ext_background_effect_manager_v1"));
    }

    void testSurfaceBlurRegionWithoutSurface()
    {
        // Without a surface (or before any client attached an effect object)
        // the committed blur region is empty.
        QVERIFY(m_protocol != nullptr);
        QVERIFY(m_protocol->surfaceBlurRegion(nullptr).isEmpty());
    }

    void cleanupTestCase()
    {
        m_server->deleteLater();
        m_server = nullptr;
    }
};

QTEST_MAIN(BackgroundEffectTest)
#include "main.moc"
