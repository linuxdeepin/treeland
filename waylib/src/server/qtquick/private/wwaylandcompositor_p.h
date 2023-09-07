// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wquickwaylandserver.h>
#include <qwglobal.h>

Q_MOC_INCLUDE(<wquickbackend_p.h>)
Q_MOC_INCLUDE(<qwrenderer.h>)
Q_MOC_INCLUDE(<qwallocator.h>)
Q_MOC_INCLUDE(<qwcompositor.h>)
Q_MOC_INCLUDE(<qwsubcompositor.h>)

QW_BEGIN_NAMESPACE
class QWRenderer;
class QWAllocator;
class QWCompositor;
class QWSubcompositor;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WQuickBackend;
class WWaylandCompositorPrivate;
class WAYLIB_SERVER_EXPORT WWaylandCompositor : public WQuickWaylandServerInterface, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WWaylandCompositor)
    Q_PROPERTY(WQuickBackend *backend READ backend WRITE setBackend REQUIRED)
    Q_PROPERTY(QW_NAMESPACE::QWRenderer *renderer READ renderer NOTIFY rendererChanged)
    Q_PROPERTY(QW_NAMESPACE::QWAllocator *allocator READ allocator NOTIFY allocatorChanged)
    Q_PROPERTY(QW_NAMESPACE::QWCompositor *compositor READ compositor NOTIFY compositorChanged)
    Q_PROPERTY(QW_NAMESPACE::QWSubcompositor *subcompositor READ subcompositor NOTIFY subcompositorChanged)
    QML_NAMED_ELEMENT(WaylandCompositor)

public:
    explicit WWaylandCompositor(QObject *parent = nullptr);

    WQuickBackend *backend() const;
    void setBackend(WQuickBackend *newBackend);

    QW_NAMESPACE::QWRenderer *renderer() const;
    QW_NAMESPACE::QWAllocator *allocator() const;
    QW_NAMESPACE::QWCompositor *compositor() const;
    QW_NAMESPACE::QWSubcompositor *subcompositor() const;

Q_SIGNALS:
    void rendererChanged();
    void allocatorChanged();
    void compositorChanged();
    void subcompositorChanged();

private:
    void create() override;
};

WAYLIB_SERVER_END_NAMESPACE
