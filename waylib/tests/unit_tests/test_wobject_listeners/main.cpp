// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <wglobal.h>
#include <wscoplistener.h>
#include <private/wglobal_p.h>

#include <wayland-server-core.h>

#include <QtTest>

WAYLIB_SERVER_USE_NAMESPACE

// Minimal WObject that always tears down before ~WObject's empty-list check.
class TestObject : public WObject
{
public:
    ~TestObject() { teardown(); }
};

class TestWObjectListeners : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void selfListenersStableUntilTeardown();
    void crossObjectTeardownDetachesFromTarget();
    void removeListenersClearsBidirectionalLinks();
    void multipleOwnersIndependent();
    void listenerOwnerDestructorRunsTeardown();
    void teardownIdempotent();
};

void TestWObjectListeners::selfListenersStableUntilTeardown()
{
    wl_signal sig;
    wl_signal_init(&sig);
    int count = 0;

    TestObject obj;
    auto *list = obj.listeners();
    QVERIFY(list);
    QCOMPARE(obj.listeners(), list);

    list->add(&sig, [&count](void *) { count++; });
    wl_signal_emit_mutable(&sig, nullptr);
    QCOMPARE(count, 1);

    auto *priv = WObjectPrivate::get(&obj);
    QCOMPARE(priv->attachedListenerLists.size(), 1);
    QCOMPARE(priv->attachedListenerLists.first().first, &obj);
    QVERIFY(priv->attachedListenerTargets.isEmpty());

    obj.teardown();
    QVERIFY(priv->attachedListenerLists.isEmpty());
    QVERIFY(priv->attachedListenerTargets.isEmpty());

    wl_signal_emit_mutable(&sig, nullptr);
    QCOMPARE(count, 1); // disconnected by teardown
}

void TestWObjectListeners::crossObjectTeardownDetachesFromTarget()
{
    wl_signal sig;
    wl_signal_init(&sig);
    int count = 0;

    TestObject target;
    TestObject owner;

    target.listeners(&owner)->add(&sig, [&count](void *) { count++; });
    wl_signal_emit_mutable(&sig, nullptr);
    QCOMPARE(count, 1);

    auto *ownerPriv = WObjectPrivate::get(&owner);
    auto *targetPriv = WObjectPrivate::get(&target);
    QCOMPARE(ownerPriv->attachedListenerTargets, QList<WObject *>{&target});
    QCOMPARE(targetPriv->attachedListenerLists.size(), 1);
    QCOMPARE(targetPriv->attachedListenerLists.first().first, &owner);

    // Owner teardown must auto-detach its groups from every registered target.
    owner.teardown();
    QVERIFY(ownerPriv->attachedListenerTargets.isEmpty());
    QVERIFY(targetPriv->attachedListenerLists.isEmpty());

    wl_signal_emit_mutable(&sig, nullptr);
    QCOMPARE(count, 1);
}

void TestWObjectListeners::removeListenersClearsBidirectionalLinks()
{
    wl_signal sig;
    wl_signal_init(&sig);
    int count = 0;

    TestObject target;
    TestObject owner;

    target.listeners(&owner)->add(&sig, [&count](void *) { count++; });
    wl_signal_emit_mutable(&sig, nullptr);
    QCOMPARE(count, 1);

    target.removeListeners(&owner);

    auto *ownerPriv = WObjectPrivate::get(&owner);
    auto *targetPriv = WObjectPrivate::get(&target);
    QVERIFY(ownerPriv->attachedListenerTargets.isEmpty());
    QVERIFY(targetPriv->attachedListenerLists.isEmpty());

    wl_signal_emit_mutable(&sig, nullptr);
    QCOMPARE(count, 1);
}

void TestWObjectListeners::multipleOwnersIndependent()
{
    wl_signal sig;
    wl_signal_init(&sig);
    int countA = 0;
    int countB = 0;

    TestObject target;
    TestObject ownerA;
    TestObject ownerB;

    target.listeners(&ownerA)->add(&sig, [&countA](void *) { countA++; });
    target.listeners(&ownerB)->add(&sig, [&countB](void *) { countB++; });

    wl_signal_emit_mutable(&sig, nullptr);
    QCOMPARE(countA, 1);
    QCOMPARE(countB, 1);

    ownerA.teardown();
    wl_signal_emit_mutable(&sig, nullptr);
    QCOMPARE(countA, 1);
    QCOMPARE(countB, 2);

    auto *targetPriv = WObjectPrivate::get(&target);
    QCOMPARE(targetPriv->attachedListenerLists.size(), 1);
    QCOMPARE(targetPriv->attachedListenerLists.first().first, &ownerB);

    ownerB.teardown();
    QVERIFY(targetPriv->attachedListenerLists.isEmpty());
    wl_signal_emit_mutable(&sig, nullptr);
    QCOMPARE(countB, 2);
}

void TestWObjectListeners::listenerOwnerDestructorRunsTeardown()
{
    wl_signal sig;
    wl_signal_init(&sig);
    int count = 0;

    TestObject target;
    {
        WListenerOwner owner;
        target.listeners(&owner)->add(&sig, [&count](void *) { count++; });
        wl_signal_emit_mutable(&sig, nullptr);
        QCOMPARE(count, 1);
    }

    // ~WListenerOwner calls teardown(), so the target list is gone and the
    // signal no longer reaches the destroyed owner's callback.
    QVERIFY(WObjectPrivate::get(&target)->attachedListenerLists.isEmpty());
    wl_signal_emit_mutable(&sig, nullptr);
    QCOMPARE(count, 1);
}

void TestWObjectListeners::teardownIdempotent()
{
    TestObject obj;
    obj.listeners();
    QCOMPARE(WObjectPrivate::get(&obj)->attachedListenerLists.size(), 1);

    obj.teardown();
    obj.teardown();
    QVERIFY(WObjectPrivate::get(&obj)->attachedListenerLists.isEmpty());
    QVERIFY(WObjectPrivate::get(&obj)->attachedListenerTargets.isEmpty());
}

QTEST_GUILESS_MAIN(TestWObjectListeners)
#include "main.moc"
