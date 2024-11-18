// Copyright (C) 2024 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wallpapercolor.h"

#include <wserver.h>

#include <QObject>
#include <QTest>

class WallpaperColorTest : public QObject
{
    Q_OBJECT

    WAYLIB_SERVER_NAMESPACE::WServer *m_server = nullptr;

public:
    WallpaperColorTest(QObject *WallpaperColor = nullptr)
        : QObject(WallpaperColor)
    {
    }

private Q_SLOTS:

    void initTestCase()
    {
        m_server = new WAYLIB_SERVER_NAMESPACE::WServer();
    }

    void testCreate()
    {
        QVERIFY(m_server->attach<WallpaperColorV1>(m_server) != nullptr);
    }

    void verifyWallpaperColor()
    {
        auto protocol = m_server->findChild<WallpaperColorV1 *>();
        QVERIFY(protocol != nullptr);
    }

    void cleanupTestCase()
    {
        m_server->deleteLater();
        m_server = nullptr;
    }
};

QTEST_MAIN(WallpaperColorTest)
#include "main.moc"
