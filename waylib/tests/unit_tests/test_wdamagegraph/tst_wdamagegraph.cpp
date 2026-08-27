// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsgdamagetracker_p.h"

WAYLIB_SERVER_USE_NAMESPACE

#include <QMatrix4x4>
#include <QRegion>
#include <QTest>
#include <QVector>

#include <utility>

static QRegion toQRegion(const pixman_region32_t *region)
{
    QRegion result;
    if (!region)
        return result;
    int count = 0;
    const pixman_box32_t *boxes = pixman_region32_rectangles(region, &count);
    for (int i = 0; i < count; ++i) {
        result +=
            QRect(boxes[i].x1, boxes[i].y1, boxes[i].x2 - boxes[i].x1, boxes[i].y2 - boxes[i].y1);
    }
    return result;
}

static QRegion toQRegion(const WPixmanRegion &region)
{
    return toQRegion(region.native());
}

static QRegion toQRegion(const QRegion &region)
{
    return region;
}

static QString regionStr(const QRegion &r)
{
    if (r.isEmpty())
        return QStringLiteral("<empty>");
    QString s;
    for (const QRect &rect : r) {
        if (!s.isEmpty())
            s += QLatin1Char(' ');
        s += QStringLiteral("[%1,%2 %3x%4]")
                 .arg(rect.x())
                 .arg(rect.y())
                 .arg(rect.width())
                 .arg(rect.height());
    }
    return s;
}

#define COMPARE_REGION(actual, expected)                                                \
    do {                                                                                \
        const QRegion _gdtActual = toQRegion(actual);                                   \
        const QRegion _gdtExpected = toQRegion(expected);                               \
        QVERIFY2((_gdtActual ^ _gdtExpected).isEmpty(),                                 \
                 qPrintable(QStringLiteral("actual=%1 expected=%2")                     \
                                .arg(regionStr(_gdtActual), regionStr(_gdtExpected)))); \
    } while (0)

class TestTracker : public WSGDamageTracker
{
public:
    using WSGDamageTracker::WSGDamageTracker;

    QRegion commit()
    {
        WSGDamageTracker::commit(viewport);
        const QRegion result = viewport.damageRegion().region.toQRegion();
        finishFrame();
        viewport.finishFrame();
        return result;
    }

    WSGViewport viewport;
};

class WSGDamageGraphTest : public QObject
{
    Q_OBJECT

private slots:
    void regionBooleanParity();
    void regionCopyMoveAndNativeHandle();
    void regionTransformMapping();
    void regionMatrixMapping();
    void regionTransformParityWithQRegion();
    void emptyCommit();
    void addGeometry();
    void idempotentCommit();
    void settleWithoutViewportDamage();
    void fullViewportSkipsAccumulation();
    void removeGeometry();
    void addRemoveSameFrame();
    void moveGeometryRect();
    void resizeGeometry();
    void partialContentDamage();
    void contentDamageClippedToBounds();
    void clipIntersectsGeometryBounds();
    void clipScrollKeepsUnclippedLocalBounds();
    void clipResizeDamagesCrescent();
    void clipDoesNotOccludeOutside();
    void nestedClips();
    void emptyClipHidesContent();
    void clippedSubtreeDefersChanges();
    void contentDirtyClippedToClipNode();
    void clipValidAndVisibleFollowClip();
    void clipBackdropVisibleStaysInsideClip();
    void nonRectangularClipClipsBoundsWithoutOpaque();
    void roundedClipInnerOmitsCorners();
    void waylandOpaqueRegionPunchesBehind();
    void hideShow();
    void hiddenSubtreeDefersChanges();
    void aggregateProxyIsOrdinaryGeometry();
    void hideShowSameFrame();
    void hideThenRemoveWithoutCommit();
    void translateTransform();
    void nestedTransforms();
    void scaleTransform();
    void rotate90();
    void rotate45Conservative();
    void siblingOcclusionFullyCovered();
    void siblingOcclusionPartial();
    void transparentDoesNotOcclude();
    void parentGeometryBehindChildren();
    void occludedRegionReported();
    void insertBetweenSiblings();
    void reparent();
    void raiseLowersZOrder();
    void fullyOpaqueFlag();
    void opaqueRegionUpdate();
    void contentLocalDirtyFollowsBox();
    void multipleDirtyRegions();
    void destroySubtree();
    void basicNodeGrouping();
    void parentHideHidesChildren();
    void fractionalTranslationOverestimates();
    void moveOpaqueRevealsBehind();
    void movingFrontDoesNotDamageCleanBackWhole();
    void contentDamageUnderOpaqueSibling();
    void partiallyOccludedMoveAvoidsOpaqueFront();
    void fullyOccludedMoveProducesNoDamage();
    void zeroSizeGeometry();
    void negativeCoordinates();
    void deepTree();
    void setMatrixIdentityNoDamage();
    void geometryChildrenZOrder();
    void nonAxisAlignedDropsOpaque();
    void matrix4x4();
    void firstCommitAppearing();
    void contentDirtyOnNewNode();
    void removeUncommittedNode();
    void siblingOrderPaint();
    void nodeAccessorsUseSceneCoordinates();
    void nodeHasContentProperty();
    void worldVisiblePartialOpaqueRegion();
    void worldVisibleTwoOpaqueFronts();
    void worldVisibleLocalizedSubtreeComposition();
    void worldVisibleRaiseReveals();
    void worldVisibleHasContentOff();
    void worldVisibleParentHideClearsChild();
    void worldVisibleUnderTranslation();
    void worldVisibleNestedAndScale();
    void worldVisibleOverlappingOpaqueNodes();
    void worldVisibleNonAxisAlignedFront();
    void worldVisibleAfterGeometryMove();
    void worldVisibleZeroSizeEmpty();
    void worldVisibleRemoveFront();
    void worldVisibleIdempotentSecondCommit();
    void worldVisibleGroupHasNone();
    void worldVisiblePartialChildCover();
    void worldVisibleDeepNested();
    void worldVisibleInsertAndReparent();
    void worldVisibleHideShow();
    void worldVisibleNegativeCoords();
    void worldVisibleRotate90();
    void backdropBehindDamageMatchesAccumulator();
    void backdropUncoveredRegion();
    void backdropKeepsCoveredBehindDamage();
    void backdropPunchesFrontOpaque();
    void backdropRecopyClippedToSampleBounds();
    void stackedBackdropsVisibleRegions();
    void stackedBackdropsRecaptureOverlap();
    void stackedBackdropsExclusiveDirtySkipsFront();
    void stackedBackdropsIndependentCopySources();
    void backdropSeesRemovedSiblingHole();
    void backgroundDamageRecopySurvivesRepeatedCommit();
    void multiOutputDisjointSceneMapping();
    void highDpiScaledOutputMapping();
    void rotatedOutputMapping();
    void singularOutputMapping();
};

void WSGDamageGraphTest::regionBooleanParity()
{
    WPixmanRegion a;
    a += QRect(-10, -5, 20, 10);
    a += QRect(20, 5, 8, 12);
    WPixmanRegion b;
    b += QRect(0, -8, 24, 20);
    b += QRect(25, 10, 8, 8);

    const QRegion qa = QRegion(-10, -5, 20, 10) + QRegion(20, 5, 8, 12);
    const QRegion qb = QRegion(0, -8, 24, 20) + QRegion(25, 10, 8, 8);

    COMPARE_REGION(a + b, qa + qb);
    COMPARE_REGION(a & b, qa & qb);
    COMPARE_REGION(a - b, qa - qb);
    QCOMPARE(a.rectCount(), qa.rectCount());
    QCOMPARE(a.boundingRect(), qa.boundingRect());
}

void WSGDamageGraphTest::regionCopyMoveAndNativeHandle()
{
    WPixmanRegion original;
    original += QRect(-20, 3, 7, 9);
    original += QRect(4, 8, 11, 6);

    WPixmanRegion copy(original);
    COMPARE_REGION(copy, original);

    WPixmanRegion moved(std::move(copy));
    COMPARE_REGION(moved, original);
    COMPARE_REGION(copy, QRegion());
    QVERIFY(pixman_region32_equal(moved.native(), original.native()));

    WPixmanRegion assigned;
    assigned = moved;
    COMPARE_REGION(assigned, original);

    WPixmanRegion moveAssigned;
    moveAssigned = std::move(assigned);
    COMPARE_REGION(moveAssigned, original);
    COMPARE_REGION(assigned, QRegion());
}

void WSGDamageGraphTest::regionTransformMapping()
{
    WPixmanRegion region;
    region += QRect(0, 0, 4, 3);
    region += QRect(8, 2, 2, 2);

    COMPARE_REGION(region.mappedOuter(QTransform()), region);
    COMPARE_REGION(region.mappedInner(QTransform()), region);

    const QTransform translation = QTransform::fromTranslate(3, -2);
    const QRegion translated = QRegion(3, -2, 4, 3) + QRegion(11, 0, 2, 2);
    COMPARE_REGION(region.mappedOuter(translation), translated);
    COMPARE_REGION(region.mappedInner(translation), translated);

    const QTransform fractional = QTransform::fromTranslate(0.25, -0.5);
    COMPARE_REGION(region.mappedOuter(fractional), QRegion(0, -1, 5, 4) + QRegion(8, 1, 3, 3));
    COMPARE_REGION(region.mappedInner(fractional), QRegion(1, 0, 3, 2) + QRegion(9, 2, 1, 1));

    QTransform mirrored;
    mirrored.translate(20, 10);
    mirrored.scale(-2, 3);
    COMPARE_REGION(region.mappedOuter(mirrored), QRegion(12, 10, 8, 9) + QRegion(0, 16, 4, 6));
    COMPARE_REGION(region.mappedInner(mirrored), QRegion(12, 10, 8, 9) + QRegion(0, 16, 4, 6));

    QTransform quarterTurn;
    quarterTurn.rotate(90);
    COMPARE_REGION(region.mappedOuter(quarterTurn), QRegion(-3, 0, 3, 4) + QRegion(-4, 8, 2, 2));
    COMPARE_REGION(region.mappedInner(quarterTurn), QRegion(-3, 0, 3, 4) + QRegion(-4, 8, 2, 2));

    QTransform arbitrary;
    arbitrary.rotate(33);
    const WPixmanRegion arbitraryOuter = region.mappedOuter(arbitrary);
    COMPARE_REGION(arbitraryOuter,
                   QRegion(mapOuter(arbitrary, QRectF(0, 0, 4, 3)))
                       + QRegion(mapOuter(arbitrary, QRectF(8, 2, 2, 2))));
    QVERIFY(region.mappedInner(arbitrary).isEmpty());
}

void WSGDamageGraphTest::regionMatrixMapping()
{
    WPixmanRegion region;
    region += QRect(-3, 2, 7, 5);
    region += QRect(10, -4, 3, 6);

    QMatrix4x4 matrix;
    matrix.translate(3.25f, -1.5f);
    matrix.scale(1.5f, 0.5f);

    const QTransform transform = matrix.toTransform();
    COMPARE_REGION(region.mappedOuter(matrix), region.mappedOuter(transform));
    COMPARE_REGION(region.mappedInner(matrix), region.mappedInner(transform));

    matrix.setToIdentity();
    matrix.rotate(90, 0, 0, 1);
    COMPARE_REGION(region.mappedOuter(matrix), region.mappedOuter(matrix.toTransform()));
    COMPARE_REGION(region.mappedInner(matrix), region.mappedInner(matrix.toTransform()));
}

