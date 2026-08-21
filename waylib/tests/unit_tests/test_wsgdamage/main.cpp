// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsgimagenode_p.h"
#include "wsgdamagetracker_p.h"
#include "wsgdamagedebug_p.h"
#include "wsgcontext_p.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QProcess>
#include <QSGOpacityNode>
#include <QSGRootNode>
#include <QSGSimpleRectNode>
#include <QSGTransformNode>
#include <QTest>

WAYLIB_SERVER_USE_NAMESPACE

static QRect inflated(const QRectF &bounds)
{
    return bounds.toAlignedRect().adjusted(-1, -1, 1, 1);
}

static QRegion inflatedRegion(const QRegion &region)
{
    QRegion out;
    for (const QRect &rect : region)
        out += rect.adjusted(-1, -1, 1, 1);
    return out;
}

class WSGDamageTrackerTest : public QObject
{
    Q_OBJECT

private:
    QRegion takeFlush(WSGDamageTracker &tracker)
    {
        tracker.commit();
        return tracker.flushRegion();
    }

private Q_SLOTS:
    void addGeometry_reportsInflatedBounds();
    void moveGeometry_unionsOldAndNew();
    void transformMove_unionsOldAndNewSubtree();
    void transformMove_withoutCachedBounds_usesNewBounds();
    void transformMove_emptySubtree_withoutCache_isNotFull();
    void geometryMove_withoutCachedBounds_usesNewBounds();
    void materialWithoutImageNodeDamage_fullQuad();
    void materialWithImageNodeDamage_usesExplicitRegion();
    void materialWithEmptyDamageRegion_noContentDamage();
    void materialDamage_doesNotUnionPreviousContentDamage();
    void imageNodeAdd_usesFullGeometryNotJustDamageRegion();
    void imageNodeDamage_mapsThroughParentTransform();
    void opacityChange_subtreeBounds();
    void removeGeometry_usesLastBounds();
    void removeTransform_usesLastSubtreeBounds();
    void geometryChangeOnImageNode_usesFullQuad();
    void removeWithoutCache_usesLiveBounds();
    void removeTransformWithoutCache_usesLiveSubtree();
    void forceUpdate_usesSubtreeBounds();
    void forceUpdate_onEmptyRoot_isFull();
    void forceUpdate_thenMove_usesCachedBounds();
    void transformAddedEmpty_thenChild_thenMove();
    void transformAddedEmpty_thenMatrixWithChild_isNotFull();
    void nestedSubtreeAdded_thenChildMove();
    void childResize_thenParentMove_usesUpdatedBounds();
    void opacityChange_withoutCachedBounds_usesSubtree();
    void materialOnTransform_usesSubtree();
    void addRegion_unionsPending();
    void highlight_recordsContentUntilFade();
    void highlight_newerOverlapRemovesOlder();
    void highlight_expiredRegionStaysInExtra();
    void highlight_olderFrameIsGreener();
    void highlight_offMarksFullUntilSwapchainCycled();
    void capture_fullAlwaysRefreshes();
    void capture_dirtyInsideLastCapture_refreshes();
    void capture_selfQuad_skipped();
    void capture_backgroundLargerThanCapture_refreshes();
    void capture_otherButtonNoIntersect_skipped();
    void nodeDamage_followsAncestor();
    void nodeDamage_removedGoesToRemovedDamage();
    void addToFlush_unionsCommittedRegion();
};

void WSGDamageTrackerTest::addGeometry_reportsInflatedBounds()
{
    WSGDamageTracker tracker;
    QSGSimpleRectNode rect(QRectF(10, 20, 30, 40), Qt::red);

    tracker.nodeChanged(&rect, QSGNode::DirtyNodeAdded);

    QVERIFY(!tracker.flushRegionIsFull());
    QCOMPARE(takeFlush(tracker), QRegion(inflated(QRectF(10, 20, 30, 40))));
    QVERIFY(!tracker.flushRegionIsFull());
}

