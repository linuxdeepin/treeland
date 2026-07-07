// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QTest>
#include <QSize>

#include "core/windowconfigstore.h"

class TestWindowConfigStore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testDefaultConstructor();
    void testDefaultConstructorParent();
    void testSaveLastSizeEmptyAppId();
    void testSaveLastSizeInvalidSize();
    void testSaveLastSizeNormal();
    void testSaveLastSizeZeroSize();
    void testSaveLastSizeNegativeSize();
};

void TestWindowConfigStore::testDefaultConstructor()
{
    WindowConfigStore store;
    QCOMPARE(store.parent(), nullptr);
}

void TestWindowConfigStore::testDefaultConstructorParent()
{
    QObject parentObj;
    WindowConfigStore store(&parentObj);
    QCOMPARE(store.parent(), &parentObj);
}

void TestWindowConfigStore::testSaveLastSizeEmptyAppId()
{
    WindowConfigStore store;
    // Empty appId should not crash - early return
    store.saveLastSize("", QSize(100, 100));
}

void TestWindowConfigStore::testSaveLastSizeInvalidSize()
{
    WindowConfigStore store;
    // Invalid (null) size should not crash - early return
    store.saveLastSize("test-app", QSize());
}

void TestWindowConfigStore::testSaveLastSizeNormal()
{
    WindowConfigStore store;
    // Normal case should not crash
    store.saveLastSize("test-app", QSize(800, 600));
}

void TestWindowConfigStore::testSaveLastSizeZeroSize()
{
    WindowConfigStore store;
    // Zero size is valid but has zero dimensions
    store.saveLastSize("test-app", QSize(0, 0));
}

void TestWindowConfigStore::testSaveLastSizeNegativeSize()
{
    WindowConfigStore store;
    // Negative dimensions - should not crash
    store.saveLastSize("test-app", QSize(-1, -1));
}

QTEST_MAIN(TestWindowConfigStore)
#include "main.moc"
