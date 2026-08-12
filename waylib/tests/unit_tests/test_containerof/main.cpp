// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

// Runtime tests for the offset-based container_of helper. The compile-time
// guarantee (non-standard-layout T is rejected by static_assert) is verified
// by a negative-compile check wired in CMakeLists.txt (negative_compile.cpp
// must fail to build).

#include <wcontainerof.h>

#include <QtTest>

// wlroots-style struct: a member that is not the first field.
struct WlrStyleStruct {
    int id;
    int destroy;
    double value;
};

// Multiple members of the same type: offsets must be per-member.
struct TwoListeners {
    char pad[3];
    int a;
    int marker;
    int b;
};

class TestContainerOf : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void recoversContainingStruct();
    void memberIsNotFirstField();
    void multipleInstances();
    void twoMembersSameType();
    void rawOffsetMatchesMacro();
};

void TestContainerOf::recoversContainingStruct()
{
    WlrStyleStruct obj { 7, {}, 3.5 };
    auto *self = W_CONTAINER_OF(&obj.destroy, WlrStyleStruct, destroy);
    QCOMPARE(self, &obj);
    QCOMPARE(self->id, 7);
    QCOMPARE(self->value, 3.5);
}

void TestContainerOf::memberIsNotFirstField()
{
    WlrStyleStruct obj { 1, {}, 2.0 };
    // The listener sits after `id`, so a plain cast of the member pointer
    // would be wrong; container_of must subtract the real offset.
    QVERIFY(reinterpret_cast<WlrStyleStruct *>(&obj.destroy) != &obj);
    QCOMPARE(W_CONTAINER_OF(&obj.destroy, WlrStyleStruct, destroy), &obj);
}

void TestContainerOf::multipleInstances()
{
    WlrStyleStruct arr[4] {};
    for (int i = 0; i < 4; ++i)
        arr[i].id = i;

    for (int i = 0; i < 4; ++i)
        QCOMPARE(W_CONTAINER_OF(&arr[i].destroy, WlrStyleStruct, destroy), &arr[i]);
}

void TestContainerOf::twoMembersSameType()
{
    TwoListeners obj {};
    QCOMPARE(W_CONTAINER_OF(&obj.a, TwoListeners, a), &obj);
    QCOMPARE(W_CONTAINER_OF(&obj.b, TwoListeners, b), &obj);
    QVERIFY(W_CONTAINER_OF(&obj.a, TwoListeners, b) != &obj); // wrong member must differ
}

void TestContainerOf::rawOffsetMatchesMacro()
{
    WlrStyleStruct obj {};
    auto *viaMacro = W_CONTAINER_OF(&obj.destroy, WlrStyleStruct, destroy);
    auto *viaRaw = container_of<WlrStyleStruct>(&obj.destroy, offsetof(WlrStyleStruct, destroy));
    QCOMPARE(viaRaw, viaMacro);
    QCOMPARE(viaRaw, &obj);
}

QTEST_GUILESS_MAIN(TestContainerOf)

#include "main.moc"