void WSGDamageTrackerTest::moveGeometry_unionsOldAndNew()
{
    WSGDamageTracker tracker;
    QSGSimpleRectNode rect(QRectF(10, 20, 30, 40), Qt::red);
    tracker.nodeChanged(&rect, QSGNode::DirtyNodeAdded);
    tracker.commit();

    rect.setRect(QRectF(100, 50, 30, 40));
    tracker.nodeChanged(&rect, QSGNode::DirtyGeometry);

    QRegion expected;
    expected += inflated(QRectF(10, 20, 30, 40));
    expected += inflated(QRectF(100, 50, 30, 40));
    QCOMPARE(takeFlush(tracker), expected);
    QVERIFY(!tracker.flushRegionIsFull());
}

void WSGDamageTrackerTest::transformMove_unionsOldAndNewSubtree()
{
    WSGDamageTracker tracker;
    QSGTransformNode transform;
    QSGSimpleRectNode rect(QRectF(10, 20, 30, 40), Qt::red);
    transform.appendChildNode(&rect);
    transform.setMatrix(QTransform::fromTranslate(5, 7));

    tracker.nodeChanged(&transform, QSGNode::DirtyNodeAdded);
    tracker.commit();

    transform.setMatrix(QTransform::fromTranslate(80, 90));
    tracker.nodeChanged(&transform, QSGNode::DirtyMatrix);

    QRegion expected;
    expected += inflated(QRectF(15, 27, 30, 40));
    expected += inflated(QRectF(90, 110, 30, 40));
    QCOMPARE(takeFlush(tracker), expected);
    QVERIFY(!tracker.flushRegionIsFull());
}

void WSGDamageTrackerTest::transformMove_withoutCachedBounds_usesNewBounds()
{
    // Never Added: this renderer has not drawn the node, so there is no old
    // position to erase. Only the current AABB is dirty.
    WSGDamageTracker tracker;
    QSGTransformNode transform;
    QSGSimpleRectNode rect(QRectF(10, 20, 30, 40), Qt::red);
    transform.appendChildNode(&rect);
    transform.setMatrix(QTransform::fromTranslate(80, 90));

    tracker.nodeChanged(&transform, QSGNode::DirtyMatrix);
    tracker.commit();

    QVERIFY(!tracker.flushRegionIsFull());
    QCOMPARE(tracker.flushRegion(), QRegion(inflated(QRectF(90, 110, 30, 40))));
}

void WSGDamageTrackerTest::transformMove_emptySubtree_withoutCache_isNotFull()
{
    // OutputLayer / hideSource: DirtyMatrix in this tree, no pixels here.
    WSGDamageTracker tracker;
    QSGTransformNode transform;
    transform.setMatrix(QTransform::fromTranslate(80, 90));

    tracker.nodeChanged(&transform, QSGNode::DirtyMatrix);
    tracker.commit();

    QVERIFY(!tracker.flushRegionIsFull());
    QVERIFY(tracker.flushRegion().isEmpty());
}

void WSGDamageTrackerTest::geometryMove_withoutCachedBounds_usesNewBounds()
{
    WSGDamageTracker tracker;
    QSGSimpleRectNode rect(QRectF(100, 50, 30, 40), Qt::red);

    tracker.nodeChanged(&rect, QSGNode::DirtyGeometry);
    tracker.commit();

    QVERIFY(!tracker.flushRegionIsFull());
    QCOMPARE(tracker.flushRegion(), QRegion(inflated(QRectF(100, 50, 30, 40))));
}

void WSGDamageTrackerTest::materialWithoutImageNodeDamage_fullQuad()
{
    WSGDamageTracker tracker;
    QSGSimpleRectNode rect(QRectF(0, 0, 200, 100), Qt::red);
    tracker.nodeChanged(&rect, QSGNode::DirtyNodeAdded);
    tracker.commit();

    tracker.nodeChanged(&rect, QSGNode::DirtyMaterial);

    QCOMPARE(takeFlush(tracker), QRegion(inflated(QRectF(0, 0, 200, 100))));
    QVERIFY(!tracker.flushRegionIsFull());
}