void WSGDamageGraphTest::regionTransformParityWithQRegion()
{
    WPixmanRegion region;
    region += QRect(-5, -7, 6, 9);
    region += QRect(0, 0, 4, 3);
    region += QRect(8, 2, 2, 2);
    const QRegion qtRegion = region.toQRegion();
    COMPARE_REGION(region, qtRegion);

    QList<QTransform> exactTransforms{
        QTransform(),
        QTransform::fromTranslate(3, -2),
        QTransform::fromScale(2, 3),
        QTransform::fromScale(-2, 1),
    };
    QTransform quarterTurn;
    quarterTurn.rotate(90);
    exactTransforms.append(quarterTurn);
    QTransform halfTurn;
    halfTurn.rotate(180);
    exactTransforms.append(halfTurn);
    for (const QTransform &t : exactTransforms) {
        const QRegion expected = t.map(qtRegion);
        COMPARE_REGION(region.mappedOuter(t), expected);
        COMPARE_REGION(region.mappedInner(t), expected);
    }

    // Fractional/rotated transforms: Qt rounds to nearest; damage is an
    // over-approximation and opaque an under-approximation of it.
    QList<QTransform> fuzzyTransforms{
        QTransform::fromTranslate(0.25, -0.5),
        QTransform::fromScale(1.5, 0.5),
        quarterTurn * QTransform::fromScale(1.5, 2.0),
    };
    QTransform arbitrary;
    arbitrary.rotate(33);
    for (const QTransform &t : fuzzyTransforms) {
        const QRegion qtMapped = t.map(qtRegion);
        QVERIFY2((qtMapped - toQRegion(region.mappedOuter(t))).isEmpty(),
                 qPrintable(
                     QStringLiteral("damage %1 misses Qt %2")
                         .arg(regionStr(region.mappedOuter(t).toQRegion()), regionStr(qtMapped))));
        QVERIFY2((toQRegion(region.mappedInner(t)) - qtMapped).isEmpty(),
                 qPrintable(
                     QStringLiteral("opaque %1 escapes Qt %2")
                         .arg(regionStr(region.mappedInner(t).toQRegion()), regionStr(qtMapped))));
    }

    // QMatrix4x4 has no map(QRegion); it must match its 2D projection and
    // cover the mapped bounding rect.
    QMatrix4x4 matrix;
    matrix.translate(3.25f, -1.5f);
    matrix.rotate(90, 0, 0, 1);
    matrix.scale(1.5f, 0.5f);
    COMPARE_REGION(region.mappedOuter(matrix), region.mappedOuter(matrix.toTransform()));
    COMPARE_REGION(region.mappedInner(matrix), region.mappedInner(matrix.toTransform()));
    QVERIFY((qtRegion.intersects(matrix.mapRect(region.boundingRect()))));
    QVERIFY(
        (toQRegion(region.mappedOuter(matrix)) - matrix.mapRect(region.boundingRect())).isEmpty());
}

void WSGDamageGraphTest::emptyCommit()
{
    WSGDamageNode root;
    TestTracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion());
}

void WSGDamageGraphTest::addGeometry()
{
    WSGDamageNode root;
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(10, 20, 30, 40));
    root.appendChild(g);

    TestTracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion(10, 20, 30, 40));
    QCOMPARE(g->worldBounds(), QRect(10, 20, 30, 40));
}

void WSGDamageGraphTest::idempotentCommit()
{
    WSGDamageNode root;
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(0, 0, 50, 50));
    root.appendChild(g);

    TestTracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion(0, 0, 50, 50));
    COMPARE_REGION(tracker.commit(), QRegion());
    COMPARE_REGION(tracker.commit(), QRegion());
}

void WSGDamageGraphTest::settleWithoutViewportDamage()
{
    WSGDamageNode root;
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(10, 20, 30, 40));
    root.appendChild(g);

    TestTracker tracker(&root);
    QVERIFY(g->isDirty());
    tracker.finishFrame();
    QVERIFY(!g->isDirty());
    QCOMPARE(g->worldBounds(), QRect(10, 20, 30, 40));
    QVERIFY(tracker.viewport.damageRegion().region.isEmpty());
    QVERIFY(!tracker.viewport.isFull());
    COMPARE_REGION(tracker.commit(), QRegion());
}

void WSGDamageGraphTest::fullViewportSkipsAccumulation()
{
    WSGDamageNode root;
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(10, 20, 30, 40));
    root.appendChild(g);

    WSGViewport viewport;
    viewport.markFull();
    WSGDamageTracker tracker(&root);
    tracker.commit(viewport);

    QVERIFY(viewport.isFull());
    QVERIFY(viewport.damageRegion().region.isEmpty());
    tracker.finishFrame();
    QVERIFY(viewport.isFull());

    // A skipped full viewport still establishes the scene baseline at frame end.
    viewport.finishFrame();
    tracker.commit(viewport);
    QVERIFY(!viewport.isFull());
    COMPARE_REGION(viewport.damageRegion().region, QRegion());
    tracker.finishFrame();
    viewport.finishFrame();

    g->setBoundingRect(QRectF(20, 20, 30, 40));
    tracker.commit(viewport);
    COMPARE_REGION(viewport.damageRegion().region, QRegion(10, 20, 40, 40));
}

void WSGDamageGraphTest::removeGeometry()
{
    WSGDamageNode root;
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(5, 5, 10, 10));
    root.appendChild(g);

    TestTracker tracker(&root);
    tracker.commit();

    root.removeChild(g);
    COMPARE_REGION(tracker.commit(), QRegion(5, 5, 10, 10));
    delete g;
}

void WSGDamageGraphTest::addRemoveSameFrame()
{
    WSGDamageNode root;
    TestTracker tracker(&root);
    tracker.commit();

    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(0, 0, 40, 40));
    root.appendChild(g);
    root.removeChild(g);
    delete g;

    COMPARE_REGION(tracker.commit(), QRegion());
}

void WSGDamageGraphTest::moveGeometryRect()
{
    WSGDamageNode root;
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(0, 0, 20, 20));
    root.appendChild(g);

    TestTracker tracker(&root);
    tracker.commit();

    g->setBoundingRect(QRectF(50, 50, 20, 20));
    COMPARE_REGION(tracker.commit(), QRegion(0, 0, 20, 20) + QRegion(50, 50, 20, 20));
}

void WSGDamageGraphTest::resizeGeometry()
{
    WSGDamageNode root;
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(0, 0, 10, 10));
    root.appendChild(g);

    TestTracker tracker(&root);
    tracker.commit();

    g->setBoundingRect(QRectF(0, 0, 30, 10));
    COMPARE_REGION(tracker.commit(), QRegion(0, 0, 30, 10));
}

void WSGDamageGraphTest::partialContentDamage()
{
    WSGDamageNode root;
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(0, 0, 100, 100));
    root.appendChild(g);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(g->worldValidRegion(), QRegion(0, 0, 100, 100));

    g->markContentDirty(QRect(10, 15, 4, 5));
    COMPARE_REGION(tracker.commit(), QRegion(10, 15, 4, 5));
    COMPARE_REGION(g->worldValidRegion(), QRegion(0, 0, 100, 100));
}

void WSGDamageGraphTest::contentDamageClippedToBounds()
{
    WSGDamageNode root;
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(0, 0, 20, 20));
    root.appendChild(g);

    TestTracker tracker(&root);
    tracker.commit();

    g->markContentDirty(QRect(-10, -10, 15, 15));
    COMPARE_REGION(tracker.commit(), QRegion(0, 0, 5, 5));
}

void WSGDamageGraphTest::clipIntersectsGeometryBounds()
{
    WSGDamageNode root;
    auto *clip = new WSGDamageClipNode;
    clip->setClipRect(QRectF(0, 0, 40, 30));
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(0, 0, 100, 100));
    clip->appendChild(g);
    root.appendChild(clip);

    TestTracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion(0, 0, 40, 30));
    QCOMPARE(g->boundingRect(), QRectF(0, 0, 100, 100));
    QCOMPARE(g->worldBounds(), QRect(0, 0, 40, 30));
    COMPARE_REGION(g->worldValidRegion(), QRegion(0, 0, 40, 30));
    COMPARE_REGION(g->worldVisibleRegion(), QRegion(0, 0, 40, 30));
    COMPARE_REGION(tracker.commit(), QRegion());
}

void WSGDamageGraphTest::clipScrollKeepsUnclippedLocalBounds()
{
    WSGDamageNode root;
    auto *clip = new WSGDamageClipNode;
    clip->setClipRect(QRectF(0, 0, 100, 50));
    auto *tr = new WSGDamageTransformNode;
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(0, 0, 20, 20));
    tr->appendChild(g);
    clip->appendChild(tr);
    root.appendChild(clip);

    TestTracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion(0, 0, 20, 20));

    tr->setTranslation(0, 30);
    COMPARE_REGION(tracker.commit(), QRegion(0, 0, 20, 20) + QRegion(0, 30, 20, 20));
    QCOMPARE(g->boundingRect(), QRectF(0, 0, 20, 20));
    QCOMPARE(g->worldBounds(), QRect(0, 30, 20, 20));

    tr->setTranslation(0, 80);
    COMPARE_REGION(tracker.commit(), QRegion(0, 30, 20, 20));
    QVERIFY(g->worldBounds().isEmpty());
    COMPARE_REGION(g->worldValidRegion(), QRegion());
    COMPARE_REGION(g->worldVisibleRegion(), QRegion());
}

void WSGDamageGraphTest::clipResizeDamagesCrescent()
{
    WSGDamageNode root;
    auto *clip = new WSGDamageClipNode;
    clip->setClipRect(QRectF(0, 0, 100, 100));
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(0, 0, 200, 200));
    clip->appendChild(g);
    root.appendChild(clip);

    TestTracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion(0, 0, 100, 100));

    clip->setClipRect(QRectF(0, 0, 50, 50));
    COMPARE_REGION(tracker.commit(), QRegion(0, 0, 100, 100));
    QCOMPARE(g->worldBounds(), QRect(0, 0, 50, 50));
}

void WSGDamageGraphTest::clipDoesNotOccludeOutside()
{
    WSGDamageNode root;
    auto *back = new WSGDamageGeometryNode;
    back->setBoundingRect(QRectF(0, 0, 100, 100));
    back->setFullyOpaque(true);
    auto *clip = new WSGDamageClipNode;
    clip->setClipRect(QRectF(0, 0, 50, 50));
    auto *front = new WSGDamageGeometryNode;
    front->setBoundingRect(QRectF(0, 0, 100, 100));
    front->setFullyOpaque(true);
    clip->appendChild(front);
    root.appendChild(back);
    root.appendChild(clip);

    TestTracker tracker(&root);
    tracker.commit();
    QCOMPARE(front->worldBounds(), QRect(0, 0, 50, 50));
    COMPARE_REGION(front->worldOpaqueRegion(), QRegion(0, 0, 50, 50));
    COMPARE_REGION(front->worldValidRegion(), QRegion(0, 0, 50, 50));
    COMPARE_REGION(front->worldVisibleRegion(), QRegion(0, 0, 50, 50));
    COMPARE_REGION(back->worldValidRegion(), QRegion(0, 0, 100, 100) - QRegion(0, 0, 50, 50));
    COMPARE_REGION(back->worldVisibleRegion(), QRegion(0, 0, 100, 100));

    back->markContentDirty(QRect(0, 0, 100, 100));
    COMPARE_REGION(tracker.commit(), QRegion(0, 0, 100, 100) - QRegion(0, 0, 50, 50));
}

void WSGDamageGraphTest::nestedClips()
{
    WSGDamageNode root;
    auto *outer = new WSGDamageClipNode;
    outer->setClipRect(QRectF(0, 0, 100, 100));
    auto *inner = new WSGDamageClipNode;
    inner->setClipRect(QRectF(50, 50, 100, 100));
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(0, 0, 200, 200));
    inner->appendChild(g);
    outer->appendChild(inner);
    root.appendChild(outer);

    TestTracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion(50, 50, 50, 50));
    QCOMPARE(g->worldBounds(), QRect(50, 50, 50, 50));
    COMPARE_REGION(g->worldValidRegion(), QRegion(50, 50, 50, 50));
    COMPARE_REGION(g->worldVisibleRegion(), QRegion(50, 50, 50, 50));
}

void WSGDamageGraphTest::emptyClipHidesContent()
{
    WSGDamageNode root;
    auto *clip = new WSGDamageClipNode;
    clip->setClipRect(QRectF(0, 0, 40, 40));
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(0, 0, 80, 80));
    clip->appendChild(g);
    root.appendChild(clip);

    TestTracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion(0, 0, 40, 40));

    clip->setClipRect(QRectF());
    COMPARE_REGION(tracker.commit(), QRegion(0, 0, 40, 40));
    QVERIFY(g->worldBounds().isEmpty());
    COMPARE_REGION(g->worldValidRegion(), QRegion());
    COMPARE_REGION(g->worldVisibleRegion(), QRegion());
    COMPARE_REGION(tracker.commit(), QRegion());
}

