// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtTest>

#include "qwdisplay.h"

class Q_DECL_HIDDEN testQWObject : public QObject
{
    Q_OBJECT
public:
    testQWObject();
    ~testQWObject() override = default;

private Q_SLOTS:
    void testcase();

private:
    qw_display* nullObject;
};

testQWObject::testQWObject()
    : nullObject(nullptr)
{
}

void testQWObject::testcase() {
    QCOMPARE(nullObject->is_valid(), false);
}

QTEST_APPLESS_MAIN(testQWObject)

#include "test_qwobject.moc"