void WSGDamageTrackerTest::materialWithImageNodeDamage_usesExplicitRegion()
{
    WSGDamageTracker tracker;
    WSGImageNode image;
    image.setRect(QRectF(0, 0, 200, 100));
    image.setDamageRegion(QRegion(12, 8, 6, 4));

    tracker.nodeChanged(&image, QSGNode::DirtyNodeAdded);
    tracker.commit();

    tracker.nodeChanged(&image, QSGNode::DirtyMaterial);

    QCOMPARE(takeFlush(tracker), inflatedRegion(QRegion(12, 8, 6, 4)));
    QVERIFY(!tracker.flushRegionIsFull());
}

void WSGDamageTrackerTest::materialWithEmptyDamageRegion_noContentDamage()
{
    WSGDamageTracker tracker;
    WSGImageNode image;
    image.setRect(QRectF(0, 0, 200, 100));
    image.setDamageRegion(QRegion());

    tracker.nodeChanged(&image, QSGNode::DirtyNodeAdded);
    tracker.commit();

    tracker.nodeChanged(&image, QSGNode::DirtyMaterial);

    QVERIFY(takeFlush(tracker).isEmpty());
    QVERIFY(!tracker.flushRegionIsFull());
}

void WSGDamageTrackerTest::materialDamage_doesNotUnionPreviousContentDamage()
{
    WSGDamageTracker tracker;
    WSGImageNode image;
    image.setRect(QRectF(0, 0, 200, 100));
    image.setDamageRegion(QRegion(10, 10, 5, 5));

    tracker.nodeChanged(&image, QSGNode::DirtyNodeAdded);
    tracker.commit();

    tracker.nodeChanged(&image, QSGNode::DirtyMaterial);
    tracker.commit();

    image.setDamageRegion(QRegion(80, 60, 3, 3));
    tracker.nodeChanged(&image, QSGNode::DirtyMaterial);

    QCOMPARE(takeFlush(tracker), inflatedRegion(QRegion(80, 60, 3, 3)));
}

void WSGDamageTrackerTest::imageNodeAdd_usesFullGeometryNotJustDamageRegion()
{
    WSGDamageTracker tracker;
    WSGImageNode image;
    image.setRect(QRectF(0, 0, 200, 100));
    image.setDamageRegion(QRegion(1, 1, 2, 2));

    tracker.nodeChanged(&image, QSGNode::DirtyNodeAdded);

    QCOMPARE(takeFlush(tracker), QRegion(inflated(QRectF(0, 0, 200, 100))));
}

void WSGDamageTrackerTest::imageNodeDamage_mapsThroughParentTransform()
{
    WSGDamageTracker tracker;
    QSGTransformNode transform;
    WSGImageNode image;
    image.setRect(QRectF(0, 0, 200, 100));
    transform.setMatrix(QTransform::fromTranslate(100, 40));
    transform.appendChildNode(&image);
    image.setDamageRegion(QRegion(10, 10, 4, 4));

    tracker.nodeChanged(&transform, QSGNode::DirtyNodeAdded);
    tracker.nodeChanged(&image, QSGNode::DirtyNodeAdded);
    tracker.commit();

    tracker.nodeChanged(&image, QSGNode::DirtyMaterial);

    QCOMPARE(takeFlush(tracker), inflatedRegion(QRegion(110, 50, 4, 4)));
}

void WSGDamageTrackerTest::opacityChange_subtreeBounds()
{
    WSGDamageTracker tracker;
    QSGOpacityNode opacity;
    QSGSimpleRectNode rect(QRectF(4, 6, 10, 12), Qt::red);
    opacity.appendChildNode(&rect);

    tracker.nodeChanged(&opacity, QSGNode::DirtyNodeAdded);
    tracker.commit();

    opacity.setOpacity(0.5);
    tracker.nodeChanged(&opacity, QSGNode::DirtyOpacity);

    QCOMPARE(takeFlush(tracker), QRegion(inflated(QRectF(4, 6, 10, 12))));
}

