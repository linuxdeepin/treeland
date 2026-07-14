// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

// Unit tests for WSGDamageTracker — Layer 2 pure data-structure damage tracker.
//
// These tests verify the core "general capability" of managing a rectangle tree
// and computing damage QRegion when nodes are added, removed, moved, resized,
// or have content refreshed.  No GPU, window, or QSG dependency is required.

#include <QTest>
#include <QRegion>
#include <QTransform>
#include "private/wsgdamagetracker_p.h"

using Waylib::Server::WSGDamageTracker;

// Helper: generate stable NodeId from an integer for test readability.
static inline WSGDamageTracker::NodeId nid(int i)
{
    return reinterpret_cast<WSGDamageTracker::NodeId>(static_cast<quintptr>(i + 1));
}

class WSGDamageTrackerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();

    // --- Basic events ---
    void testNodeAdded();
    void testNodeRemoved();
    void testGeometryChanged();
    void testPureMove();
    void testContentDirtySubRegion();
    void testNestedSubtreeTransform();
    void testMultiEventAccumulation();
    void testResetFallback();
    void testTransformChanged();
    void testThreeLevelNestedTransform();
    void testScaleTransform();
    void testChildOwnTransformPlusParent();
    void testRemoveParentWithChildren();
};

void WSGDamageTrackerTest::init()
{
    // Each test creates its own tracker; nothing global to reset.
}

// add(rect) -> damage == rect
void WSGDamageTrackerTest::testNodeAdded()
{
    WSGDamageTracker tracker;
    tracker.setNodeParent(nid(0), nullptr);
    tracker.setNodeTransform(nid(0), QTransform());

    tracker.onNodeAdded(nid(0), QRectF(10, 20, 100, 50));

    QRegion damage = tracker.takeFrameDamage();
    QCOMPARE(damage, QRegion(10, 20, 100, 50));
    QCOMPARE(tracker.takeFrameDamage(), QRegion());
}

// add then remove -> damage == rect (old area)
void WSGDamageTrackerTest::testNodeRemoved()
{
    WSGDamageTracker tracker;
    tracker.setNodeParent(nid(0), nullptr);
    tracker.setNodeTransform(nid(0), QTransform());

    tracker.onNodeAdded(nid(0), QRectF(0, 0, 100, 100));
    tracker.takeFrameDamage(); // consume the add damage

    tracker.onNodeRemoved(nid(0));

    QRegion damage = tracker.takeFrameDamage();
    QCOMPARE(damage, QRegion(0, 0, 100, 100));
}

// add(A) then change to B -> damage == A | B
void WSGDamageTrackerTest::testGeometryChanged()
{
    WSGDamageTracker tracker;
    tracker.setNodeParent(nid(0), nullptr);
    tracker.setNodeTransform(nid(0), QTransform());

    QRectF a(0, 0, 50, 50);
    QRectF b(60, 0, 50, 50);

    tracker.onNodeAdded(nid(0), a);
    tracker.takeFrameDamage(); // consume add

    tracker.onGeometryChanged(nid(0), b);

    QRegion damage = tracker.takeFrameDamage();
    QRegion expected(a.toRect());
    expected |= b.toRect();
    QCOMPARE(damage, expected);
}

// rect from (0,0,10,10) to (20,0,10,10) -> damage == union of both
void WSGDamageTrackerTest::testPureMove()
{
    WSGDamageTracker tracker;
    tracker.setNodeParent(nid(0), nullptr);
    tracker.setNodeTransform(nid(0), QTransform());

    tracker.onNodeAdded(nid(0), QRectF(0, 0, 10, 10));
    tracker.takeFrameDamage();

    tracker.onGeometryChanged(nid(0), QRectF(20, 0, 10, 10));

    QRegion damage = tracker.takeFrameDamage();
    QRegion expected(0, 0, 10, 10);
    expected |= QRegion(20, 0, 10, 10);
    QCOMPARE(damage, expected);
}

// node rect=(0,0,100,100), contentDirty(region=(10,10,5,5)) -> damage == only (10,10,5,5)
void WSGDamageTrackerTest::testContentDirtySubRegion()
{
    WSGDamageTracker tracker;
    tracker.setNodeParent(nid(0), nullptr);
    tracker.setNodeTransform(nid(0), QTransform());

    tracker.onNodeAdded(nid(0), QRectF(0, 0, 100, 100));
    tracker.takeFrameDamage();

    tracker.onContentDirty(nid(0), QRegion(10, 10, 5, 5));

    QRegion damage = tracker.takeFrameDamage();
    QCOMPARE(damage, QRegion(10, 10, 5, 5));
}

