// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <wscoplistener.h>

#include <wayland-server-core.h>

#include <QtTest>

WAYLIB_SERVER_USE_NAMESPACE

// A connected WScopedListener must be safely movable: the wl_listener node
// is re-linked into the original signal at the new instance's address, so
// callbacks keep firing and destruction never removes a stale link.

class TestWScopedListener : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void moveConstructConnected();
    void moveAssignConnected();
    void moveUnconnected();
    void disconnectInsideCallback();
    void destroyListenerInsideCallback();
    void destroyListInsideCallback();
    void moveThenExternalDestroy();
    void memberFunctionCallback();
    void constMemberFunctionCallback();
    void voidArgumentLambda();
    void dataPassedToCallback();
    void reinitToAnotherSignal();
    void reinitDoesNotLeakOldClosure();
    void listMultipleListenersAndClear();
    void listMovePreservesCallbacks();
    void emitOrderFollowsRegistration();
};

void TestWScopedListener::moveConstructConnected()
{
    wl_signal sig;
    wl_signal_init(&sig);
    int count = 0;

    WScopedListener a;
    a.init(&sig, [&count](void *) { count++; });

    WScopedListener b(std::move(a));
    QVERIFY(!a.isConnected());
    QVERIFY(b.isConnected());

    wl_signal_emit_mutable(&sig, nullptr);
    QCOMPARE(count, 1); // re-linked node still receives the signal
}

void TestWScopedListener::moveAssignConnected()
{
    wl_signal sig;
    wl_signal_init(&sig);
    int count = 0;

    WScopedListener a;
    a.init(&sig, [&count](void *) { count++; });

    WScopedListener b;
    b = std::move(a);
    QVERIFY(!a.isConnected());
    QVERIFY(b.isConnected());

    wl_signal_emit_mutable(&sig, nullptr);
    QCOMPARE(count, 1);

    // Move-assign over an already-connected target: the target's own link is
    // dropped (disconnect), and b's listener is re-linked at c's address.
    WScopedListener c;
    c.init(&sig, [&count](void *) { count++; });
    c = std::move(b);
    QVERIFY(!b.isConnected());
    QVERIFY(c.isConnected());
    wl_signal_emit_mutable(&sig, nullptr);
    QCOMPARE(count, 2); // b's callback, now living on c
}

void TestWScopedListener::moveUnconnected()
{
    WScopedListener a;
    WScopedListener b(std::move(a));
    QVERIFY(!b.isConnected());

    wl_signal sig;
    wl_signal_init(&sig);
    int count = 0;
    b.init(&sig, [&count](void *) { count++; });
    wl_signal_emit_mutable(&sig, nullptr);
    QCOMPARE(count, 1);
}

void TestWScopedListener::disconnectInsideCallback()
{
    wl_signal sig;
    wl_signal_init(&sig);
    int count = 0;

    WScopedListener a;
    a.init(&sig, [&](void *) {
        count++;
        a.disconnect(); // self-detach during emit_mutable is safe
    });

    wl_signal_emit_mutable(&sig, nullptr);
    wl_signal_emit_mutable(&sig, nullptr);
    QCOMPARE(count, 1); // only the first emit reached the listener
}

void TestWScopedListener::destroyListenerInsideCallback()
{
    wl_signal sig;
    wl_signal_init(&sig);
    int count = 0;

    auto *listener = new WScopedListener;
    listener->init(&sig, [&count, listener](void *) {
        count++;
        // Destroy the listener from inside its own callback. The closure
        // (this lambda) is shared_ptr-managed, so it stays alive until the
        // emission returns even though the listener object is gone.
        delete listener;
    });

    wl_signal_emit_mutable(&sig, nullptr);
    QCOMPARE(count, 1);
    // Emitting again must not touch the destroyed listener.
    wl_signal_emit_mutable(&sig, nullptr);
    QCOMPARE(count, 1);
}

void TestWScopedListener::destroyListInsideCallback()
{
    wl_signal sig;
    wl_signal_init(&sig);
    int count = 0;

    auto *list = new WScopedListenerList;
    list->add(&sig, [&count, list](void *) {
        count++;
        // Destroy the whole list (and its listeners) from inside a callback
        // that belongs to it.
        delete list;
    });

    wl_signal_emit_mutable(&sig, nullptr);
    QCOMPARE(count, 1);
    wl_signal_emit_mutable(&sig, nullptr);
    QCOMPARE(count, 1);
}

void TestWScopedListener::moveThenExternalDestroy()
{
    wl_signal sig;
    wl_signal_init(&sig);
    int count = 0;

    WScopedListener a;
    a.init(&sig, [&count](void *) { count++; });

    WScopedListener b(std::move(a));
    // A signal that is never emitted again is fine; destruction of b just
    // unlinks the node. The stack signal then goes out of scope — no UAF.
    QCOMPARE(b.isConnected(), true);
}

void TestWScopedListener::memberFunctionCallback()
{
    wl_signal sig;
    wl_signal_init(&sig);
    int count = 0;

    struct Receiver {
        int *counter;
        void onEvent(void *) { ++(*counter); }
        void onNoArg() { ++(*counter); }
        void onConst(void *) const { ++(*constCounter); }
        int *constCounter;
    } receiver { &count, &count };

    WScopedListener a;
    a.init(&sig, &receiver, &Receiver::onEvent);
    wl_signal_emit_mutable(&sig, nullptr);
    QCOMPARE(count, 1); // only a registered so far

    WScopedListener b;
    b.init(&sig, &receiver, &Receiver::onNoArg);
    wl_signal_emit_mutable(&sig, nullptr);
    QCOMPARE(count, 3); // both a and b fire
}