void WSGDamageTrackerTest::removeGeometry_usesLastBounds()
{
    WSGDamageTracker tracker;
    QSGSimpleRectNode rect(QRectF(10, 20, 30, 40), Qt::red);
    tracker.nodeChanged(&rect, QSGNode::DirtyNodeAdded);
    tracker.commit();

    tracker.nodeChanged(&rect, QSGNode::DirtyNodeRemoved);

    QCOMPARE(takeFlush(tracker), QRegion(inflated(QRectF(10, 20, 30, 40))));
    QVERIFY(!tracker.flushRegionIsFull());
}

void WSGDamageTrackerTest::removeTransform_usesLastSubtreeBounds()
{
    WSGDamageTracker tracker;
    QSGTransformNode transform;
    QSGSimpleRectNode rect(QRectF(10, 20, 30, 40), Qt::red);
    transform.appendChildNode(&rect);
    transform.setMatrix(QTransform::fromTranslate(5, 7));

    tracker.nodeChanged(&transform, QSGNode::DirtyNodeAdded);
    tracker.commit();

    tracker.nodeChanged(&transform, QSGNode::DirtyNodeRemoved);

    QCOMPARE(takeFlush(tracker), QRegion(inflated(QRectF(15, 27, 30, 40))));
    QVERIFY(!tracker.flushRegionIsFull());
}

void WSGDamageTrackerTest::geometryChangeOnImageNode_usesFullQuad()
{
    WSGDamageTracker tracker;
    WSGImageNode image;
    image.setRect(QRectF(0, 0, 200, 100));
    image.setDamageRegion(QRegion(1, 1, 2, 2));

    tracker.nodeChanged(&image, QSGNode::DirtyNodeAdded);
    tracker.commit();

    image.setRect(QRectF(0, 0, 250, 120));
    tracker.nodeChanged(&image, QSGNode::DirtyGeometry);

    QRegion expected;
    expected += inflated(QRectF(0, 0, 200, 100));
    expected += inflated(QRectF(0, 0, 250, 120));
    QCOMPARE(takeFlush(tracker), expected);
    QVERIFY(!tracker.flushRegionIsFull());
}

void WSGDamageTrackerTest::removeWithoutCache_usesLiveBounds()
{
    WSGDamageTracker tracker;
    QSGSimpleRectNode rect(QRectF(10, 20, 30, 40), Qt::red);

    tracker.nodeChanged(&rect, QSGNode::DirtyNodeRemoved);

    QCOMPARE(takeFlush(tracker), QRegion(inflated(QRectF(10, 20, 30, 40))));
    QVERIFY(!tracker.flushRegionIsFull());
}

void WSGDamageTrackerTest::removeTransformWithoutCache_usesLiveSubtree()
{
    WSGDamageTracker tracker;
    QSGTransformNode transform;
    QSGSimpleRectNode rect(QRectF(10, 20, 30, 40), Qt::red);
    transform.appendChildNode(&rect);
    transform.setMatrix(QTransform::fromTranslate(5, 7));

    tracker.nodeChanged(&transform, QSGNode::DirtyNodeRemoved);

    QCOMPARE(takeFlush(tracker), QRegion(inflated(QRectF(15, 27, 30, 40))));
    QVERIFY(!tracker.flushRegionIsFull());
}

void WSGDamageTrackerTest::forceUpdate_usesSubtreeBounds()
{
    WSGDamageTracker tracker;
    QSGSimpleRectNode rect(QRectF(10, 20, 30, 40), Qt::red);
    tracker.nodeChanged(&rect, QSGNode::DirtyNodeAdded);
    tracker.commit();

    tracker.nodeChanged(&rect, QSGNode::DirtyForceUpdate);

    QCOMPARE(takeFlush(tracker), QRegion(inflated(QRectF(10, 20, 30, 40))));
    QVERIFY(!tracker.flushRegionIsFull());
}

void WSGDamageTrackerTest::forceUpdate_onEmptyRoot_isFull()
{
    WSGDamageTracker tracker;
    QSGRootNode root;

    tracker.nodeChanged(&root, QSGNode::DirtyForceUpdate);
    tracker.commit();

    QVERIFY(tracker.flushRegionIsFull());
}