// parent has translate transform, child contentDirty in child-local
// -> damage is child region mapped through parent transform
void WSGDamageTrackerTest::testNestedSubtreeTransform()
{
    WSGDamageTracker tracker;

    // Parent at root level with a translate (50, 30)
    tracker.setNodeParent(nid(0), nullptr);
    tracker.setNodeTransform(nid(0), QTransform::fromTranslate(50, 30));
    tracker.onNodeAdded(nid(0), QRectF(0, 0, 200, 200));
    tracker.takeFrameDamage();

    // Child under parent, identity transform, local content dirty at (10, 10, 5, 5)
    tracker.setNodeParent(nid(1), nid(0));
    tracker.setNodeTransform(nid(1), QTransform());
    tracker.onNodeAdded(nid(1), QRectF(0, 0, 50, 50));
    tracker.takeFrameDamage();

    tracker.onContentDirty(nid(1), QRegion(10, 10, 5, 5));

    QRegion damage = tracker.takeFrameDamage();
    // Child local (10,10,5,5) mapped through parent translate (50,30) = (60,40,5,5)
    QCOMPARE(damage, QRegion(60, 40, 5, 5));
}

// multiple events in one frame -> takeFrameDamage returns union, clears after
void WSGDamageTrackerTest::testMultiEventAccumulation()
{
    WSGDamageTracker tracker;
    tracker.setNodeParent(nid(0), nullptr);
    tracker.setNodeTransform(nid(0), QTransform());
    tracker.setNodeParent(nid(1), nullptr);
    tracker.setNodeTransform(nid(1), QTransform());

    tracker.onNodeAdded(nid(0), QRectF(0, 0, 30, 30));
    tracker.onNodeAdded(nid(1), QRectF(100, 100, 20, 20));
    tracker.onContentDirty(nid(0), QRegion(5, 5, 10, 10));

    QRegion damage = tracker.takeFrameDamage();
    QRegion expected(0, 0, 30, 30);
    expected |= QRegion(100, 100, 20, 20);
    expected |= QRegion(5, 5, 10, 10);
    QCOMPARE(damage, expected);

    // After take, accumulator is cleared
    QCOMPARE(tracker.takeFrameDamage(), QRegion());
}

// reset() then takeFrameDamage is empty -> caller should do add_whole
void WSGDamageTrackerTest::testResetFallback()
{
    WSGDamageTracker tracker;
    tracker.setNodeParent(nid(0), nullptr);
    tracker.setNodeTransform(nid(0), QTransform());
    tracker.onNodeAdded(nid(0), QRectF(0, 0, 100, 100));

    tracker.reset();

    QCOMPARE(tracker.takeFrameDamage(), QRegion());
}

// transform change -> old and new root-local rects damaged
void WSGDamageTrackerTest::testTransformChanged()
{
    WSGDamageTracker tracker;
    tracker.setNodeParent(nid(0), nullptr);
    tracker.setNodeTransform(nid(0), QTransform());

    tracker.onNodeAdded(nid(0), QRectF(0, 0, 50, 50));
    tracker.takeFrameDamage();

    // Change transform to translate (100, 0)
    tracker.onTransformChanged(nid(0), QTransform::fromTranslate(100, 0));

    QRegion damage = tracker.takeFrameDamage();
    // Old root-local: (0,0,50,50), new root-local: (100,0,50,50)
    QRegion expected(0, 0, 50, 50);
    expected |= QRegion(100, 0, 50, 50);
    QCOMPARE(damage, expected);
}