void WSGDamageGraphTest::clippedSubtreeDefersChanges()
{
    WSGDamageNode root;
    auto *clip = new WSGDamageClipNode;
    clip->setClipRect(QRectF(0, 0, 40, 40));
    auto *group = new WSGDamageNode;
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(80, 0, 20, 20));
    group->appendChild(g);
    clip->appendChild(group);
    root.appendChild(clip);

    TestTracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion());
    QVERIFY(g->worldBounds().isEmpty());

    g->markContentDirty(QRect(0, 0, 10, 10));
    COMPARE_REGION(tracker.commit(), QRegion());
    QVERIFY(g->isDirty());

    g->setBoundingRect(QRectF(10, 10, 20, 20));
    COMPARE_REGION(tracker.commit(), QRegion(10, 10, 20, 20));
    QCOMPARE(g->worldBounds(), QRect(10, 10, 20, 20));
}

void WSGDamageGraphTest::contentDirtyClippedToClipNode()
{
    WSGDamageNode root;
    auto *clip = new WSGDamageClipNode;
    clip->setClipRect(QRectF(10, 10, 20, 20));
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(0, 0, 100, 100));
    clip->appendChild(g);
    root.appendChild(clip);

    TestTracker tracker(&root);
    tracker.commit();

    g->markContentDirty(QRect(0, 0, 100, 100));
    COMPARE_REGION(tracker.commit(), QRegion(10, 10, 20, 20));
}

void WSGDamageGraphTest::clipValidAndVisibleFollowClip()
{
    WSGDamageNode root;
    auto *clip = new WSGDamageClipNode;
    clip->setClipRect(QRectF(10, 20, 30, 40));
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(0, 0, 200, 200));
    g->setFullyOpaque(true);
    clip->appendChild(g);
    root.appendChild(clip);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(g->worldValidRegion(), QRegion(10, 20, 30, 40));
    COMPARE_REGION(g->worldVisibleRegion(), QRegion(10, 20, 30, 40));

    clip->setClipRect(QRectF(10, 20, 10, 10));
    tracker.commit();
    COMPARE_REGION(g->worldValidRegion(), QRegion(10, 20, 10, 10));
    COMPARE_REGION(g->worldVisibleRegion(), QRegion(10, 20, 10, 10));
}

void WSGDamageGraphTest::clipBackdropVisibleStaysInsideClip()
{
    WSGDamageNode root;
    auto *back = new WSGDamageGeometryNode;
    back->setBoundingRect(QRectF(0, 0, 100, 100));
    back->setFullyOpaque(true);
    auto *clip = new WSGDamageClipNode;
    clip->setClipRect(QRectF(20, 20, 40, 40));
    auto *glass = new WSGDamageBackdropNode;
    glass->setBoundingRect(QRectF(0, 0, 100, 100));
    clip->appendChild(glass);
    root.appendChild(back);
    root.appendChild(clip);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(glass->worldValidRegion(), QRegion(20, 20, 40, 40));
    COMPARE_REGION(glass->worldVisibleRegion(), QRegion(20, 20, 40, 40));
    COMPARE_REGION(back->worldValidRegion(), QRegion(0, 0, 100, 100));
    COMPARE_REGION(back->worldVisibleRegion(), QRegion(0, 0, 100, 100) - QRegion(20, 20, 40, 40));
}

void WSGDamageGraphTest::nonRectangularClipClipsBoundsWithoutOpaque()
{
    WSGDamageNode root;
    auto *back = new WSGDamageGeometryNode;
    back->setBoundingRect(QRectF(0, 0, 100, 100));
    back->setFullyOpaque(true);
    auto *clip = new WSGDamageClipNode;
    clip->setClipRect(QRectF(0, 0, 50, 50));
    clip->setIsRectangular(false);
    auto *front = new WSGDamageGeometryNode;
    front->setBoundingRect(QRectF(0, 0, 100, 100));
    front->setFullyOpaque(true);
    clip->appendChild(front);
    root.appendChild(back);
    root.appendChild(clip);

    TestTracker tracker(&root);
    tracker.commit();
    QCOMPARE(front->worldBounds(), QRect(0, 0, 50, 50));
    COMPARE_REGION(front->worldOpaqueRegion(), QRegion());
    COMPARE_REGION(front->worldValidRegion(), QRegion(0, 0, 50, 50));
    COMPARE_REGION(back->worldValidRegion(), QRegion(0, 0, 100, 100));

    back->markContentDirty(QRect(0, 0, 100, 100));
    COMPARE_REGION(tracker.commit(), QRegion(0, 0, 100, 100));

    clip->setIsRectangular(true);
    tracker.commit();
    COMPARE_REGION(front->worldOpaqueRegion(), QRegion(0, 0, 50, 50));
    back->markContentDirty(QRect(0, 0, 100, 100));
    COMPARE_REGION(tracker.commit(), QRegion(0, 0, 100, 100) - QRegion(0, 0, 50, 50));
}

void WSGDamageGraphTest::roundedClipInnerOmitsCorners()
{
    WSGDamageNode root;
    auto *back = new WSGDamageGeometryNode;
    back->setBoundingRect(QRectF(0, 0, 100, 100));
    back->setFullyOpaque(true);
    auto *clip = new WSGDamageClipNode;
    clip->setClipRect(QRectF(0, 0, 50, 50));
    clip->setIsRectangular(false);
    clip->setRadius(10);
    auto *front = new WSGDamageGeometryNode;
    front->setBoundingRect(QRectF(0, 0, 100, 100));
    front->setFullyOpaque(true);
    clip->appendChild(front);
    root.appendChild(back);
    root.appendChild(clip);

    TestTracker tracker(&root);
    tracker.commit();
    QCOMPARE(front->worldBounds(), QRect(0, 0, 50, 50));
    const QRegion inner = QRegion(0, 0, 50, 50) - QRect(0, 0, 10, 10) - QRect(40, 0, 10, 10)
        - QRect(0, 40, 10, 10) - QRect(40, 40, 10, 10);
    COMPARE_REGION(front->worldOpaqueRegion(), inner);
    COMPARE_REGION(back->worldValidRegion(), QRegion(0, 0, 100, 100) - inner);

    back->markContentDirty(QRect(0, 0, 100, 100));
    COMPARE_REGION(tracker.commit(), QRegion(0, 0, 100, 100) - inner);
}

void WSGDamageGraphTest::waylandOpaqueRegionPunchesBehind()
{
    // Client opaque_region is (0,0,80,50) on a 100x50 surface placed at (10,20).
    WSGDamageNode root;
    auto *back = new WSGDamageGeometryNode;
    back->setBoundingRect(QRectF(0, 0, 200, 200));
    back->setFullyOpaque(true);
    auto *front = new WSGDamageGeometryNode;
    front->setBoundingRect(QRectF(10, 20, 100, 50));
    front->setOpaqueRegion(QRegion(0, 0, 80, 50));
    root.appendChild(back);
    root.appendChild(front);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(front->worldOpaqueRegion(), QRegion(10, 20, 80, 50));
    COMPARE_REGION(back->worldValidRegion(), QRegion(0, 0, 200, 200) - QRegion(10, 20, 80, 50));

    back->markContentDirty(QRect(0, 0, 200, 200));
    COMPARE_REGION(tracker.commit(), QRegion(0, 0, 200, 200) - QRegion(10, 20, 80, 50));
}

void WSGDamageGraphTest::hideShow()
{
    WSGDamageNode root;
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(8, 8, 16, 16));
    root.appendChild(g);

    TestTracker tracker(&root);
    tracker.commit();

    g->setVisible(false);
    COMPARE_REGION(tracker.commit(), QRegion(8, 8, 16, 16));

    COMPARE_REGION(tracker.commit(), QRegion());

    g->setVisible(true);
    COMPARE_REGION(tracker.commit(), QRegion(8, 8, 16, 16));
}

void WSGDamageGraphTest::hiddenSubtreeDefersChanges()
{
    WSGDamageNode root;
    auto *group = new WSGDamageNode;
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(0, 0, 20, 20));
    group->appendChild(g);
    root.appendChild(group);

    TestTracker tracker(&root);
    tracker.commit();
    group->setVisible(false);
    COMPARE_REGION(tracker.commit(), QRegion(0, 0, 20, 20));

    g->setBoundingRect(QRectF(100, 0, 20, 20));
    COMPARE_REGION(tracker.commit(), QRegion());
    QVERIFY(g->isDirty());

    group->setVisible(true);
    COMPARE_REGION(tracker.commit(), QRegion(100, 0, 20, 20));
    QCOMPARE(g->worldBounds(), QRect(100, 0, 20, 20));
}

void WSGDamageGraphTest::aggregateProxyIsOrdinaryGeometry()
{
    WSGDamageNode root;
    auto *group = new WSGDamageTransformNode;
    group->setTranslation(20, 30);
    auto *proxy = new WSGDamageGeometryNode;
    proxy->setBoundingRect(QRectF(0, 0, 100, 80));
    group->appendChild(proxy);
    root.appendChild(group);

    TestTracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion(20, 30, 100, 80));
    QCOMPARE(proxy->worldBounds(), QRect(20, 30, 100, 80));

    proxy->markContentDirty(QRect(0, 0, 100, 80));
    COMPARE_REGION(tracker.commit(), QRegion(20, 30, 100, 80));

    group->setTranslation(200, 30);
    COMPARE_REGION(tracker.commit(), QRegion(20, 30, 100, 80) + QRegion(200, 30, 100, 80));
    QCOMPARE(proxy->worldBounds(), QRect(200, 30, 100, 80));
}

void WSGDamageGraphTest::hideShowSameFrame()
{
    WSGDamageNode root;
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(0, 0, 12, 12));
    root.appendChild(g);

    TestTracker tracker(&root);
    tracker.commit();

    g->setVisible(false);
    g->setVisible(true);
    COMPARE_REGION(tracker.commit(), QRegion());
}

void WSGDamageGraphTest::hideThenRemoveWithoutCommit()
{
    WSGDamageNode root;
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(1, 2, 3, 4));
    root.appendChild(g);

    TestTracker tracker(&root);
    tracker.commit();

    g->setVisible(false);
    root.removeChild(g);
    COMPARE_REGION(tracker.commit(), QRegion(1, 2, 3, 4));
    delete g;
}

void WSGDamageGraphTest::translateTransform()
{
    WSGDamageNode root;
    auto *tr = new WSGDamageTransformNode;
    tr->setTranslation(10, 20);
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(0, 0, 5, 6));
    tr->appendChild(g);
    root.appendChild(tr);

    TestTracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion(10, 20, 5, 6));

    tr->setTranslation(40, 20);
    COMPARE_REGION(tracker.commit(), QRegion(10, 20, 5, 6) + QRegion(40, 20, 5, 6));
}

void WSGDamageGraphTest::nestedTransforms()
{
    WSGDamageNode root;
    auto *a = new WSGDamageTransformNode;
    a->setTranslation(10, 0);
    auto *b = new WSGDamageTransformNode;
    b->setTranslation(0, 20);
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(1, 2, 3, 4));
    b->appendChild(g);
    a->appendChild(b);
    root.appendChild(a);

    TestTracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion(11, 22, 3, 4));
}

void WSGDamageGraphTest::scaleTransform()
{
    WSGDamageNode root;
    auto *tr = new WSGDamageTransformNode;
    tr->setScale(2, 3);
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(1, 1, 4, 2));
    tr->appendChild(g);
    root.appendChild(tr);

    TestTracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion(2, 3, 8, 6));
}

void WSGDamageGraphTest::rotate90()
{
    WSGDamageNode root;
    auto *tr = new WSGDamageTransformNode;
    QTransform t;
    t.translate(50, 50);
    t.rotate(90);
    t.translate(-50, -50);
    tr->setMatrix(t);

    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(40, 40, 20, 10));
    g->setFullyOpaque(true);
    tr->appendChild(g);
    root.appendChild(tr);

    TestTracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion(g->worldBounds()));
    QVERIFY(isAxisAligned(g->worldTransform().toTransform()));
    COMPARE_REGION(g->worldOpaqueRegion(), QRegion(g->worldBounds()));
}

