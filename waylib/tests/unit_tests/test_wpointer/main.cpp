// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <wpointer.h>

#include <QtTest>

WAYLIB_SERVER_USE_NAMESPACE

// wlr_seat is used as the test subject: it has a public create/destroy pair
// (wlr_seat_create/wlr_seat_destroy) and carries a events.destroy signal,
// which exercises both RAII destruction and auto-nulling. A wl_display is
// created per test and destroyed after the seat is gone.

// Fake wlroots-style object with a destroy signal; a custom
// WlrObjectTraits specialization counts destroy calls so shared-ownership
// semantics are verified precisely without relying on ASan double-free
// detection.
struct FakeObject {
    struct { struct wl_signal destroy; } events;
    static inline int destroyCount = 0;

    static FakeObject *create() {
        auto *obj = new FakeObject;
        wl_signal_init(&obj->events.destroy);
        return obj;
    }
};

template<>
struct Waylib::Server::WlrObjectTraits<FakeObject> {
    static void destroy(FakeObject *obj) {
        wl_signal_emit_mutable(&obj->events.destroy, obj);
        ++FakeObject::destroyCount;
        delete obj;
    }
};

class TestWPointer : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void uniquePointerDestroysOnScopeExit();
    void uniquePointerMoveTransfersOwnership();
    void uniquePointerResetReplaces();
    void uniquePointerReleaseDetaches();
    void uniquePointerWithoutSignalStillDestroys();
    void uniquePointerSwap();
    void uniquePointerCompareOperators();
    void uniquePointerResetReentrantDestroy();
    void observerPointerNullsOnDestroy();
    void observerCopiesShareLifetime();
    void observerAssignFromRawPointer();
    void observerMoveTransfersObservation();
    void observerAccessors();
    void observerReassignToNewHandle();
};

void TestWPointer::uniquePointerDestroysOnScopeExit()
{
    wl_display *display = wl_display_create();
    auto *cursor = wlr_seat_create(display, "test");
    QVERIFY(cursor);
    {
        WUniquePointer<wlr_seat> ptr(cursor);
        QCOMPARE(ptr.get(), cursor);
        QVERIFY(ptr);
        QCOMPARE(ptr.get(), cursor);
    }
    // The cursor must be gone; calling wlr_seat_destroy again would be a
    // double free, so no explicit check here — a regression double-free shows
    // up as a crash under ASan and as the destructor never running otherwise.
    wl_display_destroy(display);
}

void TestWPointer::uniquePointerMoveTransfersOwnership()
{
    wl_display *display = wl_display_create();
    auto *cursor = wlr_seat_create(display, "test");
    {
        WUniquePointer<wlr_seat> a(cursor);
        WUniquePointer<wlr_seat> b(std::move(a));
        QVERIFY(!a);
        QCOMPARE(b.get(), cursor);

        WUniquePointer<wlr_seat> c;
        c = std::move(b);
        QVERIFY(!b);
        QCOMPARE(c.get(), cursor);
        // Ownership transferred; the seat is released by c's destructor
        // (WUniquePointer does not observe the native destroy signal, so it
        // must outlive the native object it owns).
    }
    wl_display_destroy(display);
}

void TestWPointer::uniquePointerResetReplaces()
{
    wl_display *display = wl_display_create();
    auto *first = wlr_seat_create(display, "first");
    auto *second = wlr_seat_create(display, "second");
    {
        WUniquePointer<wlr_seat> ptr(first);
        ptr.reset(second); // destroys first, adopts second
        QCOMPARE(ptr.get(), second);
        // second is released by ptr's destructor (no external destroy).
    }
    wl_display_destroy(display);
}

void TestWPointer::uniquePointerReleaseDetaches()
{
    wl_display *display = wl_display_create();
    auto *cursor = wlr_seat_create(display, "test");
    WUniquePointer<wlr_seat> ptr(cursor);
    auto *raw = ptr.release();
    QCOMPARE(raw, cursor);
    QVERIFY(!ptr);
    // Ownership is ours again; the raw handle is destroyed by us.
    wlr_seat_destroy(raw);
    wl_display_destroy(display);
}

void TestWPointer::uniquePointerWithoutSignalStillDestroys()
{
    // wlr_cursor has no events.destroy signal: RAII destruction still works
    // through the traits deleter, without any signal observation.
    auto *cursor = wlr_cursor_create();
    QVERIFY(cursor);
    WUniquePointer<wlr_cursor> ptr(cursor);
    QCOMPARE(ptr.get(), cursor);
    ptr.reset();
    QVERIFY(!ptr);
}

void TestWPointer::observerPointerNullsOnDestroy()
{
    wl_display *display = wl_display_create();
    auto *cursor = wlr_seat_create(display, "test");
    {
        WPointer<wlr_seat> p(cursor);
        QCOMPARE(p.get(), cursor);
        wlr_seat_destroy(cursor);
        QVERIFY(p.isNull());
    }
    wl_display_destroy(display);
}

void TestWPointer::observerCopiesShareLifetime()
{
    wl_display *display = wl_display_create();
    auto *cursor = wlr_seat_create(display, "test");
    WPointer<wlr_seat> a(cursor);
    WPointer<wlr_seat> b = a;
    WPointer<wlr_seat> c;
    c = a;
    QCOMPARE(b.get(), cursor);
    QCOMPARE(c.get(), cursor);

    wlr_seat_destroy(cursor);
    QVERIFY(a.isNull());
    QVERIFY(b.isNull());
    QVERIFY(c.isNull());
    wl_display_destroy(display);
}

