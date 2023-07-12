// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wwaylandcompositor_p.h"
#include "woutputhelper.h"
#include "wquickbackend_p.h"

#include <qwrenderer.h>
#include <qwallocator.h>
#include <qwcompositor.h>
#include <qwsubcompositor.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class WWaylandCompositorPrivate : public WObjectPrivate
{
public:
    WWaylandCompositorPrivate(WWaylandCompositor *qq)
        : WObjectPrivate(qq)
    {

    }

    W_DECLARE_PUBLIC(WWaylandCompositor)

    WQuickBackend *backend = nullptr;
    QWRenderer *renderer = nullptr;
    QWAllocator *allocator = nullptr;
};

WWaylandCompositor::WWaylandCompositor(QObject *parent)
    : WQuickWaylandServerInterface(parent)
    , WObject(*new WWaylandCompositorPrivate(this))
{

}

WQuickBackend *WWaylandCompositor::backend() const
{
    W_DC(WWaylandCompositor);
    return d->backend;
}

void WWaylandCompositor::setBackend(WQuickBackend *newBackend)
{
    W_D(WWaylandCompositor);
    Q_ASSERT(!d->backend);
    d->backend = newBackend;
}

QW_NAMESPACE::QWRenderer *WWaylandCompositor::renderer() const
{
    W_DC(WWaylandCompositor);
    return d->renderer;
}

QWAllocator *WWaylandCompositor::allocator() const
{
    W_DC(WWaylandCompositor);
    return d->allocator;
}

void WWaylandCompositor::create()
{
    W_D(WWaylandCompositor);

    Q_ASSERT(d->backend);
    d->renderer = WOutputHelper::createRenderer(d->backend->backend());
    if (!d->renderer) {
        qFatal("Failed to create renderer");
    }

    d->allocator = QWAllocator::autoCreate(d->backend->backend(), d->renderer);
    auto display = server()->nativeInterface<QWDisplay>();
    d->renderer->initWlDisplay(display);

    // free follow display
    Q_UNUSED(QWCompositor::create(display, d->renderer));
    Q_UNUSED(QWSubcompositor::create(display));

    WQuickWaylandServerInterface::create();
}

WAYLIB_SERVER_END_NAMESPACE
