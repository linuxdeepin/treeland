// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "private/wsurfaceitem_p.h"
#include "wsurfaceitem.h"

#include <QCoreApplication>
#include <QEvent>
#include <QPointer>
#include <QQuickItem>
#include <QSignalSpy>
#include <QTest>

WAYLIB_SERVER_USE_NAMESPACE

class WSurfaceItemLifetimeTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void recursiveContainerDeletionClearsRegistry()
    {
        WSurfaceItem parent;
        auto *container = new QQuickItem(&parent);
        auto *first = new WSurfaceItem(container);
        auto *second = new WSurfaceItem(container);
        auto *d = WSurfaceItemPrivate::get(&parent);

        d->subsurfaces.append(first);
        d->subsurfaces.append(second);
        d->trackSubsurfaceItemLifetime(first);
        d->trackSubsurfaceItemLifetime(second);

        QSignalSpy removedSpy(&parent, &WSurfaceItem::subsurfaceRemoved);
        QPointer<QQuickItem> containerGuard(container);
        QPointer<WSurfaceItem> firstGuard(first);
        QPointer<WSurfaceItem> secondGuard(second);

        container->deleteLater();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

        QVERIFY(containerGuard.isNull());
        QVERIFY(firstGuard.isNull());
        QVERIFY(secondGuard.isNull());
        QVERIFY(d->subsurfaces.isEmpty());
        QCOMPARE(removedSpy.count(), 0);

        // These paths iterate the live-subsurface registry in production.
        parent.setFlags(WSurfaceItem::RejectEvent);
        parent.setSize(QSizeF(10, 10));
        QCOMPARE(parent.boundingRect(), QRectF(0, 0, 10, 10));
    }

    void cleanupIsIdempotent()
    {
        WSurfaceItem parent;
        auto *child = new WSurfaceItem;
        auto *d = WSurfaceItemPrivate::get(&parent);

        d->subsurfaces.append(child);
        d->trackSubsurfaceItemLifetime(child);
        QVERIFY(d->subsurfaces.removeOne(child));

        delete child;

        QVERIFY(d->subsurfaces.isEmpty());
    }
};

QTEST_MAIN(WSurfaceItemLifetimeTest)
#include "main.moc"