void WSGDamageTrackerTest::forceUpdate_thenMove_usesCachedBounds()
{
    WSGDamageTracker tracker;
    QSGRootNode root;
    tracker.nodeChanged(&root, QSGNode::DirtyForceUpdate);
    QVERIFY(tracker.pendingIsFull());

    QSGSimpleRectNode rect(QRectF(10, 20, 30, 40), Qt::red);
    root.appendChildNode(&rect);
    tracker.nodeChanged(&rect, QSGNode::DirtyNodeAdded);
    tracker.commit();
    QVERIFY(tracker.flushRegionIsFull());

    rect.setRect(QRectF(80, 90, 30, 40));
    tracker.nodeChanged(&rect, QSGNode::DirtyGeometry);
    QVERIFY(!tracker.pendingIsFull());

    QRegion expected;
    expected += inflated(QRectF(10, 20, 30, 40));
    expected += inflated(QRectF(80, 90, 30, 40));
    QCOMPARE(takeFlush(tracker), expected);
}

void WSGDamageTrackerTest::transformAddedEmpty_thenChild_thenMove()
{
    WSGDamageTracker tracker;
    QSGTransformNode transform;
    tracker.nodeChanged(&transform, QSGNode::DirtyNodeAdded);

    QSGSimpleRectNode rect(QRectF(0, 0, 30, 40), Qt::red);
    transform.appendChildNode(&rect);
    tracker.nodeChanged(&rect, QSGNode::DirtyNodeAdded);
    tracker.commit();

    transform.setMatrix(QTransform::fromTranslate(80, 90));
    tracker.nodeChanged(&transform, QSGNode::DirtyMatrix);
    QVERIFY(!tracker.pendingIsFull());

    QRegion expected;
    expected += inflated(QRectF(0, 0, 30, 40));
    expected += inflated(QRectF(80, 90, 30, 40));
    QCOMPARE(takeFlush(tracker), expected);
}

void WSGDamageTrackerTest::transformAddedEmpty_thenMatrixWithChild_isNotFull()
{
    // QQuickOverlay: Added while hidden (empty AABB is still recorded).
    // Opening a Popup DirtyMatrix-es the overlay with the popup already
    // attached — Qt does not send another DirtyNodeAdded on the overlay.
    WSGDamageTracker tracker;
    QSGTransformNode overlay;
    tracker.nodeChanged(&overlay, QSGNode::DirtyNodeAdded);
    tracker.commit();

    QSGSimpleRectNode panel(QRectF(220, 250, 160, 80), Qt::green);
    overlay.appendChildNode(&panel);
    tracker.nodeChanged(&overlay, QSGNode::DirtyMatrix);
    tracker.commit();

    QVERIFY(!tracker.flushRegionIsFull());
    QCOMPARE(tracker.flushRegion(), QRegion(inflated(QRectF(220, 250, 160, 80))));
}

void WSGDamageTrackerTest::nestedSubtreeAdded_thenChildMove()
{
    // Qt setRootNode / parent insert only notifies the subtree root.
    WSGDamageTracker tracker;
    QSGRootNode root;
    QSGTransformNode parent;
    QSGTransformNode item;
    QSGSimpleRectNode rect(QRectF(0, 0, 30, 40), Qt::red);
    item.appendChildNode(&rect);
    parent.appendChildNode(&item);
    root.appendChildNode(&parent);

    tracker.nodeChanged(&root, QSGNode::DirtyNodeAdded);
    tracker.commit();

    item.setMatrix(QTransform::fromTranslate(80, 90));
    tracker.nodeChanged(&item, QSGNode::DirtyMatrix);
    QVERIFY(!tracker.pendingIsFull());

    QRegion expected;
    expected += inflated(QRectF(0, 0, 30, 40));
    expected += inflated(QRectF(80, 90, 30, 40));
    QCOMPARE(takeFlush(tracker), expected);
}

