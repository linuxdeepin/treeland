// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

// Unit tests for WSGDamageTracker.
//
// The damage-tracking algorithm is pure logic that operates on QSG node trees
// without a running compositor or render context. These tests construct QSG
// nodes directly, feed dirty-state through recordDirtyNode(), and verify the
// damage region returned by consumeDamageRegion().
//
// Coverage:
//   1. recordDirtyNode() accumulation (same node marked dirty multiple times)
//   2. consumeDamageRegion() computes region and clears the dirty set
//   3. Each DirtyDamageMask flag triggers damage (Geometry / Material / Opacity
//      / Matrix / NodeAdded / SubtreeBlocked / ForceUpdate)
//   4. Non-dirty nodes are skipped; empty dirty set returns empty region
//   5. mirrorVertically() Y-flip produces correct buffer-space damage

#include "wsgdamagetracker.h"

#include <QTest>
#include <QRegion>
#include <QTransform>
#include <QMatrix4x4>

#include <QtQuick/qsgnode.h>
#include <QtQuick/qsggeometry.h>

WAYLIB_SERVER_USE_NAMESPACE

// Custom node that always reports its subtree as blocked, independent of
// QSGOpacityNode::isSubtreeBlocked() which may behave differently across Qt versions.
class BlockedOpacityNode : public QSGOpacityNode {
public:
    bool isSubtreeBlocked() const override { return true; }
};

// Create a QSGGeometryNode whose vertices form the rectangle (x, y, w, h).
// The node owns its geometry so it is freed when the node is deleted.
static QSGGeometryNode *makeRectNode(qreal x, qreal y, qreal w, qreal h)
{
    auto *node = new QSGGeometryNode;
    auto *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 4);
    auto *pts = geometry->vertexDataAsPoint2D();
    pts[0].set(static_cast<float>(x),         static_cast<float>(y));
    pts[1].set(static_cast<float>(x + w),     static_cast<float>(y));
    pts[2].set(static_cast<float>(x),         static_cast<float>(y + h));
    pts[3].set(static_cast<float>(x + w),     static_cast<float>(y + h));
    node->setGeometry(geometry);
    node->setFlag(QSGNode::OwnsGeometry);
    return node;
}

class WSGDamageTrackerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase() { }

    // --- 1. recordDirtyNode accumulation ---

    void testRecordDirtyNodeAccumulation()
    {
        // Marking the same node twice should accumulate dirty bits.
        QSGRootNode root;
        auto *geo = makeRectNode(10, 10, 20, 20);
        root.appendChildNode(geo);

        WSGDamageTracker tracker;
        QVERIFY(!tracker.hasDirtyNodes());

        tracker.recordDirtyNode(geo, QSGNode::DirtyGeometry);
        QVERIFY(tracker.hasDirtyNodes());

        tracker.recordDirtyNode(geo, QSGNode::DirtyMaterial);
        // Still one entry, but with combined flags.
        QVERIFY(tracker.hasDirtyNodes());

        // Consuming should produce damage (DirtyGeometry is in the mask).
        QRegion damage = tracker.consumeDamageRegion(&root);
        QVERIFY(damage.contains(QRect(10, 10, 20, 20)));
        QVERIFY(!tracker.hasDirtyNodes());

        delete geo;
    }

    void testRecordDirtyNodeIgnoresNull()
    {
        WSGDamageTracker tracker;
        tracker.recordDirtyNode(nullptr, QSGNode::DirtyGeometry);
        QVERIFY(!tracker.hasDirtyNodes());
    }

    // --- 2. consumeDamageRegion clears the dirty set ---

    void testConsumeClearsDirtySet()
    {
        QSGRootNode root;
        auto *geo = makeRectNode(0, 0, 10, 10);
        root.appendChildNode(geo);

        WSGDamageTracker tracker;
        tracker.recordDirtyNode(geo, QSGNode::DirtyGeometry);

        QRegion first = tracker.consumeDamageRegion(&root);
        QVERIFY(!first.isEmpty());

        // Second consume with no new dirty nodes returns empty.
        QRegion second = tracker.consumeDamageRegion(&root);
        QVERIFY(second.isEmpty());
        QVERIFY(!tracker.hasDirtyNodes());

        delete geo;
    }

    void testResetClearsDirtySet()
    {
        QSGRootNode root;
        auto *geo = makeRectNode(0, 0, 10, 10);
        root.appendChildNode(geo);

        WSGDamageTracker tracker;
        tracker.recordDirtyNode(geo, QSGNode::DirtyGeometry);
        QVERIFY(tracker.hasDirtyNodes());

        tracker.reset();
        QVERIFY(!tracker.hasDirtyNodes());

        QRegion damage = tracker.consumeDamageRegion(&root);
        QVERIFY(damage.isEmpty());

        delete geo;
    }

    // --- 3. Each DirtyDamageMask flag triggers damage ---

    void testDirtyGeometry()
    {
        QSGRootNode root;
        auto *geo = makeRectNode(10, 10, 20, 20);
        root.appendChildNode(geo);

        WSGDamageTracker tracker;
        tracker.recordDirtyNode(geo, QSGNode::DirtyGeometry);
        QRegion damage = tracker.consumeDamageRegion(&root);
        QVERIFY(damage.contains(QRect(10, 10, 20, 20)));

        delete geo;
    }

    void testDirtyMaterial()
    {
        QSGRootNode root;
        auto *geo = makeRectNode(10, 10, 20, 20);
        root.appendChildNode(geo);

        WSGDamageTracker tracker;
        tracker.recordDirtyNode(geo, QSGNode::DirtyMaterial);
        QRegion damage = tracker.consumeDamageRegion(&root);
        QVERIFY(damage.contains(QRect(10, 10, 20, 20)));

        delete geo;
    }

    void testDirtyOpacity()
    {
        // DirtyOpacity on an OpacityNode should propagate damage to its
        // (non-dirty) geometry child.
        QSGRootNode root;
        auto *opacity = new QSGOpacityNode;
        auto *geo = makeRectNode(10, 10, 20, 20);
        root.appendChildNode(opacity);
        opacity->appendChildNode(geo);

        WSGDamageTracker tracker;
        tracker.recordDirtyNode(opacity, QSGNode::DirtyOpacity);
        QRegion damage = tracker.consumeDamageRegion(&root);
        QVERIFY(damage.contains(QRect(10, 10, 20, 20)));

        delete opacity;
    }

    void testDirtyMatrix()
    {
        // DirtyMatrix on a TransformNode should propagate damage to its
        // (non-dirty) geometry child, with the child's bounding box mapped
        // through the transform.
        QSGRootNode root;
        auto *transform = new QSGTransformNode;
        QMatrix4x4 matrix;
        matrix.translate(100, 100);
        transform->setMatrix(matrix);
        auto *geo = makeRectNode(10, 10, 20, 20);
        root.appendChildNode(transform);
        transform->appendChildNode(geo);

        WSGDamageTracker tracker;
        tracker.recordDirtyNode(transform, QSGNode::DirtyMatrix);
        QRegion damage = tracker.consumeDamageRegion(&root);
        // (10,10) + translate(100,100) = (110,110)
        QVERIFY(damage.contains(QRect(110, 110, 20, 20)));

        delete transform;
    }

    void testDirtyNodeAdded()
    {
        QSGRootNode root;
        auto *geo = makeRectNode(5, 5, 15, 15);
        root.appendChildNode(geo);

        WSGDamageTracker tracker;
        tracker.recordDirtyNode(geo, QSGNode::DirtyNodeAdded);
        QRegion damage = tracker.consumeDamageRegion(&root);
        QVERIFY(damage.contains(QRect(5, 5, 15, 15)));

        delete geo;
    }

    void testDirtySubtreeBlocked()
    {
        // DirtySubtreeBlocked on a non-blocked GeometryNode should produce
        // damage (the flag is in the mask; the node is not actually blocked).
        QSGRootNode root;
        auto *geo = makeRectNode(10, 10, 20, 20);
        root.appendChildNode(geo);

        WSGDamageTracker tracker;
        tracker.recordDirtyNode(geo, QSGNode::DirtySubtreeBlocked);
        QRegion damage = tracker.consumeDamageRegion(&root);
        QVERIFY(damage.contains(QRect(10, 10, 20, 20)));

        delete geo;
    }

    void testDirtyForceUpdate()
    {
        QSGRootNode root;
        auto *geo = makeRectNode(10, 10, 20, 20);
        root.appendChildNode(geo);

        WSGDamageTracker tracker;
        tracker.recordDirtyNode(geo, QSGNode::DirtyForceUpdate);
        QRegion damage = tracker.consumeDamageRegion(&root);
        QVERIFY(damage.contains(QRect(10, 10, 20, 20)));

        delete geo;
    }

    void testDirtyNodeRemovedNotInMask()
    {
        // DirtyNodeRemoved is intentionally NOT in DirtyDamageMask, so a node
        // marked only with this flag should not produce damage.
        QSGRootNode root;
        auto *geo = makeRectNode(10, 10, 20, 20);
        root.appendChildNode(geo);

        WSGDamageTracker tracker;
        tracker.recordDirtyNode(geo, QSGNode::DirtyNodeRemoved);
        QRegion damage = tracker.consumeDamageRegion(&root);
        QVERIFY(damage.isEmpty());

        delete geo;
    }

    // --- 4. Non-dirty nodes skipped; empty dirty set ---

    void testNonDirtyNodeSkipped()
    {
        QSGRootNode root;
        auto *geo = makeRectNode(10, 10, 20, 20);
        root.appendChildNode(geo);

        WSGDamageTracker tracker;
        // No dirty nodes recorded.
        QRegion damage = tracker.consumeDamageRegion(&root);
        QVERIFY(damage.isEmpty());

        delete geo;
    }

    void testEmptyDirtySet()
    {
        QSGRootNode root;
        auto *geo = makeRectNode(10, 10, 20, 20);
        root.appendChildNode(geo);

        WSGDamageTracker tracker;
        tracker.recordDirtyNode(geo, QSGNode::DirtyGeometry);
        tracker.reset();

        QRegion damage = tracker.consumeDamageRegion(&root);
        QVERIFY(damage.isEmpty());

        delete geo;
    }

    void testBlockedSubtreeSkipsChildren()
    {
        // A blocked subtree's children should not be traversed even if they
        // are dirty. This documents the known limitation: the former visible
        // area of a now-hidden subtree is not damaged.
        QSGRootNode root;
        auto *opacity = new BlockedOpacityNode;
        QVERIFY(opacity->isSubtreeBlocked());
        auto *geo = makeRectNode(10, 10, 20, 20);
        root.appendChildNode(opacity);
        opacity->appendChildNode(geo);

        WSGDamageTracker tracker;
        tracker.recordDirtyNode(geo, QSGNode::DirtyGeometry);
        QRegion damage = tracker.consumeDamageRegion(&root);
        QVERIFY(damage.isEmpty());

        delete opacity;
    }

    void testNestedTransforms()
    {
        // Nested TransformNodes should accumulate their matrices so the
        // child's bounding box is mapped through the combined transform.
        QSGRootNode root;
        auto *outer = new QSGTransformNode;
        QMatrix4x4 outerMatrix;
        outerMatrix.translate(50, 0);
        outer->setMatrix(outerMatrix);

        auto *inner = new QSGTransformNode;
        QMatrix4x4 innerMatrix;
        innerMatrix.translate(0, 50);
        inner->setMatrix(innerMatrix);

        auto *geo = makeRectNode(10, 10, 20, 20);
        root.appendChildNode(outer);
        outer->appendChildNode(inner);
        inner->appendChildNode(geo);

        WSGDamageTracker tracker;
        tracker.recordDirtyNode(outer, QSGNode::DirtyMatrix);
        QRegion damage = tracker.consumeDamageRegion(&root);
        // (10,10) + translate(50,0) + translate(0,50) = (60,60)
        QVERIFY(damage.contains(QRect(60, 60, 20, 20)));

        delete outer;
    }

    // --- 5. mirrorVertically() Y-flip correctness ---

    void testMirrorVerticallyYFlip()
    {
        // Build a damage region via WSGDamageTracker (root-node local coords),
        // convert to buffer pixel space with a simple identity toPixel transform,
        // then apply the same Y-flip used in WBufferRenderer::render() when
        // mirrorVertically() is true. Verify the flipped region is correct.
        QSGRootNode root;
        auto *geo = makeRectNode(10, 10, 20, 20);
        root.appendChildNode(geo);

        WSGDamageTracker tracker;
        tracker.recordDirtyNode(geo, QSGNode::DirtyGeometry);
        QRegion logicalDamage = tracker.consumeDamageRegion(&root);
        QCOMPARE(logicalDamage, QRegion(QRect(10, 10, 20, 20)));

        // Identity toPixel transform (DPR=1, identity world transform, full
        // source/target rect). Buffer height = 100.
        const int bufferHeight = 100;
        QTransform toPixel = QTransform::fromScale(1.0, 1.0);
        QRegion pixelDamage = toPixel.map(logicalDamage);
        pixelDamage &= QRect(0, 0, 100, bufferHeight);

        // Apply Y-flip (same logic as wbufferrenderer.cpp).
        QTransform flipY;
        flipY.scale(1, -1);
        flipY.translate(0, -bufferHeight);
        QRegion flippedDamage = flipY.map(pixelDamage);

        // (10, 10, 20, 20) with height 100 flips to (10, 70, 20, 20):
        // y=10 -> 100-10-20=70, height stays 20.
        QVERIFY(flippedDamage.contains(QRect(10, 70, 20, 20)));
        QVERIFY(!flippedDamage.contains(QRect(10, 10, 20, 20)));

        delete geo;
    }

    void testMirrorVerticallyYFlipRoundTrip()
    {
        // Flipping twice should restore the original region.
        const int bufferHeight = 200;
        QRegion original(QRect(5, 10, 15, 25));

        QTransform flipY;
        flipY.scale(1, -1);
        flipY.translate(0, -bufferHeight);

        QRegion flipped = flipY.map(original);
        QRegion roundTripped = flipY.map(flipped);

        QCOMPARE(roundTripped, original);
    }
};

QTEST_MAIN(WSGDamageTrackerTest)
#include "main.moc"