void WSGDamageGraphTest::rotate45Conservative()
{
    WSGDamageNode root;
    auto *tr = new WSGDamageTransformNode;
    QTransform t;
    t.translate(100, 100);
    t.rotate(45);
    t.translate(-50, -50);
    tr->setMatrix(t);

    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(0, 0, 100, 100));
    g->setFullyOpaque(true);
    tr->appendChild(g);
    root.appendChild(tr);

    TestTracker tracker(&root);
    const QRegion d = tracker.commit();
    const QRect aabb = g->worldBounds();
    COMPARE_REGION(d, QRegion(aabb));
    // Must not claim opacity under a non-axis-aligned transform.
    COMPARE_REGION(g->worldOpaqueRegion(), QRegion());
}

void WSGDamageGraphTest::siblingOcclusionFullyCovered()
{
    WSGDamageNode root;
    auto *back = new WSGDamageGeometryNode;
    back->setName(QStringLiteral("back"));
    back->setBoundingRect(QRectF(0, 0, 100, 100));
    back->setFullyOpaque(true);
    auto *front = new WSGDamageGeometryNode;
    front->setName(QStringLiteral("front"));
    front->setBoundingRect(QRectF(0, 0, 100, 100));
    front->setFullyOpaque(true);
    root.appendChild(back);
    root.appendChild(front);

    TestTracker tracker(&root);
    tracker.commit();

    back->markContentDirty(QRect(10, 10, 8, 8));
    COMPARE_REGION(tracker.commit(), QRegion());
}

void WSGDamageGraphTest::siblingOcclusionPartial()
{
    WSGDamageNode root;
    auto *back = new WSGDamageGeometryNode;
    back->setBoundingRect(QRectF(0, 0, 100, 100));
    back->setFullyOpaque(true);
    auto *front = new WSGDamageGeometryNode;
    front->setBoundingRect(QRectF(25, 25, 50, 50));
    front->setFullyOpaque(true);
    root.appendChild(back);
    root.appendChild(front);

    TestTracker tracker(&root);
    tracker.commit();

    back->markContentDirty(QRect(0, 0, 10, 10));
    COMPARE_REGION(tracker.commit(), QRegion(0, 0, 10, 10));

    back->markContentDirty(QRect(40, 40, 10, 10));
    COMPARE_REGION(tracker.commit(), QRegion());
}

void WSGDamageGraphTest::transparentDoesNotOcclude()
{
    WSGDamageNode root;
    auto *back = new WSGDamageGeometryNode;
    back->setBoundingRect(QRectF(0, 0, 40, 40));
    auto *front = new WSGDamageGeometryNode;
    front->setBoundingRect(QRectF(0, 0, 40, 40));
    root.appendChild(back);
    root.appendChild(front);

    TestTracker tracker(&root);
    tracker.commit();

    back->markContentDirty(QRect(2, 2, 4, 4));
    COMPARE_REGION(tracker.commit(), QRegion(2, 2, 4, 4));
}

void WSGDamageGraphTest::parentGeometryBehindChildren()
{
    WSGDamageNode root;
    auto *parent = new WSGDamageGeometryNode;
    parent->setBoundingRect(QRectF(0, 0, 80, 80));
    parent->setFullyOpaque(true);
    auto *child = new WSGDamageGeometryNode;
    child->setBoundingRect(QRectF(0, 0, 80, 80));
    child->setFullyOpaque(true);
    parent->appendChild(child);
    root.appendChild(parent);

    TestTracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion(0, 0, 80, 80));
    parent->markContentDirty(QRect(0, 0, 80, 80));
    COMPARE_REGION(tracker.commit(), QRegion());
}

void WSGDamageGraphTest::occludedRegionReported()
{
    WSGDamageNode root;
    auto *back = new WSGDamageGeometryNode;
    back->setBoundingRect(QRectF(0, 0, 30, 30));
    back->setFullyOpaque(true);
    auto *front = new WSGDamageGeometryNode;
    front->setBoundingRect(QRectF(10, 0, 30, 30));
    front->setFullyOpaque(true);
    root.appendChild(back);
    root.appendChild(front);

    TestTracker tracker(&root);
    tracker.commit();

    COMPARE_REGION(front->worldValidRegion(), QRegion(10, 0, 30, 30));
    COMPARE_REGION(back->worldValidRegion(), QRegion(0, 0, 10, 30));
}

void WSGDamageGraphTest::insertBetweenSiblings()
{
    WSGDamageNode root;
    auto *a = new WSGDamageGeometryNode;
    a->setBoundingRect(QRectF(0, 0, 10, 10));
    auto *c = new WSGDamageGeometryNode;
    c->setBoundingRect(QRectF(20, 0, 10, 10));
    root.appendChild(a);
    root.appendChild(c);

    TestTracker tracker(&root);
    tracker.commit();

    auto *b = new WSGDamageGeometryNode;
    b->setBoundingRect(QRectF(10, 0, 10, 10));
    root.insertChildBefore(b, c);
    QCOMPARE(a->nextSibling(), b);
    QCOMPARE(b->nextSibling(), c);
    COMPARE_REGION(tracker.commit(), QRegion(10, 0, 10, 10));
}

void WSGDamageGraphTest::reparent()
{
    WSGDamageNode root;
    auto *t1 = new WSGDamageTransformNode;
    t1->setTranslation(0, 0);
    auto *t2 = new WSGDamageTransformNode;
    t2->setTranslation(80, 0);
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(0, 0, 10, 10));
    t1->appendChild(g);
    root.appendChild(t1);
    root.appendChild(t2);

    TestTracker tracker(&root);
    tracker.commit();

    t2->appendChild(g);
    COMPARE_REGION(tracker.commit(), QRegion(0, 0, 10, 10) + QRegion(80, 0, 10, 10));
}

void WSGDamageGraphTest::raiseLowersZOrder()
{
    WSGDamageNode root;
    auto *a = new WSGDamageGeometryNode;
    a->setBoundingRect(QRectF(0, 0, 40, 40));
    a->setFullyOpaque(true);
    auto *b = new WSGDamageGeometryNode;
    b->setBoundingRect(QRectF(0, 0, 40, 40));
    b->setFullyOpaque(true);
    root.appendChild(a);
    root.appendChild(b);

    TestTracker tracker(&root);
    tracker.commit();

    root.appendChild(a); // raise a
    QCOMPARE(static_cast<WSGDamageNode*>(root.lastChild()), static_cast<WSGDamageNode*>(a));
    QCOMPARE(static_cast<WSGDamageNode*>(root.firstChild()), static_cast<WSGDamageNode*>(b));
    COMPARE_REGION(tracker.commit(), QRegion(0, 0, 40, 40));

    b->markContentDirty(QRect(5, 5, 10, 10));
    COMPARE_REGION(tracker.commit(), QRegion());

    a->markContentDirty(QRect(5, 5, 10, 10));
    COMPARE_REGION(tracker.commit(), QRegion(5, 5, 10, 10));
}

void WSGDamageGraphTest::fullyOpaqueFlag()
{
    WSGDamageNode root;
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(0, 0, 16, 16));
    g->setFullyOpaque(true);
    root.appendChild(g);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(g->worldOpaqueRegion(), QRegion(0, 0, 16, 16));

    g->setBoundingRect(QRectF(80, 80, 32, 16));
    tracker.commit();
    COMPARE_REGION(g->worldOpaqueRegion(), QRegion(80, 80, 32, 16));
    COMPARE_REGION(g->opaqueRegion(), QRegion(0, 0, 32, 16));
}

void WSGDamageGraphTest::opaqueRegionUpdate()
{
    WSGDamageNode root;
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(0, 0, 40, 40));
    const WPixmanRegion opaqueStrip(0, 0, 10, 40);
    g->setOpaqueRegion(opaqueStrip.native());
    auto *back = new WSGDamageGeometryNode;
    back->setBoundingRect(QRectF(0, 0, 40, 40));
    root.appendChild(back);
    root.appendChild(g);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(g->worldOpaqueRegion(), QRegion(0, 0, 10, 40));
    COMPARE_REGION(back->worldValidRegion(), QRegion(10, 0, 30, 40));

    g->setBoundingRect(QRectF(80, 80, 40, 40));
    tracker.commit();
    COMPARE_REGION(g->worldOpaqueRegion(), QRegion(80, 80, 10, 40));
    COMPARE_REGION(g->opaqueRegion(), QRegion(0, 0, 10, 40));
}

void WSGDamageGraphTest::contentLocalDirtyFollowsBox()
{
    WSGDamageNode root;
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(100, 110, 240, 150));
    root.appendChild(g);

    TestTracker tracker(&root);
    tracker.commit();

    g->markContentDirty(QRect(12, 24, 26, 20));
    COMPARE_REGION(tracker.commit(), QRegion(112, 134, 26, 20));

    g->setBoundingRect(QRectF(180, 150, 240, 150));
    tracker.commit();
    g->markContentDirty(QRect(12, 24, 26, 20));
    COMPARE_REGION(tracker.commit(), QRegion(192, 174, 26, 20));
}

void WSGDamageGraphTest::multipleDirtyRegions()
{
    WSGDamageNode root;
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(0, 0, 200, 200));
    root.appendChild(g);

    TestTracker tracker(&root);
    tracker.commit();

    g->markContentDirty(QRect(0, 0, 2, 2));
    g->markContentDirty(QRect(50, 50, 3, 3));
    COMPARE_REGION(tracker.commit(), QRegion(0, 0, 2, 2) + QRegion(50, 50, 3, 3));
}

void WSGDamageGraphTest::destroySubtree()
{
    WSGDamageNode root;
    auto *tr = new WSGDamageTransformNode;
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(3, 4, 5, 6));
    tr->appendChild(g);
    root.appendChild(tr);

    TestTracker tracker(&root);
    tracker.commit();

    delete tr;
    COMPARE_REGION(tracker.commit(), QRegion(3, 4, 5, 6));
}

void WSGDamageGraphTest::basicNodeGrouping()
{
    WSGDamageNode root;
    auto *group = new WSGDamageNode;
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(7, 8, 9, 10));
    group->appendChild(g);
    root.appendChild(group);

    TestTracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion(7, 8, 9, 10));
}

void WSGDamageGraphTest::parentHideHidesChildren()
{
    WSGDamageNode root;
    auto *group = new WSGDamageNode;
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(0, 0, 20, 20));
    group->appendChild(g);
    root.appendChild(group);

    TestTracker tracker(&root);
    tracker.commit();

    group->setVisible(false);
    COMPARE_REGION(tracker.commit(), QRegion(0, 0, 20, 20));
}

void WSGDamageGraphTest::fractionalTranslationOverestimates()
{
    WSGDamageNode root;
    auto *tr = new WSGDamageTransformNode;
    QTransform t;
    t.translate(0.4, 0.4);
    tr->setMatrix(t);
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(0, 0, 10, 10));
    tr->appendChild(g);
    root.appendChild(tr);

    TestTracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion(0, 0, 11, 11));
}

void WSGDamageGraphTest::moveOpaqueRevealsBehind()
{
    WSGDamageNode root;
    auto *back = new WSGDamageGeometryNode;
    back->setBoundingRect(QRectF(0, 0, 80, 80));
    back->setFullyOpaque(true);
    auto *front = new WSGDamageGeometryNode;
    front->setBoundingRect(QRectF(0, 0, 80, 80));
    front->setFullyOpaque(true);
    root.appendChild(back);
    root.appendChild(front);

    TestTracker tracker(&root);
    tracker.commit();

    front->setBoundingRect(QRectF(100, 0, 80, 80));
    COMPARE_REGION(tracker.commit(), QRegion(0, 0, 80, 80) + QRegion(100, 0, 80, 80));
}

void WSGDamageGraphTest::movingFrontDoesNotDamageCleanBackWhole()
{
    WSGDamageNode root;
    auto *back = new WSGDamageGeometryNode;
    back->setBoundingRect(QRectF(0, 0, 100, 100));
    auto *front = new WSGDamageGeometryNode;
    front->setBoundingRect(QRectF(25, 0, 50, 100));
    front->setFullyOpaque(true);
    root.appendChild(back);
    root.appendChild(front);

    TestTracker tracker(&root);
    tracker.commit();

    front->setBoundingRect(QRectF(35, 0, 50, 100));
    const QRegion damage = tracker.commit();
    COMPARE_REGION(WSGDamageNodeTestAccess::ownDamage(back), QRegion());
    COMPARE_REGION(damage, QRegion(25, 0, 60, 100));
}

