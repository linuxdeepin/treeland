// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

// Unit test: cross-verify W_PRIVATE_BF_SET/GET against direct member access.
//
// The bit-field accessor in wrenderbuffernode.cpp reaches QSGRenderer's private
// bit fields (m_changed_emitted, m_is_rendering) via memory arithmetic from the
// preceding member (m_nodes_dont_preprocess).  This test verifies:
//
//   1. Accessor write  → direct read  : W_PRIVATE_BF_SET sets the correct field.
//   2. Direct write    → accessor read: W_PRIVATE_BF_GET reads the correct field.
//   3. Bit isolation   : setting one bit does not affect the other.
//
// If Qt changes the bit-field declaration order or inserts a member between
// m_nodes_dont_preprocess and the bit fields, one or more tests below will fail.
//
// Implementation note: QSGRenderer is abstract; constructing it requires a live
// QSGRenderContext.  For layout tests we only need pointer arithmetic on raw
// memory — no virtual dispatch, no initialised members.  A zero-filled,
// correctly-aligned buffer is sufficient.

#include <QTest>
#include <private/qsgrenderer_p.h>
#include "private/wprivateaccessor_p.h"

// Register tag structs with names distinct from the ones in wrenderbuffernode.cpp
// to avoid duplicate explicit instantiations when the test links to WaylibServer.
W_DECLARE_PRIVATE_BITFIELD(Test_QSGRendererBF,
                            QSGRenderer, m_nodes_dont_preprocess,
                            QSet<QSGNode *>, uint);

// Bit positions must match the declaration order in qsgrenderer_p.h:
//   uint m_changed_emitted : 1;  → bit 0
//   uint m_is_rendering    : 1;  → bit 1
static constexpr unsigned kChangedEmittedBit = 0;
static constexpr unsigned kIsRenderingBit    = 1;

// Direct-access helpers from qsgrenderer_direct.cpp (#define private public).
// extern "C" + void* avoids any ODR issue with the QSGRenderer class definition.
extern "C" {
    int  direct_qsgrenderer_get_is_rendering(const void *obj);
    int  direct_qsgrenderer_get_changed_emitted(const void *obj);
    void direct_qsgrenderer_set_is_rendering(void *obj, bool v);
    void direct_qsgrenderer_set_changed_emitted(void *obj, bool v);
}

class QSGRendererAccessorTest : public QObject
{
    Q_OBJECT

    // Raw buffer: same size and alignment as QSGRenderer, zero-initialised.
    // No construction — we only test offset arithmetic and bit masking,
    // neither of which requires a valid vtable or initialised data members.
    alignas(QSGRenderer) char m_buf[sizeof(QSGRenderer)];
    QSGRenderer *r;

public:
    explicit QSGRendererAccessorTest(QObject *parent = nullptr)
        : QObject(parent)
        , r(reinterpret_cast<QSGRenderer *>(m_buf))
    {}

private Q_SLOTS:
    // Reset buffer to all-zeros before each sub-test.
    void init() { memset(m_buf, 0, sizeof(m_buf)); }

    // --- Accessor write → direct read --------------------------------------

    void test_set_isRendering_visible_via_direct()
    {
        W_PRIVATE_BF_SET(*r, Test_QSGRendererBF, kIsRenderingBit, true);
        QCOMPARE(direct_qsgrenderer_get_is_rendering(r), 1);
        QCOMPARE(direct_qsgrenderer_get_changed_emitted(r), 0); // other bit untouched
    }

    void test_set_changedEmitted_visible_via_direct()
    {
        W_PRIVATE_BF_SET(*r, Test_QSGRendererBF, kChangedEmittedBit, true);
        QCOMPARE(direct_qsgrenderer_get_changed_emitted(r), 1);
        QCOMPARE(direct_qsgrenderer_get_is_rendering(r), 0);    // other bit untouched
    }

    // --- Direct write → accessor read --------------------------------------

    void test_read_isRendering_via_accessor()
    {
        direct_qsgrenderer_set_is_rendering(r, true);
        QCOMPARE(W_PRIVATE_BF_GET(*r, Test_QSGRendererBF, kIsRenderingBit),    true);
        QCOMPARE(W_PRIVATE_BF_GET(*r, Test_QSGRendererBF, kChangedEmittedBit), false);
    }

    void test_read_changedEmitted_via_accessor()
    {
        direct_qsgrenderer_set_changed_emitted(r, true);
        QCOMPARE(W_PRIVATE_BF_GET(*r, Test_QSGRendererBF, kChangedEmittedBit), true);
        QCOMPARE(W_PRIVATE_BF_GET(*r, Test_QSGRendererBF, kIsRenderingBit),    false);
    }

    // --- Clear bits via accessor -------------------------------------------

    void test_clear_isRendering_via_accessor()
    {
        direct_qsgrenderer_set_is_rendering(r, true);
        direct_qsgrenderer_set_changed_emitted(r, true);

        W_PRIVATE_BF_SET(*r, Test_QSGRendererBF, kIsRenderingBit, false);

        QCOMPARE(direct_qsgrenderer_get_is_rendering(r),    0);
        QCOMPARE(direct_qsgrenderer_get_changed_emitted(r), 1); // unaffected
    }
};

QTEST_MAIN(QSGRendererAccessorTest)
#include "main.moc"