// 3-level nested: root→child→grandchild, each translate, verify accumulation
void WSGDamageTrackerTest::testThreeLevelNestedTransform()
{
    WSGDamageTracker tracker;

    // root: translate(10, 0)
    tracker.setNodeParent(nid(0), nullptr);
    tracker.setNodeTransform(nid(0), QTransform::fromTranslate(10, 0));
    tracker.onNodeAdded(nid(0), QRectF(0, 0, 100, 100));

    // child: translate(20, 0) under root
    tracker.setNodeParent(nid(1), nid(0));
    tracker.setNodeTransform(nid(1), QTransform::fromTranslate(20, 0));
    tracker.onNodeAdded(nid(1), QRectF(0, 0, 100, 100));

    // grandchild: translate(30, 0) under child
    tracker.setNodeParent(nid(2), nid(1));
    tracker.setNodeTransform(nid(2), QTransform::fromTranslate(30, 0));
    tracker.onNodeAdded(nid(2), QRectF(0, 0, 100, 100));
    tracker.takeFrameDamage();

    // Content dirty on grandchild at local (5, 5, 5, 5)
    // Mapped through: translate(30) * translate(20) * translate(10) = (65, 5, 5, 5)
    tracker.onContentDirty(nid(2), QRegion(5, 5, 5, 5));

    QRegion damage = tracker.takeFrameDamage();
    QCOMPARE(damage, QRegion(65, 5, 5, 5));
}

// Scale transform: fromScale(2,2), onContentDirty maps correctly
void WSGDamageTrackerTest::testScaleTransform()
{
    WSGDamageTracker tracker;

    tracker.setNodeParent(nid(0), nullptr);
    tracker.setNodeTransform(nid(0), QTransform::fromScale(2, 2));
    tracker.onNodeAdded(nid(0), QRectF(0, 0, 100, 100));
    tracker.takeFrameDamage();

    // local (5, 5, 5, 5) scaled by 2 → (10, 10, 10, 10)
    tracker.onContentDirty(nid(0), QRegion(5, 5, 5, 5));

    QRegion damage = tracker.takeFrameDamage();
    QCOMPARE(damage, QRegion(10, 10, 10, 10));
}

// Child has own non-identity transform + parent transform
void WSGDamageTrackerTest::testChildOwnTransformPlusParent()
{
    WSGDamageTracker tracker;

    // parent: translate(100, 0)
    tracker.setNodeParent(nid(0), nullptr);
    tracker.setNodeTransform(nid(0), QTransform::fromTranslate(100, 0));
    tracker.onNodeAdded(nid(0), QRectF(0, 0, 200, 200));

    // child: translate(10, 0) under parent
    tracker.setNodeParent(nid(1), nid(0));
    tracker.setNodeTransform(nid(1), QTransform::fromTranslate(10, 0));
    tracker.onNodeAdded(nid(1), QRectF(0, 0, 50, 50));
    tracker.takeFrameDamage();

    // Content dirty at child local (5, 5, 5, 5)
    // Mapped: translate(10) * translate(100) = (115, 5, 5, 5)
    tracker.onContentDirty(nid(1), QRegion(5, 5, 5, 5));

    QRegion damage = tracker.takeFrameDamage();
    QCOMPARE(damage, QRegion(115, 5, 5, 5));
}

// Remove parent with children: all descendants damaged and removed
void WSGDamageTrackerTest::testRemoveParentWithChildren()
{
    WSGDamageTracker tracker;

    // parent: translate(100, 0), rect (0,0,50,50) → root (100,0,50,50)
    tracker.setNodeParent(nid(0), nullptr);
    tracker.setNodeTransform(nid(0), QTransform::fromTranslate(100, 0));
    tracker.onNodeAdded(nid(0), QRectF(0, 0, 50, 50));

    // child: identity transform, rect (5,5,10,10) → root (105,5,10,10)
    tracker.setNodeParent(nid(1), nid(0));
    tracker.setNodeTransform(nid(1), QTransform());
    tracker.onNodeAdded(nid(1), QRectF(5, 5, 10, 10));
    tracker.takeFrameDamage();

    // Remove parent — should recursively remove child too
    tracker.onNodeRemoved(nid(0));

    QRegion damage = tracker.takeFrameDamage();
    // Both parent's and child's lastRootRect should be damaged
    QRegion expected(100, 0, 50, 50);
    expected |= QRegion(105, 5, 10, 10);
    QCOMPARE(damage, expected);

    // After removal, no damage from further events on removed child
    tracker.onContentDirty(nid(1), QRegion(0, 0, 10, 10));
    QCOMPARE(tracker.takeFrameDamage(), QRegion());
}

QTEST_MAIN(WSGDamageTrackerTest)
#include "tst_wsgdamagetracker.moc"