void WSGDamageGraphTest::contentDamageUnderOpaqueSibling()
{
    WSGDamageNode root;
    auto *back = new WSGDamageGeometryNode;
    back->setBoundingRect(QRectF(0, 0, 60, 60));
    auto *front = new WSGDamageGeometryNode;
    front->setBoundingRect(QRectF(20, 20, 20, 20));
    front->setFullyOpaque(true);
    root.appendChild(back);
    root.appendChild(front);

    TestTracker tracker(&root);
    tracker.commit();

    back->markContentDirty(QRect(22, 22, 4, 4));
    // Fully covered by the opaque front sibling: no scene damage escapes.
    COMPARE_REGION(tracker.commit(), QRegion());
}

void WSGDamageGraphTest::fullyOccludedMoveProducesNoDamage()
{
    WSGDamageNode root;
    auto *back = new WSGDamageGeometryNode;
    back->setBoundingRect(QRectF(20, 20, 20, 20));
    back->setFullyOpaque(true);
    auto *front = new WSGDamageGeometryNode;
    front->setBoundingRect(QRectF(0, 0, 100, 100));
    front->setFullyOpaque(true);
    root.appendChild(back);
    root.appendChild(front);

    TestTracker tracker(&root);
    tracker.commit();

    back->setBoundingRect(QRectF(60, 20, 20, 20));
    COMPARE_REGION(tracker.commit(), QRegion());
}

void WSGDamageGraphTest::partiallyOccludedMoveAvoidsOpaqueFront()
{
    WSGDamageNode root;
    auto *back = new WSGDamageGeometryNode;
    back->setBoundingRect(QRectF(80, 80, 260, 200));
    back->setFullyOpaque(true);
    auto *front = new WSGDamageGeometryNode;
    front->setBoundingRect(QRectF(160, 140, 220, 180));
    front->setFullyOpaque(true);
    root.appendChild(back);
    root.appendChild(front);

    TestTracker tracker(&root);
    tracker.commit();

    back->setBoundingRect(QRectF(100, 100, 260, 200));
    const QRegion expected =
        (QRegion(80, 80, 260, 200) + QRegion(100, 100, 260, 200)) - QRegion(front->worldBounds());
    COMPARE_REGION(tracker.commit(), expected);
}

void WSGDamageGraphTest::zeroSizeGeometry()
{
    WSGDamageNode root;
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(10, 10, 0, 10));
    root.appendChild(g);

    TestTracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion());
}

void WSGDamageGraphTest::negativeCoordinates()
{
    WSGDamageNode root;
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(-30, -10, 20, 20));
    root.appendChild(g);

    TestTracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion(-30, -10, 20, 20));
}

void WSGDamageGraphTest::deepTree()
{
    WSGDamageNode root;
    WSGDamageNode *cur = &root;
    for (int i = 0; i < 32; ++i) {
        auto *t = new WSGDamageTransformNode;
        t->setTranslation(1, 0);
        cur->appendChild(t);
        cur = t;
    }
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(0, 0, 2, 2));
    cur->appendChild(g);

    TestTracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion(32, 0, 2, 2));
}

void WSGDamageGraphTest::setMatrixIdentityNoDamage()
{
    WSGDamageNode root;
    auto *tr = new WSGDamageTransformNode;
    tr->setTranslation(0, 0);
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(0, 0, 8, 8));
    tr->appendChild(g);
    root.appendChild(tr);

    TestTracker tracker(&root);
    tracker.commit();

    tr->setTranslation(0, 0);
    COMPARE_REGION(tracker.commit(), QRegion());
}

void WSGDamageGraphTest::geometryChildrenZOrder()
{
    WSGDamageNode root;
    auto *parent = new WSGDamageGeometryNode;
    parent->setBoundingRect(QRectF(0, 0, 50, 50));
    auto *child = new WSGDamageGeometryNode;
    child->setBoundingRect(QRectF(10, 10, 10, 10));
    child->setFullyOpaque(true);
    parent->appendChild(child);
    root.appendChild(parent);

    TestTracker tracker(&root);
    tracker.commit();

    parent->markContentDirty(QRect(10, 10, 10, 10));
    // The opaque child covers the dirty spot: the parent emits nothing.
    COMPARE_REGION(tracker.commit(), QRegion());
}

void WSGDamageGraphTest::nonAxisAlignedDropsOpaque()
{
    WSGDamageNode root;
    auto *tr = new WSGDamageTransformNode;
    QTransform t;
    t.rotate(33);
    tr->setMatrix(t);
    auto *front = new WSGDamageGeometryNode;
    front->setBoundingRect(QRectF(0, 0, 80, 80));
    front->setFullyOpaque(true);
    auto *back = new WSGDamageGeometryNode;
    back->setBoundingRect(QRectF(0, 0, 80, 80));
    tr->appendChild(back);
    tr->appendChild(front);
    root.appendChild(tr);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(front->worldOpaqueRegion(), QRegion());
}

void WSGDamageGraphTest::matrix4x4()
{
    WSGDamageNode root;
    auto *tr = new WSGDamageTransformNode;
    QMatrix4x4 m;
    m.translate(15, 25);
    tr->setMatrix(m);
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(0, 0, 4, 4));
    tr->appendChild(g);
    root.appendChild(tr);

    TestTracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion(15, 25, 4, 4));
}

void WSGDamageGraphTest::siblingOrderPaint()
{
    WSGDamageNode root;
    auto *back = new WSGDamageGeometryNode;
    back->setBoundingRect(QRectF(0, 0, 40, 40));
    back->setFullyOpaque(true);
    auto *front = new WSGDamageGeometryNode;
    front->setBoundingRect(QRectF(0, 0, 40, 40));
    front->setFullyOpaque(true);
    root.appendChild(back);
    root.appendChild(front);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(front->worldValidRegion(), QRegion(0, 0, 40, 40));
    COMPARE_REGION(back->worldValidRegion(), QRegion());
}

void WSGDamageGraphTest::firstCommitAppearing()
{
    WSGDamageNode root;
    auto *g1 = new WSGDamageGeometryNode;
    g1->setBoundingRect(QRectF(0, 0, 5, 5));
    auto *g2 = new WSGDamageGeometryNode;
    g2->setBoundingRect(QRectF(10, 0, 5, 5));
    root.appendChild(g1);
    root.appendChild(g2);

    TestTracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion(0, 0, 5, 5) + QRegion(10, 0, 5, 5));
}

void WSGDamageGraphTest::contentDirtyOnNewNode()
{
    WSGDamageNode root;
    TestTracker tracker(&root);
    tracker.commit();

    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(0, 0, 20, 20));
    g->markContentDirty(QRect(1, 1, 1, 1));
    root.appendChild(g);
    COMPARE_REGION(tracker.commit(), QRegion(0, 0, 20, 20));
}

void WSGDamageGraphTest::removeUncommittedNode()
{
    WSGDamageNode root;
    auto *keep = new WSGDamageGeometryNode;
    keep->setBoundingRect(QRectF(0, 0, 5, 5));
    root.appendChild(keep);

    TestTracker tracker(&root);
    tracker.commit();

    auto *tmp = new WSGDamageGeometryNode;
    tmp->setBoundingRect(QRectF(100, 100, 5, 5));
    root.appendChild(tmp);
    root.removeChild(tmp);
    delete tmp;
    COMPARE_REGION(tracker.commit(), QRegion());
}

void WSGDamageGraphTest::nodeAccessorsUseSceneCoordinates()
{
    WSGDamageNode root;
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(0, 0, 10, 10));
    root.appendChild(g);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(g->worldValidRegion(), QRegion(0, 0, 10, 10));
    QCOMPARE(g->worldBounds(), QRect(0, 0, 10, 10));
}

void WSGDamageGraphTest::nodeHasContentProperty()
{
    WSGDamageNode root;
    auto *container = new WSGDamageGeometryNode;
    container->setBoundingRect(QRectF(0, 0, 300, 300));
    container->setHasContent(false); // Acts as a structural container without drawing own pixels

    auto *child = new WSGDamageGeometryNode;
    child->setBoundingRect(QRectF(50, 50, 100, 100));
    container->appendChild(child);
    root.appendChild(container);

    TestTracker tracker(&root);
    // First commit: only child generates damage, container has no own damage
    const QRegion damage1 = tracker.commit();
    COMPARE_REGION(damage1, QRegion(50, 50, 100, 100));
    COMPARE_REGION(WSGDamageNodeTestAccess::ownDamage(container), QRegion());
    QCOMPARE(container->worldBounds(), QRect());
    COMPARE_REGION(container->worldOpaqueRegion(), QRegion());
    COMPARE_REGION(container->worldValidRegion(), QRegion());
    COMPARE_REGION(child->worldValidRegion(), QRegion(50, 50, 100, 100));

    container->setBoundingRect(QRectF(0, 0, 400, 400));
    const QRegion damage2 = tracker.commit();
    COMPARE_REGION(damage2, QRegion());
    COMPARE_REGION(container->worldValidRegion(), QRegion());

    container->setHasContent(true);
    QVERIFY(container->hasContent());
    container->setBoundingRect(QRectF(0, 0, 500, 500));
    const QRegion damage3 = tracker.commit();
    COMPARE_REGION(damage3, QRegion(0, 0, 500, 500));
    COMPARE_REGION(container->worldValidRegion(), QRegion(0, 0, 500, 500));
}

void WSGDamageGraphTest::worldVisiblePartialOpaqueRegion()
{
    WSGDamageNode root;
    auto *back = new WSGDamageGeometryNode;
    back->setBoundingRect(QRectF(0, 0, 40, 40));
    auto *front = new WSGDamageGeometryNode;
    front->setBoundingRect(QRectF(0, 0, 40, 40));
    const WPixmanRegion opaqueStrip(0, 0, 10, 40);
    front->setOpaqueRegion(opaqueStrip.native());
    root.appendChild(back);
    root.appendChild(front);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(front->worldValidRegion(), QRegion(0, 0, 40, 40));
    COMPARE_REGION(back->worldValidRegion(), QRegion(10, 0, 30, 40));
}

void WSGDamageGraphTest::worldVisibleTwoOpaqueFronts()
{
    WSGDamageNode root;
    auto *back = new WSGDamageGeometryNode;
    back->setBoundingRect(QRectF(0, 0, 100, 40));
    back->setFullyOpaque(true);
    auto *left = new WSGDamageGeometryNode;
    left->setBoundingRect(QRectF(0, 0, 20, 40));
    left->setFullyOpaque(true);
    auto *right = new WSGDamageGeometryNode;
    right->setBoundingRect(QRectF(80, 0, 20, 40));
    right->setFullyOpaque(true);
    root.appendChild(back);
    root.appendChild(left);
    root.appendChild(right);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(right->worldValidRegion(), QRegion(80, 0, 20, 40));
    COMPARE_REGION(left->worldValidRegion(), QRegion(0, 0, 20, 40));
    COMPARE_REGION(back->worldValidRegion(), QRegion(20, 0, 60, 40));
}

void WSGDamageGraphTest::worldVisibleLocalizedSubtreeComposition()
{
    WSGDamageNode root;
    auto *probe = new WSGDamageGeometryNode;
    probe->setBoundingRect(QRectF(0, 0, 400, 40));

    auto *group = new WSGDamageNode;
    auto *groupBack = new WSGDamageGeometryNode;
    groupBack->setBoundingRect(QRectF(0, 0, 400, 40));
    const WPixmanRegion bottomOpaque(0, 20, 400, 20);
    groupBack->setOpaqueRegion(bottomOpaque.native());
    auto *glass = new WSGDamageBackdropNode;
    const QRect glassBounds(100, 0, 20, 20);
    glass->setBoundingRect(glassBounds);
    glass->setNeedsBackdrop(true);
    group->appendChild(groupBack);
    group->appendChild(glass);

    root.appendChild(probe);
    root.appendChild(group);

    QRegion frontOpaque;
    for (int i = 0; i < 17; ++i) {
        const QRect rect(i * 20, 0, 10, 10);
        auto *front = new WSGDamageGeometryNode;
        front->setBoundingRect(rect);
        front->setFullyOpaque(true);
        root.appendChild(front);
        frontOpaque += rect;
    }

    TestTracker tracker(&root);
    tracker.commit();

    const QRegion bounds(0, 0, 400, 40);
    const QRegion effectiveFront = frontOpaque - glassBounds;
    COMPARE_REGION(groupBack->worldValidRegion(), bounds - effectiveFront);
    COMPARE_REGION(groupBack->worldVisibleRegion(), bounds - glassBounds);
    COMPARE_REGION(probe->worldValidRegion(), bounds - effectiveFront - QRegion(0, 20, 400, 20));
    COMPARE_REGION(probe->worldVisibleRegion(), bounds - glassBounds);
}

