#include "shortcutmanager.h"

#include <wserver.h>

#include <QObject>
#include <QTest>

class ShortcutTest : public QObject
{
    Q_OBJECT

    WAYLIB_SERVER_NAMESPACE::WServer *m_server = nullptr;

public:
    ShortcutTest(QObject *parent = nullptr)
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
        QVERIFY(m_server->attach<ShortcutV1>(m_server) != nullptr);
    }

    void verifyShortcut()
    {
        auto protocol = m_server->findChild<ShortcutV1*>();
        QVERIFY(protocol != nullptr);
    }

    void cleanupTestCase()
    {
        m_server->deleteLater();
        m_server = nullptr;
    }
};

QTEST_MAIN(ShortcutTest)
#include "main.moc"