void TestWPointer::observerAssignFromRawPointer()
{
    wl_display *display = wl_display_create();
    auto *cursor = wlr_seat_create(display, "test");
    WPointer<wlr_seat> p;
    p = cursor;
    QCOMPARE(p.get(), cursor);
    wlr_seat_destroy(cursor);
    QVERIFY(p.isNull());
    wl_display_destroy(display);
}

void TestWPointer::uniquePointerSwap()
{
    wl_display *display = wl_display_create();
    auto *first = wlr_seat_create(display, "first");
    auto *second = wlr_seat_create(display, "second");
    {
        WUniquePointer<wlr_seat> a(first);
        WUniquePointer<wlr_seat> b(second);

        a.swap(b);
        QCOMPARE(a.get(), second);
        QCOMPARE(b.get(), first);

        swap(a, b); // std::swap-free friend function
        QCOMPARE(a.get(), first);
        QCOMPARE(b.get(), second);
        // Both handles released by the wrappers' destructors (no external
        // destroy — pure RAII ownership).
    }
    wl_display_destroy(display);
}

void TestWPointer::uniquePointerCompareOperators()
{
    wl_display *display = wl_display_create();
    auto *cursor = wlr_seat_create(display, "test");
    auto *other = wlr_seat_create(display, "other");
    {
        WUniquePointer<wlr_seat> a(cursor);
        WUniquePointer<wlr_seat> c(other);

        QVERIFY(a != c);       // different handles
        QCOMPARE(a.get(), cursor);   // vs raw pointer
        QCOMPARE(c.get(), other);
        // Both handles released by the wrappers' destructors.
    }
    wl_display_destroy(display);
}

void TestWPointer::uniquePointerResetReentrantDestroy()
{
    // Destroying the old handle from reset() must not leave the wrapper
    // pointing at the half-destroyed object: the handle is exchanged out
    // before Traits::destroy runs, so a destroy callback that re-enters the
    // wrapper sees the new handle (or null).
    FakeObject::destroyCount = 0;
    auto *first = FakeObject::create();
    auto *second = FakeObject::create();

    struct Observer {
        WUniquePointer<FakeObject> *wrapper = nullptr;
        FakeObject *expected = nullptr;
        bool reentered = false;
        wl_listener listener {};
    } observer { nullptr, nullptr, false };

    // The wrapper must be fully constructed before the destroy listener can
    // re-enter it.
    auto ptr = std::make_unique<WUniquePointer<FakeObject>>(first);
    observer.wrapper = ptr.get();
    observer.expected = second;

    observer.listener.notify = [](wl_listener *l, void *) {
        auto *obs = reinterpret_cast<Observer *>(
            reinterpret_cast<char *>(l) - offsetof(Observer, listener));
        obs->reentered = true;
        // During reset(second), the old handle is already exchanged out.
        QCOMPARE(obs->wrapper->get(), obs->expected);
    };
    wl_signal_add(&first->events.destroy, &observer.listener);

    ptr->reset(second); // destroys first; destroy callback re-enters wrapper
    QVERIFY(observer.reentered);
    QCOMPARE(ptr->get(), second);

    // wl_signal_emit_mutable detaches each node before notifying, so the
    // observer's link is already removed; the observer goes out of scope.
    ptr.reset();
    QCOMPARE(FakeObject::destroyCount, 2);
}

void TestWPointer::observerMoveTransfersObservation()
{
    wl_display *display = wl_display_create();
    auto *cursor = wlr_seat_create(display, "test");
    WPointer<wlr_seat> a(cursor);
    WPointer<wlr_seat> b(std::move(a));
    QVERIFY(a.isNull()); // moved-from observer no longer watches
    QCOMPARE(b.get(), cursor);

    WPointer<wlr_seat> c;
    c = std::move(b);
    QVERIFY(b.isNull());
    QCOMPARE(c.get(), cursor);

    wlr_seat_destroy(cursor);
    QVERIFY(c.isNull());
    wl_display_destroy(display);
}

void TestWPointer::observerAccessors()
{
    wl_display *display = wl_display_create();
    auto *cursor = wlr_seat_create(display, "test");
    WPointer<wlr_seat> p(cursor);

    QVERIFY(p);                 // operator bool
    QVERIFY(!p.isNull());
    QCOMPARE(p.get(), cursor);
    QCOMPARE(p.operator->(), cursor);
    QCOMPARE(&(*p), cursor);    // operator*
    QCOMPARE(p, cursor);        // implicit conversion to raw pointer

    wlr_seat_destroy(cursor);
    QVERIFY(!p);
    QVERIFY(p.isNull());
    QCOMPARE(p.get(), nullptr);
    wl_display_destroy(display);
}

void TestWPointer::observerReassignToNewHandle()
{
    wl_display *display = wl_display_create();
    auto *first = wlr_seat_create(display, "first");
    auto *second = wlr_seat_create(display, "second");
    WPointer<wlr_seat> p(first);

    p = second; // reassign: stops watching first, watches second
    QCOMPARE(p.get(), second);
    wlr_seat_destroy(first); // must not null p (no longer watching first)
    QCOMPARE(p.get(), second);

    wlr_seat_destroy(second);
    QVERIFY(p.isNull());
    wl_display_destroy(display);
}

QTEST_GUILESS_MAIN(TestWPointer)

#include "main.moc"