void WSGDamageGraphTest::worldVisibleRaiseReveals()
{
    WSGDamageNode root;
    auto *a = new WSGDamageGeometryNode;
    a->setBoundingRect(QRectF(0, 0, 40, 40));
    a->setFullyOpaque(true);
    auto *b = new WSGDamageGeometryNode;
    b->setBoundingRect(QRectF(0, 0, 40, 40));
    b->setFullyOpaque(true);
    root.appendChild(a);
    root.appendChild(b);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(b->worldValidRegion(), QRegion(0, 0, 40, 40));
    COMPARE_REGION(a->worldValidRegion(), QRegion());

    root.appendChild(a);
    tracker.commit();
    COMPARE_REGION(a->worldValidRegion(), QRegion(0, 0, 40, 40));
    COMPARE_REGION(b->worldValidRegion(), QRegion());
}

void WSGDamageGraphTest::worldVisibleHasContentOff()
{
    WSGDamageNode root;
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(0, 0, 30, 30));
    g->setFullyOpaque(true);
    root.appendChild(g);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(g->worldValidRegion(), QRegion(0, 0, 30, 30));

    g->setHasContent(false);
    tracker.commit();
    COMPARE_REGION(g->worldValidRegion(), QRegion());
    QCOMPARE(g->worldBounds(), QRect());
    COMPARE_REGION(g->worldOpaqueRegion(), QRegion());
}

void WSGDamageGraphTest::worldVisibleParentHideClearsChild()
{
    WSGDamageNode root;
    auto *group = new WSGDamageNode;
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(0, 0, 20, 20));
    group->appendChild(g);
    root.appendChild(group);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(g->worldValidRegion(), QRegion(0, 0, 20, 20));
    COMPARE_REGION(group->worldValidRegion(), QRegion());

    group->setVisible(false);
    tracker.commit();
    COMPARE_REGION(g->worldValidRegion(), QRegion());
    COMPARE_REGION(group->worldValidRegion(), QRegion());
}

void WSGDamageGraphTest::worldVisibleUnderTranslation()
{
    WSGDamageNode root;
    auto *tr = new WSGDamageTransformNode;
    tr->setTranslation(10, 20);
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(0, 0, 5, 6));
    tr->appendChild(g);
    root.appendChild(tr);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(g->worldValidRegion(), QRegion(10, 20, 5, 6));

    tr->setTranslation(40, 20);
    tracker.commit();
    COMPARE_REGION(g->worldValidRegion(), QRegion(40, 20, 5, 6));
}

void WSGDamageGraphTest::worldVisibleNestedAndScale()
{
    WSGDamageNode root;
    auto *a = new WSGDamageTransformNode;
    a->setTranslation(10, 0);
    auto *b = new WSGDamageTransformNode;
    b->setTranslation(0, 20);
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(1, 2, 3, 4));
    b->appendChild(g);
    a->appendChild(b);
    root.appendChild(a);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(g->worldValidRegion(), QRegion(11, 22, 3, 4));

    WSGDamageNode root2;
    auto *tr = new WSGDamageTransformNode;
    tr->setScale(2, 3);
    auto *s = new WSGDamageGeometryNode;
    s->setBoundingRect(QRectF(1, 1, 4, 2));
    tr->appendChild(s);
    root2.appendChild(tr);
    TestTracker tracker2(&root2);
    tracker2.commit();
    COMPARE_REGION(s->worldValidRegion(), QRegion(2, 3, 8, 6));
}

void WSGDamageGraphTest::worldVisibleOverlappingOpaqueNodes()
{
    WSGDamageNode root;
    auto *back = new WSGDamageGeometryNode;
    back->setBoundingRect(QRectF(160, 140, 220, 180));
    back->setFullyOpaque(true);
    auto *front = new WSGDamageGeometryNode;
    front->setBoundingRect(QRectF(80, 80, 260, 200));
    front->setFullyOpaque(true);
    root.appendChild(back);
    root.appendChild(front);

    TestTracker tracker(&root);
    tracker.commit();

    COMPARE_REGION(front->worldValidRegion(), QRegion(80, 80, 260, 200));
    COMPARE_REGION(back->worldValidRegion(),
                   QRegion(160, 140, 220, 180) - QRegion(80, 80, 260, 200));
}

void WSGDamageGraphTest::worldVisibleNonAxisAlignedFront()
{
    WSGDamageNode root;
    auto *tr = new WSGDamageTransformNode;
    QTransform t;
    t.rotate(33);
    tr->setMatrix(t);
    auto *back = new WSGDamageGeometryNode;
    back->setBoundingRect(QRectF(0, 0, 80, 80));
    auto *front = new WSGDamageGeometryNode;
    front->setBoundingRect(QRectF(0, 0, 80, 80));
    front->setFullyOpaque(true);
    tr->appendChild(back);
    tr->appendChild(front);
    root.appendChild(tr);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(front->worldOpaqueRegion(), QRegion());
    COMPARE_REGION(front->worldValidRegion(), QRegion(front->worldBounds()));
    COMPARE_REGION(back->worldValidRegion(), QRegion(back->worldBounds()));
}

void WSGDamageGraphTest::worldVisibleAfterGeometryMove()
{
    WSGDamageNode root;
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(0, 0, 20, 20));
    root.appendChild(g);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(g->worldValidRegion(), QRegion(0, 0, 20, 20));

    g->setBoundingRect(QRectF(50, 50, 20, 20));
    tracker.commit();
    COMPARE_REGION(g->worldValidRegion(), QRegion(50, 50, 20, 20));
}

void WSGDamageGraphTest::worldVisibleZeroSizeEmpty()
{
    WSGDamageNode root;
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(10, 10, 0, 10));
    root.appendChild(g);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(g->worldValidRegion(), QRegion());
}

void WSGDamageGraphTest::worldVisibleRemoveFront()
{
    WSGDamageNode root;
    auto *back = new WSGDamageGeometryNode;
    back->setBoundingRect(QRectF(0, 0, 40, 40));
    back->setFullyOpaque(true);
    auto *front = new WSGDamageGeometryNode;
    front->setBoundingRect(QRectF(0, 0, 40, 40));
    front->setFullyOpaque(true);
    root.appendChild(back);
    root.appendChild(front);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(back->worldValidRegion(), QRegion());

    root.removeChild(front);
    delete front;
    tracker.commit();
    COMPARE_REGION(back->worldValidRegion(), QRegion(0, 0, 40, 40));
}

void WSGDamageGraphTest::worldVisibleIdempotentSecondCommit()
{
    WSGDamageNode root;
    auto *back = new WSGDamageGeometryNode;
    back->setBoundingRect(QRectF(0, 0, 30, 30));
    back->setFullyOpaque(true);
    auto *front = new WSGDamageGeometryNode;
    front->setBoundingRect(QRectF(10, 0, 30, 30));
    front->setFullyOpaque(true);
    root.appendChild(back);
    root.appendChild(front);

    TestTracker tracker(&root);
    tracker.commit();
    const QRegion firstFront = toQRegion(front->worldValidRegion());
    const QRegion firstBack = toQRegion(back->worldValidRegion());
    tracker.commit();
    COMPARE_REGION(front->worldValidRegion(), firstFront);
    COMPARE_REGION(back->worldValidRegion(), firstBack);
}

void WSGDamageGraphTest::worldVisibleGroupHasNone()
{
    WSGDamageNode root;
    auto *group = new WSGDamageNode;
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(7, 8, 9, 10));
    group->appendChild(g);
    root.appendChild(group);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(g->worldValidRegion(), QRegion(7, 8, 9, 10));
    COMPARE_REGION(group->worldValidRegion(), QRegion());
}

void WSGDamageGraphTest::worldVisiblePartialChildCover()
{
    WSGDamageNode root;
    auto *parent = new WSGDamageGeometryNode;
    parent->setBoundingRect(QRectF(0, 0, 50, 50));
    parent->setFullyOpaque(true);
    auto *child = new WSGDamageGeometryNode;
    child->setBoundingRect(QRectF(10, 10, 10, 10));
    child->setFullyOpaque(true);
    parent->appendChild(child);
    root.appendChild(parent);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(child->worldValidRegion(), QRegion(10, 10, 10, 10));
    COMPARE_REGION(parent->worldValidRegion(), QRegion(0, 0, 50, 50) - QRegion(10, 10, 10, 10));
}

void WSGDamageGraphTest::worldVisibleDeepNested()
{
    WSGDamageNode root;
    WSGDamageNode *cur = &root;
    for (int i = 0; i < 32; ++i) {
        auto *t = new WSGDamageTransformNode;
        t->setTranslation(1, 0);
        cur->appendChild(t);
        cur = t;
    }
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(0, 0, 2, 2));
    cur->appendChild(g);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(g->worldValidRegion(), QRegion(32, 0, 2, 2));
}

void WSGDamageGraphTest::worldVisibleInsertAndReparent()
{
    WSGDamageNode root;
    auto *a = new WSGDamageGeometryNode;
    a->setBoundingRect(QRectF(0, 0, 30, 30));
    a->setFullyOpaque(true);
    auto *c = new WSGDamageGeometryNode;
    c->setBoundingRect(QRectF(0, 0, 30, 30));
    c->setFullyOpaque(true);
    root.appendChild(a);
    root.appendChild(c);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(c->worldValidRegion(), QRegion(0, 0, 30, 30));
    COMPARE_REGION(a->worldValidRegion(), QRegion());

    auto *b = new WSGDamageGeometryNode;
    b->setBoundingRect(QRectF(10, 0, 10, 30));
    b->setFullyOpaque(true);
    root.insertChildBefore(b, c);
    tracker.commit();
    COMPARE_REGION(c->worldValidRegion(), QRegion(0, 0, 30, 30));
    COMPARE_REGION(b->worldValidRegion(), QRegion());
    COMPARE_REGION(a->worldValidRegion(), QRegion());

    auto *t2 = new WSGDamageTransformNode;
    t2->setTranslation(80, 0);
    root.appendChild(t2);
    t2->appendChild(b);
    tracker.commit();
    COMPARE_REGION(b->worldValidRegion(), QRegion(90, 0, 10, 30));
    COMPARE_REGION(c->worldValidRegion(), QRegion(0, 0, 30, 30));
    COMPARE_REGION(a->worldValidRegion(), QRegion());
}

void WSGDamageGraphTest::worldVisibleHideShow()
{
    WSGDamageNode root;
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(8, 8, 16, 16));
    root.appendChild(g);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(g->worldValidRegion(), QRegion(8, 8, 16, 16));

    g->setVisible(false);
    tracker.commit();
    COMPARE_REGION(g->worldValidRegion(), QRegion());

    g->setVisible(true);
    tracker.commit();
    COMPARE_REGION(g->worldValidRegion(), QRegion(8, 8, 16, 16));
}

void WSGDamageGraphTest::worldVisibleNegativeCoords()
{
    WSGDamageNode root;
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(-30, -10, 20, 20));
    root.appendChild(g);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(g->worldValidRegion(), QRegion(-30, -10, 20, 20));
}

void WSGDamageGraphTest::worldVisibleRotate90()
{
    WSGDamageNode root;
    auto *tr = new WSGDamageTransformNode;
    QTransform t;
    t.translate(50, 50);
    t.rotate(90);
    t.translate(-50, -50);
    tr->setMatrix(t);
    auto *g = new WSGDamageGeometryNode;
    g->setBoundingRect(QRectF(40, 40, 20, 10));
    g->setFullyOpaque(true);
    tr->appendChild(g);
    root.appendChild(tr);

    TestTracker tracker(&root);
    tracker.commit();
    QVERIFY(isAxisAligned(g->worldTransform().toTransform()));
    COMPARE_REGION(g->worldOpaqueRegion(), QRegion(g->worldBounds()));
    COMPARE_REGION(g->worldValidRegion(), QRegion(g->worldBounds()));
}