void WSGDamageTrackerTest::childResize_thenParentMove_usesUpdatedBounds()
{
    WSGDamageTracker tracker;
    QSGTransformNode parent;
    QSGSimpleRectNode child(QRectF(0, 0, 20, 20), Qt::red);
    parent.appendChildNode(&child);

    tracker.nodeChanged(&parent, QSGNode::DirtyNodeAdded);
    tracker.commit();

    child.setRect(QRectF(0, 0, 40, 20));
    tracker.nodeChanged(&child, QSGNode::DirtyGeometry);
    tracker.commit();

    parent.setMatrix(QTransform::fromTranslate(100, 0));
    tracker.nodeChanged(&parent, QSGNode::DirtyMatrix);

    QRegion expected;
    expected += inflated(QRectF(0, 0, 40, 20));
    expected += inflated(QRectF(100, 0, 40, 20));
    QCOMPARE(takeFlush(tracker), expected);
}

void WSGDamageTrackerTest::opacityChange_withoutCachedBounds_usesSubtree()
{
    WSGDamageTracker tracker;
    QSGOpacityNode opacity;
    QSGSimpleRectNode rect(QRectF(10, 20, 1, 18), Qt::red);
    opacity.appendChildNode(&rect);
    opacity.setOpacity(0);

    tracker.nodeChanged(&opacity, QSGNode::DirtyOpacity | QSGNode::DirtySubtreeBlocked);

    QCOMPARE(takeFlush(tracker), QRegion(inflated(QRectF(10, 20, 1, 18))));
    QVERIFY(!tracker.flushRegionIsFull());
}

void WSGDamageTrackerTest::materialOnTransform_usesSubtree()
{
    WSGDamageTracker tracker;
    QSGTransformNode transform;
    QSGSimpleRectNode rect(QRectF(10, 20, 30, 40), Qt::red);
    transform.appendChildNode(&rect);

    tracker.nodeChanged(&transform, QSGNode::DirtyMaterial);

    QCOMPARE(takeFlush(tracker), QRegion(inflated(QRectF(10, 20, 30, 40))));
    QVERIFY(!tracker.flushRegionIsFull());
}

void WSGDamageTrackerTest::addRegion_unionsPending()
{
    WSGDamageTracker tracker;
    tracker.addRegion(QRegion(QRect(10, 10, 20, 20)));
    tracker.addRegion(QRegion(QRect(100, 100, 5, 5)));

    QRegion expected;
    expected += QRect(10, 10, 20, 20);
    expected += QRect(100, 100, 5, 5);
    QCOMPARE(takeFlush(tracker), expected);
}

void WSGDamageTrackerTest::highlight_recordsContentUntilFade()
{
    WSGDamageDebug debug;
    debug.addFrame(QRegion(QRect(0, 0, 10, 10)), false, 0);
    QCOMPARE(debug.entries().size(), 1);
    QCOMPARE(debug.extraDamage(), QRegion(QRect(0, 0, 10, 10).adjusted(-2, -2, 2, 2)));
    QVERIFY(!debug.extraIsFull());

    debug.addFrame(QRegion(), false, WSGDamageDebug::fadeOutMs - 1);
    QCOMPARE(debug.entries().size(), 1);

    debug.addFrame(QRegion(), false, WSGDamageDebug::fadeOutMs);
    QVERIFY(debug.entries().isEmpty());
}

void WSGDamageTrackerTest::highlight_newerOverlapRemovesOlder()
{
    WSGDamageDebug debug;
    debug.addFrame(QRegion(QRect(0, 0, 20, 20)), false, 0);
    debug.addFrame(QRegion(QRect(0, 0, 10, 10)), false, 10);

    QCOMPARE(debug.entries().size(), 2);
    QCOMPARE(debug.entries().at(0).region, QRegion(QRect(0, 0, 10, 10)));
    QCOMPARE(debug.entries().at(1).region, QRegion(QRect(0, 0, 20, 20)) - QRect(0, 0, 10, 10));
}

void WSGDamageTrackerTest::highlight_expiredRegionStaysInExtra()
{
    WSGDamageDebug debug;
    debug.addFrame(QRegion(QRect(0, 0, 8, 8)), false, 0);
    debug.addFrame(QRegion(), false, WSGDamageDebug::fadeOutMs);

    QVERIFY(debug.entries().isEmpty());
    QCOMPARE(debug.extraDamage(), QRegion(QRect(0, 0, 8, 8).adjusted(-2, -2, 2, 2)));
}

