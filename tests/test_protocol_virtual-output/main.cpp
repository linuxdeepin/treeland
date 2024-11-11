#include "virtualoutputmanager.h"

#include <wserver.h>

#include <QObject>
#include <QTest>

class VirtualOutputTest : public QObject
{
    Q_OBJECT

    WAYLIB_SERVER_NAMESPACE::WServer *m_server = nullptr;

public:
    VirtualOutputTest(QObject *parent = nullptr)
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
        QVERIFY(m_server->attach<VirtualOutputV1>(m_server) != nullptr);
    }

    void verifyVirtualOutput()
    {
        auto protocol = m_server->findChild<VirtualOutputV1 *>();
        QVERIFY(protocol != nullptr);
    }

    void cleanupTestCase()
    {
        m_server->deleteLater();
        m_server = nullptr;
    }
};

QTEST_MAIN(VirtualOutputTest)
#include "main.moc"
