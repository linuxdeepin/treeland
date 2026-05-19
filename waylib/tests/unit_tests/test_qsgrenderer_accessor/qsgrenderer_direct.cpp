// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

// This translation unit intentionally uses the "#define private public" technique
// to expose QSGRenderer's private bit-field members for cross-verification in
// layout tests.
//
// Strategy: pre-include all of qsgrenderer_p.h's transitive dependencies first,
// so their include guards are already set.  The "#define private public" block then
// only opens up the QSGRenderer class body itself — keeping the blast radius small.

#include <QtQuick/QQuickItem>
#include <QtQuick/QSGNode>
#include <private/qsgcontext_p.h>
#include <private/qsgnode_p.h>

// --- The hack: expose QSGRenderer's private section -------------------------
// Everything transitively included by qsgrenderer_p.h is already guarded above.
// The redefinition of 'private' therefore only affects the QSGRenderer class body.
#define private public
#include <private/qsgrenderer_p.h>
#undef private
// ----------------------------------------------------------------------------

QT_USE_NAMESPACE

// These functions provide direct access to QSGRenderer's private bit fields.
// They are declared extern "C" so that main.cpp can reference them without
// needing to agree on the (ODR-violated) QSGRenderer class definition.
// void* parameters avoid any class-type ODR issues at the call sites.

extern "C" {

int direct_qsgrenderer_get_is_rendering(const void *obj)
{
    return static_cast<const QSGRenderer *>(obj)->m_is_rendering;
}

int direct_qsgrenderer_get_changed_emitted(const void *obj)
{
    return static_cast<const QSGRenderer *>(obj)->m_changed_emitted;
}

void direct_qsgrenderer_set_is_rendering(void *obj, bool v)
{
    static_cast<QSGRenderer *>(obj)->m_is_rendering = v;
}

void direct_qsgrenderer_set_changed_emitted(void *obj, bool v)
{
    static_cast<QSGRenderer *>(obj)->m_changed_emitted = v;
}

} // extern "C"