void WSGDamageTrackerTest::highlight_olderFrameIsGreener()
{
    QList<WSGDamageDebug::Entry> entries;
    WSGDamageDebug::Entry newer;
    newer.region = QRegion(QRect(2, 2, 12, 12));
    newer.whenMs = 10;
    WSGDamageDebug::Entry older;
    older.region = QRegion(QRect(22, 2, 12, 12));
    older.whenMs = 0;
    entries << newer << older;

    QImage img(40, 20, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);
    QPainter painter(&img);
    WSGDamageDebug::paint(&painter, QTransform(), entries, 10);
    painter.end();

    const QRgb newPx = img.pixel(8, 8);
    const QRgb oldPx = img.pixel(28, 8);
    QVERIFY(qRed(newPx) > qGreen(newPx));
    QVERIFY(qGreen(oldPx) > qRed(oldPx));
    QVERIFY(qRed(newPx) > qRed(oldPx));
    QVERIFY(qGreen(oldPx) > qGreen(newPx));
}

void WSGDamageTrackerTest::highlight_offMarksFullUntilSwapchainCycled()
{
    const auto previous = WSGDamageDebug::mode();
    WSGDamageDebug::setMode(WSGDamageDebug::Mode::None);

    WSGDamageDebug debug;
    debug.addFrame(QRegion(QRect(0, 0, 10, 10)), false, 0);
    QVERIFY(debug.needsAnotherFrame());

    WSGDamageTracker first;
    debug.applyToTracker(&first);
    QVERIFY(first.pendingIsFull());
    QVERIFY(debug.entries().isEmpty());
    QVERIFY(debug.needsAnotherFrame());

    int fullFrames = 1;
    while (debug.needsAnotherFrame()) {
        WSGDamageTracker next;
        debug.applyToTracker(&next);
        QVERIFY(next.pendingIsFull());
        ++fullFrames;
        QVERIFY2(fullFrames <= 16, "None-mode swapchain erase did not stop");
    }
    QVERIFY(fullFrames >= 2);

    WSGDamageTracker idle;
    debug.applyToTracker(&idle);
    QVERIFY(!idle.pendingIsFull());
    QVERIFY(idle.pendingRegion().isEmpty());
    QVERIFY(!debug.needsAnotherFrame());
    WSGDamageDebug::setMode(previous);
}

void WSGDamageTrackerTest::capture_fullAlwaysRefreshes()
{
    QVERIFY(wsgFlushRequiresCapture(QRegion(), true, QRect(10, 10, 40, 40), QRect(10, 10, 40, 40)));
}

void WSGDamageTrackerTest::capture_dirtyInsideLastCapture_refreshes()
{
    const QRect capture(100, 200, 220, 30);
    const QRegion behind(QRect(108, 206, 40, 20));
    QVERIFY(wsgFlushRequiresCapture(behind, false, capture, capture));
}

void WSGDamageTrackerTest::capture_selfQuad_skipped()
{
    const QRect capture(100, 200, 220, 30);
    QVERIFY(!wsgFlushRequiresCapture(QRegion(capture), false, capture, capture));
}

void WSGDamageTrackerTest::capture_backgroundLargerThanCapture_refreshes()
{
    const QRect capture(100, 200, 220, 30);
    const QRegion wallpaper(QRect(0, 0, 800, 480));
    QVERIFY(wsgFlushRequiresCapture(wallpaper, false, capture, capture));
}

void WSGDamageTrackerTest::capture_otherButtonNoIntersect_skipped()
{
    const QRect capture(100, 200, 220, 30);
    const QRegion other(QRect(640, 420, 36, 36));
    QVERIFY(!wsgFlushRequiresCapture(other, false, capture, capture));
}

