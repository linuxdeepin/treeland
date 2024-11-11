
#include "windowmanagement.h"

#include <wserver.h>

#include <QObject>
#include <QTest>

class WindowManagementTest : public QObject
{
    Q_OBJECT

    WAYLIB_SERVER_NAMESPACE::WServer *m_server = nullptr;

public:
    WindowManagementTest(QObject *parent = nullptr)
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
        QVERIFY(m_server->attach<WindowManagementV1>(m_server) != nullptr);
    }

    void verifyWindowManagement()
    {
        auto protocol = m_server->findChild<WindowManagementV1 *>();
        QVERIFY(protocol != nullptr);
    }

    void cleanupTestCase()
    {
        m_server->deleteLater();
        m_server = nullptr;
    }
};

QTEST_MAIN(WindowManagementTest)
#include "main.moc"