void TestWScopedListener::constMemberFunctionCallback()
{
    wl_signal sig;
    wl_signal_init(&sig);
    int count = 0;

    struct Receiver {
        int *counter;
        void onConst(void *) const { ++(*counter); }
    } receiver { &count };

    WScopedListener a;
    a.init(&sig, &receiver, &Receiver::onConst);
    wl_signal_emit_mutable(&sig, nullptr);
    QCOMPARE(count, 1);
}

void TestWScopedListener::voidArgumentLambda()
{
    wl_signal sig;
    wl_signal_init(&sig);
    int count = 0;

    // Lambda taking no parameters must still be invocable.
    WScopedListener a;
    a.init(&sig, [&count]() { count++; });
    wl_signal_emit_mutable(&sig, nullptr);
    QCOMPARE(count, 1);

    // Also the 2-arg init form without an object.
    WScopedListener b;
    b.init(&sig, [&count]() { count++; });
    wl_signal_emit_mutable(&sig, nullptr);
    QCOMPARE(count, 3); // both a and b fire
}

void TestWScopedListener::dataPassedToCallback()
{
    wl_signal sig;
    wl_signal_init(&sig);
    int received = 0;
    struct Payload { int value; } payload { 42 };

    WScopedListener a;
    a.init(&sig, [&received](Payload *p) { received = p->value; });
    wl_signal_emit_mutable(&sig, &payload);
    QCOMPARE(received, 42);

    // Member-function overload with the event pointer.
    struct Receiver {
        int *out;
        void onPayload(Payload *p) { *out = p->value; }
    } receiver { &received };
    WScopedListener b;
    b.init(&sig, &receiver, &Receiver::onPayload);
    wl_signal_emit_mutable(&sig, &payload);
    QCOMPARE(received, 42);
}

void TestWScopedListener::reinitToAnotherSignal()
{
    wl_signal sig1, sig2;
    wl_signal_init(&sig1);
    wl_signal_init(&sig2);
    int count = 0;

    WScopedListener a;
    a.init(&sig1, [&count](void *) { count++; });
    wl_signal_emit_mutable(&sig1, nullptr);
    QCOMPARE(count, 1);

    // Re-init onto another signal: the old link must be dropped and only the
    // new signal fires.
    a.init(&sig2, [&count](void *) { count++; });
    wl_signal_emit_mutable(&sig1, nullptr);
    QCOMPARE(count, 1); // sig1 no longer observed
    wl_signal_emit_mutable(&sig2, nullptr);
    QCOMPARE(count, 2);
}

void TestWScopedListener::reinitDoesNotLeakOldClosure()
{
    wl_signal sig1, sig2;
    wl_signal_init(&sig1);
    wl_signal_init(&sig2);

    // Track closure lifetime: each live copy of Tracker increments alive.
    // Re-init must free the previous closure before creating the new one;
    // without the fix the old heap allocation is leaked and alive == 2.
    struct Tracker {
        int *alive;
        // Construct: one new logical entity → alive++.
        // Move: the new entity inherits the old's identity (no alive++),
        // the old's alive is nulled so its destructor is a no-op.
        // This makes alive == number of logically-live closures, so
        // a leaked old closure shows up as alive > 1 after re-init.
        explicit Tracker(int *a) : alive(a) { ++*alive; }
        Tracker(const Tracker &o) : alive(o.alive) { ++*alive; }
        Tracker(Tracker &&o) noexcept : alive(o.alive) { o.alive = nullptr; }
        ~Tracker() { if (alive) --*alive; }
        Tracker &operator=(const Tracker &) = delete;
        void operator()(void *) {}
    };

    int alive = 0;
    {
        WScopedListener a;
        a.init(&sig1, Tracker(&alive));
        QCOMPARE(alive, 1);
        a.init(&sig2, Tracker(&alive));
        QCOMPARE(alive, 1); // old freed, new created — still one alive
    }
    QCOMPARE(alive, 0);
}

void TestWScopedListener::listMultipleListenersAndClear()
{
    wl_signal sig;
    wl_signal_init(&sig);
    int a = 0, b = 0;

    WScopedListenerList list;
    list.add(&sig, [&a](void *) { a++; });
    list.add(&sig, [&b](void *) { b++; });

    wl_signal_emit_mutable(&sig, nullptr);
    QCOMPARE(a, 1);
    QCOMPARE(b, 1);

    list.clear();
    wl_signal_emit_mutable(&sig, nullptr);
    QCOMPARE(a, 1); // listeners detached
    QCOMPARE(b, 1);
}

void TestWScopedListener::listMovePreservesCallbacks()
{
    wl_signal sig;
    wl_signal_init(&sig);
    int count = 0;

    WScopedListenerList a;
    a.add(&sig, [&count](void *) { count++; });

    WScopedListenerList b(std::move(a));
    wl_signal_emit_mutable(&sig, nullptr);
    QCOMPARE(count, 1); // node re-linked at b's storage, still fires

    WScopedListenerList c;
    c = std::move(b);
    wl_signal_emit_mutable(&sig, nullptr);
    QCOMPARE(count, 2);
}

void TestWScopedListener::emitOrderFollowsRegistration()
{
    wl_signal sig;
    wl_signal_init(&sig);
    QList<int> order;

    WScopedListenerList list;
    list.add(&sig, [&order](void *) { order << 1; });
    list.add(&sig, [&order](void *) { order << 2; });
    list.add(&sig, [&order](void *) { order << 3; });

    wl_signal_emit_mutable(&sig, nullptr);
    QCOMPARE(order, QList<int>({1, 2, 3}));
}

QTEST_GUILESS_MAIN(TestWScopedListener)

#include "main.moc"