void WSGDamageTrackerTest::nodeDamage_followsAncestor()
{
    QSGTransformNode transform;
    QSGSimpleRectNode rect(QRectF(10, 20, 30, 40), Qt::red);
    transform.appendChildNode(&rect);

    WSGDamageTracker tracker;
    tracker.nodeChanged(&rect, QSGNode::DirtyNodeAdded);
    tracker.nodeChanged(&transform, QSGNode::DirtyNodeAdded);
    tracker.commit();
    tracker.clearNodeDamage();

    transform.setMatrix(QMatrix4x4());
    tracker.nodeChanged(&transform, QSGNode::DirtyMatrix);

    QVERIFY(!tracker.damageForNode(&rect).isEmpty());
    QVERIFY(tracker.damageForNode(&rect).contains(inflated(QRectF(10, 20, 30, 40))));
}

void WSGDamageTrackerTest::nodeDamage_removedGoesToRemovedDamage()
{
    QSGSimpleRectNode rect(QRectF(10, 20, 30, 40), Qt::red);
    WSGDamageTracker tracker;
    tracker.nodeChanged(&rect, QSGNode::DirtyNodeAdded);
    tracker.commit();
    tracker.clearNodeDamage();

    tracker.nodeChanged(&rect, QSGNode::DirtyNodeRemoved);
    QVERIFY(tracker.damageForNode(&rect).isEmpty());
    QVERIFY(tracker.removedDamage().contains(inflated(QRectF(10, 20, 30, 40))));
}

void WSGDamageTrackerTest::addToFlush_unionsCommittedRegion()
{
    QSGSimpleRectNode rect(QRectF(10, 20, 30, 40), Qt::red);
    WSGDamageTracker tracker;
    tracker.nodeChanged(&rect, QSGNode::DirtyNodeAdded);
    tracker.commit();

    const QRegion extra(QRect(200, 200, 16, 16));
    tracker.addToFlush(extra);
    QVERIFY(tracker.flushRegion().contains(inflated(QRectF(10, 20, 30, 40))));
    QVERIFY(tracker.flushRegion().contains(QRect(200, 200, 16, 16)));
}

int runDamageSceneTests(int argc, char **argv);

int runDamageTests(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    // Offscreen QPA has no RhiBasedRendering, so Qt would pick the software
    // adaptation. "rhi" forces QSGDefaultContext so WSGContext can install.
    qputenv("QT_QUICK_BACKEND", "rhi");
    if (!qEnvironmentVariableIsSet("QSG_RHI_BACKEND"))
        qputenv("QSG_RHI_BACKEND", "null");
    QGuiApplication app(argc, argv);
    WSGContext::ensureInstalled();

    int status = 0;
    {
        WSGDamageTrackerTest tracker;
        status |= QTest::qExec(&tracker, argc, argv);
    }
    status |= runDamageSceneTests(argc, argv);
    return status;
}

int main(int argc, char **argv)
{
    if (!qEnvironmentVariableIsSet("WSG_DAMAGE_TEST_CHILD")
        && !qEnvironmentVariableIsSet("QSG_RHI_BACKEND")) {
        QCoreApplication app(argc, argv);
        int status = 0;
        const QStringList args = app.arguments().mid(1);
        const char *backends[] = { "null", "opengl", "vulkan" };
        for (const char *backend : backends) {
            QProcess process;
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.insert(QStringLiteral("QSG_RHI_BACKEND"), QLatin1String(backend));
            env.insert(QStringLiteral("WSG_DAMAGE_TEST_CHILD"), QStringLiteral("1"));
            process.setProcessEnvironment(env);
            process.setProcessChannelMode(QProcess::ForwardedChannels);
            fprintf(stderr, "\n===== QSG_RHI_BACKEND=%s =====\n", backend);
            process.start(app.applicationFilePath(), args);
            if (!process.waitForStarted(15000) || !process.waitForFinished(-1)) {
                fprintf(stderr, "QSG_RHI_BACKEND=%s failed to run: %s\n",
                        backend, qPrintable(process.errorString()));
                return 1;
            }
            const int code = process.exitStatus() == QProcess::NormalExit ? process.exitCode() : 1;
            fprintf(stderr, "===== QSG_RHI_BACKEND=%s exit %d =====\n", backend, code);
            status |= code;
        }
        return status;
    }
    return runDamageTests(argc, argv);
}

#include "main.moc"