void WSGDamageGraphTest::backdropKeepsCoveredBehindDamage()
{
    WSGDamageNode root;
    auto *back = new WSGDamageGeometryNode;
    back->setBoundingRect(QRectF(20, 20, 40, 40));
    back->setFullyOpaque(true);
    auto *backdrop = new WSGDamageBackdropNode;
    backdrop->setBoundingRect(QRectF(10, 10, 60, 60));
    auto *cover = new WSGDamageGeometryNode;
    cover->setBoundingRect(QRectF(0, 0, 80, 80));
    cover->setFullyOpaque(true);
    root.appendChild(back);
    root.appendChild(backdrop);
    root.appendChild(cover);

    TestTracker tracker(&root);
    tracker.commit();

    back->markContentDirty(QRect(8, 8, 10, 10));
    COMPARE_REGION(tracker.commit(), QRegion(28, 28, 10, 10));
}

void WSGDamageGraphTest::backdropPunchesFrontOpaque()
{
    WSGDamageNode root;
    auto *back = new WSGDamageGeometryNode;
    back->setBoundingRect(QRectF(20, 20, 40, 40));
    back->setFullyOpaque(true);
    auto *backdrop = new WSGDamageBackdropNode;
    backdrop->setBoundingRect(QRectF(10, 10, 60, 60));
    backdrop->setNeedsBackdrop(true);
    auto *cover = new WSGDamageGeometryNode;
    cover->setBoundingRect(QRectF(0, 0, 80, 80));
    cover->setFullyOpaque(true);
    root.appendChild(back);
    root.appendChild(backdrop);
    root.appendChild(cover);

    TestTracker tracker(&root);
    tracker.commit();

    COMPARE_REGION(back->worldValidRegion(), QRegion(20, 20, 40, 40));
    COMPARE_REGION(cover->worldValidRegion(), QRegion(0, 0, 80, 80));

    backdrop->setFullyOpaque(true);
    tracker.commit();
    COMPARE_REGION(back->worldValidRegion(), QRegion(20, 20, 40, 40));

    auto *outside = new WSGDamageGeometryNode;
    outside->setBoundingRect(QRectF(90, 0, 10, 10));
    outside->setFullyOpaque(true);
    root.prependChild(outside);
    auto *outsideCover = new WSGDamageGeometryNode;
    outsideCover->setBoundingRect(QRectF(90, 0, 10, 10));
    outsideCover->setFullyOpaque(true);
    root.appendChild(outsideCover);
    tracker.commit();
    COMPARE_REGION(outside->worldValidRegion(), QRegion());
}

void WSGDamageGraphTest::backdropBehindDamageMatchesAccumulator()
{
    WSGDamageNode root;
    auto *back = new WSGDamageGeometryNode;
    back->setBoundingRect(QRectF(0, 0, 50, 50));
    auto *backdrop = new WSGDamageBackdropNode;
    backdrop->setBoundingRect(QRectF(10, 10, 40, 40));
    backdrop->setNeedsBackdrop(true);
    root.appendChild(back);
    root.appendChild(backdrop);

    TestTracker tracker(&root);
    tracker.commit();
    back->markContentDirty(QRect(5, 5, 8, 8));
    tracker.commit();

    COMPARE_REGION(backdrop->behindDamageRegion(), QRegion(5, 5, 8, 8));
    COMPARE_REGION(WPixmanRegion(backdrop->behindDamageRegion()) & backdrop->worldBounds(),
                   QRegion(10, 10, 3, 3));
}

void WSGDamageGraphTest::backdropUncoveredRegion()
{
    WSGDamageNode root;
    auto *back = new WSGDamageGeometryNode;
    back->setBoundingRect(QRectF(0, 0, 50, 50));
    auto *backdrop = new WSGDamageBackdropNode;
    backdrop->setBoundingRect(QRectF(10, 10, 40, 40));
    backdrop->setNeedsBackdrop(true);
    root.appendChild(back);
    root.appendChild(backdrop);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(back->worldVisibleRegion(), QRegion(0, 0, 50, 50) - QRegion(10, 10, 40, 40));
    COMPARE_REGION(backdrop->worldVisibleRegion(), QRegion(10, 10, 40, 40));
}

void WSGDamageGraphTest::stackedBackdropsVisibleRegions()
{
    WSGDamageNode root;
    auto *wall = new WSGDamageGeometryNode;
    wall->setBoundingRect(QRectF(0, 0, 120, 120));
    wall->setFullyOpaque(true);
    auto *backGlass = new WSGDamageBackdropNode;
    backGlass->setBoundingRect(QRectF(0, 0, 80, 80));
    auto *frontGlass = new WSGDamageBackdropNode;
    frontGlass->setBoundingRect(QRectF(40, 40, 80, 80));
    root.appendChild(wall);
    root.appendChild(backGlass);
    root.appendChild(frontGlass);

    TestTracker tracker(&root);
    tracker.commit();

    COMPARE_REGION(backGlass->worldValidRegion(), QRegion(0, 0, 80, 80));
    COMPARE_REGION(frontGlass->worldValidRegion(), QRegion(40, 40, 80, 80));
    COMPARE_REGION(backGlass->worldVisibleRegion(),
                   QRegion(0, 0, 80, 80) - QRegion(40, 40, 80, 80));
    COMPARE_REGION(frontGlass->worldVisibleRegion(), QRegion(40, 40, 80, 80));
    COMPARE_REGION(wall->worldVisibleRegion(),
                   QRegion(0, 0, 120, 120) - QRegion(0, 0, 80, 80) - QRegion(40, 40, 80, 80));
}

void WSGDamageGraphTest::backdropRecopyClippedToSampleBounds()
{
    WSGDamageNode root;
    auto *wall = new WSGDamageGeometryNode;
    wall->setBoundingRect(QRectF(0, 0, 200, 200));
    auto *glass = new WSGDamageBackdropNode;
    glass->setBoundingRect(QRectF(50, 50, 100, 100));
    glass->setRecopyExpansion(QMargins(10, 10, 10, 10)); // sample bounds: (40, 40, 120, 120)
    root.appendChild(wall);
    root.appendChild(glass);

    WSGDamageTracker tracker(&root);
    WSGViewport viewport;
    int outputStorage = 0;
    const void *output = &outputStorage;

    tracker.commit(viewport);
    tracker.finishFrame();
    viewport.finishFrame();
    glass->consumeRecopy(glass->pendingRecopy(output), output);

    // Dirty region on wall partially outside the glass sample bounds:
    // sample bounds are (40, 40, 120, 120).
    // Wall dirt: (20, 50, 60, 50). Intersection with sample is (40, 50, 40, 50).
    wall->markContentDirty(QRect(20, 50, 60, 50));
    tracker.commit(viewport);
    tracker.finishFrame();
    viewport.finishFrame();

    const WPixmanRegion recopy = glass->pendingRecopy(output);
    COMPARE_REGION(recopy, QRegion(40, 50, 40, 50));
}

void WSGDamageGraphTest::stackedBackdropsRecaptureOverlap()
{
    WSGDamageNode root;
    auto *wall = new WSGDamageGeometryNode;
    wall->setBoundingRect(QRectF(0, 0, 120, 120));
    wall->setFullyOpaque(true);
    auto *backGlass = new WSGDamageBackdropNode;
    backGlass->setBoundingRect(QRectF(0, 0, 80, 80));
    backGlass->setNeedsBackdrop(true);
    auto *frontGlass = new WSGDamageBackdropNode;
    frontGlass->setBoundingRect(QRectF(40, 40, 80, 80));
    frontGlass->setNeedsBackdrop(true);
    root.appendChild(wall);
    root.appendChild(backGlass);
    root.appendChild(frontGlass);

    TestTracker tracker(&root);
    tracker.commit();
    wall->markContentDirty(QRect(50, 50, 10, 10));
    tracker.commit();

    COMPARE_REGION(WPixmanRegion(backGlass->behindDamageRegion()) & backGlass->worldBounds(),
                   QRegion(50, 50, 10, 10));
    COMPARE_REGION(WPixmanRegion(frontGlass->behindDamageRegion()) & frontGlass->worldBounds(),
                   QRegion(50, 50, 10, 10));
}

void WSGDamageGraphTest::stackedBackdropsExclusiveDirtySkipsFront()
{
    WSGDamageNode root;
    auto *wall = new WSGDamageGeometryNode;
    wall->setBoundingRect(QRectF(0, 0, 120, 120));
    wall->setFullyOpaque(true);
    auto *backGlass = new WSGDamageBackdropNode;
    backGlass->setBoundingRect(QRectF(0, 0, 80, 80));
    backGlass->setNeedsBackdrop(true);
    auto *frontGlass = new WSGDamageBackdropNode;
    frontGlass->setBoundingRect(QRectF(40, 40, 80, 80));
    frontGlass->setNeedsBackdrop(true);
    root.appendChild(wall);
    root.appendChild(backGlass);
    root.appendChild(frontGlass);

    TestTracker tracker(&root);
    tracker.commit();
    wall->markContentDirty(QRect(5, 5, 10, 10));
    tracker.commit();

    COMPARE_REGION(WPixmanRegion(backGlass->behindDamageRegion()) & backGlass->worldBounds(),
                   QRegion(5, 5, 10, 10));
    COMPARE_REGION(WPixmanRegion(frontGlass->behindDamageRegion()) & frontGlass->worldBounds(),
                   QRegion());
}

void WSGDamageGraphTest::stackedBackdropsIndependentCopySources()
{
    WSGDamageNode root;
    auto *wall = new WSGDamageGeometryNode;
    wall->setBoundingRect(QRectF(0, 0, 200, 100));
    wall->setFullyOpaque(true);
    auto *left = new WSGDamageBackdropNode;
    left->setBoundingRect(QRectF(0, 0, 80, 80));
    auto *overlap = new WSGDamageBackdropNode;
    overlap->setBoundingRect(QRectF(40, 40, 80, 80));
    auto *right = new WSGDamageBackdropNode;
    right->setBoundingRect(QRectF(140, 20, 50, 50));
    root.appendChild(wall);
    root.appendChild(left);
    root.appendChild(overlap);
    root.appendChild(right);

    TestTracker tracker(&root);
    // One simulated output: its address keys the per-output recopy debt.
    int outputStorage = 0;
    const void *output = &outputStorage;
    tracker.commit();
    // Simulate the renderer consuming every cache refresh after the settle.
    for (auto *glass : { left, overlap, right })
        glass->consumeRecopy(glass->pendingRecopy(output), output);

    wall->markContentDirty(QRect(50, 50, 10, 10));
    tracker.commit();
    COMPARE_REGION(WPixmanRegion(*WSGDamageNodeTestAccess::pendingRecopy(left, output)),
                   QRegion(50, 50, 10, 10));
    COMPARE_REGION(WPixmanRegion(*WSGDamageNodeTestAccess::pendingRecopy(overlap, output)),
                   QRegion(50, 50, 10, 10));
    COMPARE_REGION(WPixmanRegion(*WSGDamageNodeTestAccess::pendingRecopy(right, output)),
                   QRegion());

    wall->markContentDirty(QRect(150, 30, 8, 8));
    tracker.commit();
    COMPARE_REGION(WPixmanRegion(*WSGDamageNodeTestAccess::pendingRecopy(left, output)),
                   QRegion(50, 50, 10, 10));
    COMPARE_REGION(WPixmanRegion(*WSGDamageNodeTestAccess::pendingRecopy(overlap, output)),
                   QRegion(50, 50, 10, 10));
    COMPARE_REGION(WPixmanRegion(*WSGDamageNodeTestAccess::pendingRecopy(right, output)),
                   QRegion(150, 30, 8, 8));
}

void WSGDamageGraphTest::backdropSeesRemovedSiblingHole()
{
    // Regression: a node removed BEHIND a backdrop sibling leaves its hole on
    // the grouping parent. Grouping ownDamage must land before children so the
    // backdrop's copy source includes the hole; otherwise the blitter keeps
    // stale pixels of the removed content.
    WSGDamageNode root;
    auto *group = new WSGDamageNode;
    auto *back = new WSGDamageGeometryNode;
    back->setBoundingRect(QRectF(20, 20, 40, 40));
    auto *backdrop = new WSGDamageBackdropNode;
    backdrop->setBoundingRect(QRectF(10, 10, 60, 60));
    backdrop->setNeedsBackdrop(true);
    group->appendChild(back);
    group->appendChild(backdrop);
    root.appendChild(group);

    TestTracker tracker(&root);
    int outputStorage = 0;
    const void *output = &outputStorage;
    tracker.commit();
    backdrop->consumeRecopy(backdrop->pendingRecopy(output), output);

    group->removeChild(back);
    tracker.commit();
    COMPARE_REGION(WPixmanRegion(*WSGDamageNodeTestAccess::pendingRecopy(backdrop, output)),
                   QRegion(20, 20, 40, 40));
    COMPARE_REGION(tracker.commit(), QRegion());
}

