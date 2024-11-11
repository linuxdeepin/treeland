#include "personalizationmanager.h"

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
        QVERIFY(m_server->attach<PersonalizationV1>(m_server) != nullptr);
    }

    void verifyPersonalization()
    {
        auto protocol = m_server->findChild<PersonalizationV1 *>();
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
