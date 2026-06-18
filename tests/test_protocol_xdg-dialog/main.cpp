// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "modules/xdg-dialog/xdgdialogmanagerinterfacev1.h"

#include <wserver.h>

#include <QSignalSpy>
#include <QTest>

WAYLIB_SERVER_USE_NAMESPACE

class XdgDialogV1Test : public QObject
{
    Q_OBJECT

    WServer *m_server = nullptr;
    XdgDialogManagerInterfaceV1 *m_protocol = nullptr;

public:
    XdgDialogV1Test(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

private Q_SLOTS:

    void initTestCase()
    {
        m_server = new WServer();
    }

    void testCreateProtocol()
    {
        m_protocol = m_server->attach<XdgDialogManagerInterfaceV1>();
        QVERIFY(m_protocol != nullptr);
    }

    void testVerifyProtocol()
    {
        QVERIFY(m_protocol != nullptr);
    }

    void testSignals()
    {
        QVERIFY(m_protocol != nullptr);

        QSignalSpy modalSpy(m_protocol, &XdgDialogManagerInterfaceV1::surfaceModalChanged);
        QVERIFY(modalSpy.isValid());
    }

    void cleanupTestCase()
    {
        m_server->deleteLater();
        m_server = nullptr;
        m_protocol = nullptr;
    }
};

QTEST_MAIN(XdgDialogV1Test)
#include "main.moc"