void WSGDamageGraphTest::backgroundDamageRecopySurvivesRepeatedCommit()
{
    WSGDamageNode root;
    // Simulated background node injected at the front of a downstream source tree
    auto *background = new WSGDamageGeometryNode;
    background->setBoundingRect(QRectF(0, 0, 800, 600));
    root.appendChild(background);

    // Opaque card occluding a portion of the background
    auto *opaqueCard = new WSGDamageGeometryNode;
    opaqueCard->setBoundingRect(QRectF(100, 100, 200, 200));
    opaqueCard->setFullyOpaque(true);
    root.appendChild(opaqueCard);

    // Backdrop/glass node sampling the background
    auto *glass = new WSGDamageBackdropNode;
    glass->setBoundingRect(QRectF(350, 100, 150, 150));
    glass->setRecopyExpansion(QMargins(10, 10, 10, 10));
    root.appendChild(glass);

    WSGDamageTracker tracker(&root);
    WSGViewport viewport;
    int outputStorage = 0;
    const void *output = &outputStorage;

    // Initial frame
    tracker.commit(viewport);
    tracker.finishFrame();
    viewport.finishFrame();
    glass->consumeRecopy(glass->pendingRecopy(output), output);
    // Dirty regions in the preceding source background:
    // Region A: (150, 150, 50, 50) wholly under opaque card
    // Region B: (380, 120, 40, 40) under glass node
    // Region C: (600, 400, 50, 50) in clear background
    const QRegion dirtA(150, 150, 50, 50);
    const QRegion dirtB(380, 120, 40, 40);
    const QRegion dirtC(600, 400, 50, 50);
    background->markContentDirty(dirtA + dirtB + dirtC);

    tracker.commit(viewport);
    const WPixmanRegion damage = viewport.damageRegion().region;

    // 1. Occlusion: dirtA must NOT appear in damage
    QVERIFY((damage & WPixmanRegion(dirtA.boundingRect())).isEmpty());

    // 2. Clear background: dirtC must appear in damage
    COMPARE_REGION(damage & WPixmanRegion(dirtC.boundingRect()), dirtC);

    // 3. Glass recopy: glass must have received dirtB as recopy debt
    const WPixmanRegion recopyDebt = glass->pendingRecopy(output);
    COMPARE_REGION(recopyDebt, dirtB);

    // 4. Repeated commit in the same frame is idempotent
    tracker.commit(viewport);
    COMPARE_REGION(viewport.damageRegion().region, damage);

    tracker.finishFrame();
    viewport.finishFrame();
    glass->consumeRecopy(recopyDebt, output);

    // Next idle frame produces no damage and no recopy debt
    tracker.commit(viewport);
    const WPixmanRegion idleDamage = viewport.damageRegion().region;
    tracker.finishFrame();
    viewport.finishFrame();
    QVERIFY(idleDamage.isEmpty());
    QVERIFY(glass->pendingRecopy(output).isEmpty());
}

void WSGDamageGraphTest::multiOutputDisjointSceneMapping()
{
    WSGDamageNode root;
    auto *itemScreen1 = new WSGDamageGeometryNode;
    itemScreen1->setBoundingRect(QRectF(100, 100, 80, 80));
    root.appendChild(itemScreen1);

    auto *itemScreen2 = new WSGDamageGeometryNode;
    itemScreen2->setBoundingRect(QRectF(2100, 100, 80, 80));
    root.appendChild(itemScreen2);

    WSGDamageTracker tracker(&root);
    const QSize buf(1920, 1080);
    WSGViewport viewport1;
    WSGViewport viewport2;
    viewport2.setRenderParameters(QMatrix4x4(), QRectF(1920, 0, 1920, 1080));
    viewport1.finishFrame();
    viewport2.finishFrame();

    auto bufferDamage = [&](const WSGViewport &v) {
        WPixmanRegion r = v.damageRegion().region;
        const QRect clip = v.sceneOutputRect(buf);
        if (!clip.isEmpty())
            r &= clip;
        return r.mappedOuter(v.sceneToBufferTransform(buf));
    };
    auto commitOutputs = [&] {
        tracker.commit(viewport1);
        tracker.commit(viewport2);
        tracker.finishFrame();
    };

    commitOutputs();
    QVERIFY(viewport1.affectsBuffer(buf));
    QVERIFY(viewport2.affectsBuffer(buf));
    COMPARE_REGION(bufferDamage(viewport1), QRegion(100, 100, 80, 80));
    COMPARE_REGION(bufferDamage(viewport2), QRegion(180, 100, 80, 80));
    viewport1.finishFrame();

    itemScreen1->setBoundingRect(QRectF(150, 100, 80, 80));
    commitOutputs();
    QVERIFY(viewport1.affectsBuffer(buf));
    QVERIFY(viewport2.affectsBuffer(buf));
    COMPARE_REGION(bufferDamage(viewport1), QRegion(100, 100, 130, 80));
    COMPARE_REGION(bufferDamage(viewport2), QRegion(180, 100, 80, 80));
    viewport1.finishFrame();

    itemScreen2->setBoundingRect(QRectF(2200, 100, 80, 80));
    commitOutputs();
    QVERIFY(!viewport1.affectsBuffer(buf));
    QVERIFY(viewport2.affectsBuffer(buf));
    COMPARE_REGION(bufferDamage(viewport2),
                   QRegion(180, 100, 80, 80) + QRegion(280, 100, 80, 80));
    viewport1.finishFrame();

    itemScreen2->setBoundingRect(QRectF(2300, 100, 80, 80));
    commitOutputs();
    QVERIFY(!viewport1.affectsBuffer(buf));
    QVERIFY(viewport2.affectsBuffer(buf));
    COMPARE_REGION(bufferDamage(viewport2),
                   QRegion(180, 100, 80, 80) + QRegion(280, 100, 80, 80)
                       + QRegion(380, 100, 80, 80));
    viewport2.finishFrame();

    auto *straddle = new WSGDamageGeometryNode;
    straddle->setBoundingRect(QRectF(1900, 200, 40, 40));
    root.appendChild(straddle);
    commitOutputs();
    QVERIFY(viewport1.affectsBuffer(buf));
    QVERIFY(viewport2.affectsBuffer(buf));
    COMPARE_REGION(bufferDamage(viewport1), QRegion(1900, 200, 20, 40));
    COMPARE_REGION(bufferDamage(viewport2), QRegion(0, 200, 20, 40));
}

void WSGDamageGraphTest::highDpiScaledOutputMapping()
{
    WSGDamageNode root;
    auto *item = new WSGDamageGeometryNode;
    item->setBoundingRect(QRectF(50, 60, 100, 80));
    root.appendChild(item);

    WSGDamageTracker tracker(&root);
    WSGViewport viewport;
    viewport.setDevicePixelRatio(2.0);
    viewport.finishFrame();
    const QSize buf(3840, 2160);
    auto mapped = [&] {
        WPixmanRegion r = viewport.damageRegion().region;
        const QRect clip = viewport.sceneOutputRect(buf);
        if (!clip.isEmpty())
            r &= clip;
        return r.mappedOuter(viewport.sceneToBufferTransform(buf));
    };

    tracker.commit(viewport);
    QVERIFY(viewport.affectsBuffer(buf));
    COMPARE_REGION(viewport.damageRegion().region, QRegion(50, 60, 100, 80));
    COMPARE_REGION(mapped(), QRegion(100, 120, 200, 160));
    tracker.finishFrame();
    viewport.finishFrame();

    item->setBoundingRect(QRectF(100, 60, 100, 80));
    auto *edge = new WSGDamageGeometryNode;
    edge->setBoundingRect(QRectF(1900, 1040, 80, 80));
    root.appendChild(edge);
    auto *outside = new WSGDamageGeometryNode;
    outside->setBoundingRect(QRectF(2200, 1200, 80, 80));
    root.appendChild(outside);
    tracker.commit(viewport);
    QVERIFY(viewport.affectsBuffer(buf));
    COMPARE_REGION(mapped(), QRegion(100, 120, 300, 160) + QRegion(3800, 2080, 40, 80));
    tracker.finishFrame();
    viewport.finishFrame();

    viewport.setDevicePixelRatio(1.5);
    QVERIFY(viewport.isFull());
    tracker.commit(viewport);
    tracker.finishFrame();
    QVERIFY(viewport.isFull());
    QVERIFY(viewport.affectsBuffer(buf));
    viewport.finishFrame();
    QVERIFY(!viewport.isFull());

    item->markContentDirty(QRect(0, 0, 100, 80));
    auto *fractionalEdge = new WSGDamageGeometryNode;
    fractionalEdge->setBoundingRect(QRectF(2550, 1430, 40, 40));
    root.appendChild(fractionalEdge);
    tracker.commit(viewport);
    QVERIFY(viewport.affectsBuffer(buf));
    COMPARE_REGION(mapped(), QRegion(150, 90, 150, 120) + QRegion(3825, 2145, 15, 15));
}

void WSGDamageGraphTest::rotatedOutputMapping()
{
    WSGDamageNode root;
    auto *item = new WSGDamageGeometryNode;
    item->setBoundingRect(QRectF(100, 200, 50, 60));
    root.appendChild(item);

    QMatrix4x4 rotation;
    rotation.translate(1920, 0);
    rotation.rotate(90, 0, 0, 1);
    WSGViewport viewport;
    viewport.setRenderParameters(rotation);
    viewport.finishFrame();
    const QSize buf(1920, 1080);
    auto mapped = [&] {
        WPixmanRegion r = viewport.damageRegion().region;
        const QRect clip = viewport.sceneOutputRect(buf);
        if (!clip.isEmpty())
            r &= clip;
        return r.mappedOuter(viewport.sceneToBufferTransform(buf));
    };
    WSGDamageTracker tracker(&root);

    tracker.commit(viewport);
    QVERIFY(viewport.affectsBuffer(buf));
    COMPARE_REGION(mapped(), QRegion(1660, 100, 60, 50));
    tracker.finishFrame();
    viewport.finishFrame();

    auto *lower = new WSGDamageGeometryNode;
    lower->setBoundingRect(QRectF(100, 1200, 50, 60));
    root.appendChild(lower);
    auto *edge = new WSGDamageGeometryNode;
    edge->setBoundingRect(QRectF(1060, 1900, 50, 60));
    root.appendChild(edge);
    auto *outside = new WSGDamageGeometryNode;
    outside->setBoundingRect(QRectF(1500, 100, 50, 60));
    root.appendChild(outside);
    tracker.commit(viewport);
    QVERIFY(viewport.affectsBuffer(buf));
    COMPARE_REGION(mapped(), QRegion(660, 100, 60, 50) + QRegion(0, 1060, 20, 20));
}

void WSGDamageGraphTest::singularOutputMapping()
{
    WSGDamageNode root;
    auto *item = new WSGDamageGeometryNode;
    item->setBoundingRect(QRectF(100, 100, 80, 80));
    root.appendChild(item);

    WSGViewport viewport;
    QMatrix4x4 singular;
    singular.scale(0, 1);
    viewport.setRenderParameters(singular);
    const QSize buf(1920, 1080);
    WSGDamageTracker tracker(&root);

    tracker.commit(viewport);
    QVERIFY(viewport.isFull());
    QVERIFY(viewport.affectsBuffer(buf));
    tracker.finishFrame();
    QVERIFY(viewport.isFull());
    viewport.finishFrame();

    viewport.setRenderParameters(QMatrix4x4());
    QVERIFY(viewport.isFull());
    QVERIFY(viewport.affectsBuffer(buf));
    tracker.commit(viewport);
    tracker.finishFrame();
    viewport.finishFrame();
    item->markContentDirty(QRect(10, 10, 10, 10));
    tracker.commit(viewport);
    QVERIFY(!viewport.isFull());
    QVERIFY(viewport.affectsBuffer(buf));
    COMPARE_REGION(viewport.damageRegion().region, QRegion(110, 110, 10, 10));
}

QTEST_MAIN(WSGDamageGraphTest)
#include "tst_wdamagegraph.moc"
