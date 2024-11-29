#include "wrapobject.h"

#include <wrappointer.h>

#include <QTest>

#define TEST_RW(wp, p)                                     \
    do {                                                   \
        qint64 time1 = QDateTime::currentSecsSinceEpoch(); \
        (p)->object()->setValue(time1);                    \
        QVERIFY((wp)->object()->value() == time1);         \
        qint64 time2 = QDateTime::currentSecsSinceEpoch(); \
        (wp)->object()->setValue(time2);                   \
        QVERIFY((p)->object()->value() == time2);          \
    } while (0);

#define TEST_ACCESS(wp, p)       \
    QVERIFY((wp));               \
    QVERIFY((wp) == (p));        \
    QVERIFY((wp).data() == (p)); \
    QVERIFY((wp).get() == (p));

#define TEST_VALID(wp, qp) \
    wp->invalidate();      \
    QVERIFY(!wp);          \
    QVERIFY(qp);

class WrapPointerTest : public QObject
{
    Q_OBJECT
public:
    WrapPointerTest(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

private Q_SLOTS:

    void initTestCase() { }

    void testDirectGuardCase()
    {
        // Directly constructed
        FakeWrapObject *p = new FakeWrapObject;
        QScopedPointer<FakeWrapObject> sp(p);
        QPointer<FakeWrapObject> qp(p);
        WrapPointer<FakeWrapObject> wp(p);
        QVERIFY(p->object());
        TEST_ACCESS(wp, p);
        TEST_RW(wp, p);
        TEST_VALID(wp, qp);
    }

    void testDelayedGuardCase()
    {
        // Delayed constructed
        FakeWrapObject *p = new FakeWrapObject;
        QScopedPointer<FakeWrapObject> sp(p);
        QPointer<FakeWrapObject> qp(p);
        WrapPointer<FakeWrapObject> wp;
        wp = p; // Assign operator
        QVERIFY(p->object());
        TEST_ACCESS(wp, p);
        TEST_RW(wp, p);
        TEST_VALID(wp, qp);
    }

    void testCopyAndMoveCase()
    {
        {
            // Copy
            FakeWrapObject *p = new FakeWrapObject;
            QScopedPointer<FakeWrapObject> sp(p);
            QPointer<FakeWrapObject> qp(p);
            WrapPointer<FakeWrapObject> wpr(p);
            WrapPointer<FakeWrapObject> wp(wpr); // Copy constructor
            QVERIFY(p->object());
            TEST_ACCESS(wp, p);
            TEST_ACCESS(wpr, p);
            TEST_RW(wp, p);
            TEST_RW(wpr, p);
            TEST_VALID(wp, qp);
            QVERIFY(!wpr);
        }
        {
            // Copy assignment
            FakeWrapObject *p = new FakeWrapObject;
            QScopedPointer<FakeWrapObject> sp(p);
            QPointer<FakeWrapObject> qp(p);
            WrapPointer<FakeWrapObject> wpr(p);
            WrapPointer<FakeWrapObject> wp = wpr; // Copy assignment
            QVERIFY(p->object());
            TEST_ACCESS(wp, p);
            TEST_ACCESS(wpr, p);
            TEST_RW(wp, p);
            TEST_RW(wpr, p);
            TEST_VALID(wp, qp);
            QVERIFY(!wpr);
        }
        {
            // Move
            FakeWrapObject *p = new FakeWrapObject;
            QScopedPointer<FakeWrapObject> sp(p);
            QPointer<FakeWrapObject> qp(p);
            WrapPointer<FakeWrapObject> wpr(p);
            TEST_ACCESS(wpr, p);
            WrapPointer<FakeWrapObject> wp(std::move(wpr)); // Move constructor
            QVERIFY(!wpr);
            QVERIFY(p->object());
            TEST_ACCESS(wp, p);
            TEST_RW(wp, p);
            TEST_VALID(wp, qp);
        }
        {
            // Move assignment
            FakeWrapObject *p = new FakeWrapObject;
            QScopedPointer<FakeWrapObject> sp(p);
            QPointer<FakeWrapObject> qp(p);
            WrapPointer<FakeWrapObject> wpr(p);
            TEST_ACCESS(wpr, p);
            WrapPointer<FakeWrapObject> wp = std::move(wpr); // Move assignment
            QVERIFY(!wpr);
            QVERIFY(p->object());
            TEST_ACCESS(wp, p);
            TEST_RW(wp, p);
            TEST_VALID(wp, qp);
        }
    }

    void cleanupTestCase() { }
};

QTEST_MAIN(WrapPointerTest)
#include "main.moc"
