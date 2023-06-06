// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutput.h"
#include "wbackend.h"
#include "wcursor.h"
#include "wseat.h"
#include "platformplugin/qwlrootscreen.h"

#include <qwoutput.h>
#include <qwoutputlayout.h>
#include <qwrenderer.h>

extern "C" {
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
}

#include <QDebug>
#include <QCoreApplication>
#include <QQuickWindow>
#include <QCursor>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WOutputPrivate : public WObjectPrivate
{
public:
    WOutputPrivate(WOutput *qq, void *handle)
        : WObjectPrivate(qq)
        , handle(reinterpret_cast<QWOutput*>(handle))
    {
        this->handle->handle()->data = qq;

        QObject::connect(this->handle.get(), qOverload<wlr_output_event_commit*>(&QWOutput::commit),
                         qq, [qq] (wlr_output_event_commit *event) {
            if (event->committed & WLR_OUTPUT_STATE_SCALE) {
                Q_EMIT qq->scaleChanged();
                Q_EMIT qq->effectiveSizeChanged();
            }

            if (event->committed & WLR_OUTPUT_STATE_MODE) {
                Q_EMIT qq->sizeChanged();
                Q_EMIT qq->effectiveSizeChanged();
            }

            if (event->committed & WLR_OUTPUT_STATE_TRANSFORM) {
                Q_EMIT qq->orientationChanged();
                Q_EMIT qq->transformedSizeChanged();
                Q_EMIT qq->effectiveSizeChanged();
            }
        });
    }

    ~WOutputPrivate() {
        handle->handle()->data = nullptr;
        if (layout)
            layout->remove(q_func());
    }

    inline QSize size() const {
        Q_ASSERT(handle);
        return QSize(handle->handle()->width, handle->handle()->height);
    }

    inline WOutput::Transform orientation() const {
        return static_cast<WOutput::Transform>(handle->handle()->transform);
    }

    W_DECLARE_PUBLIC(WOutput)

    QPointer<QWOutput> handle;
    bool forceSoftwareCursor = false;
    QWlrootsScreen *screen = nullptr;
    QQuickWindow *window = nullptr;

    WBackend *backend = nullptr;
    WOutputLayout *layout = nullptr;
};

WOutput::WOutput(WOutputViewport *handle, WBackend *backend)
    : QObject()
    , WObject(*new WOutputPrivate(this, handle))
{
    d_func()->backend = backend;
}

WOutput::~WOutput()
{

}

WBackend *WOutput::backend() const
{
    W_DC(WOutput);
    return d->backend;
}

WServer *WOutput::server() const
{
    W_DC(WOutput);
    return d->backend->server();
}

QWRenderer *WOutput::renderer() const
{
    W_DC(WOutput);
    return QWRenderer::from(d->handle->handle()->renderer);
}

WOutputViewport *WOutput::handle() const
{
    W_DC(WOutput);
    return reinterpret_cast<WOutputViewport*>(d->handle.data());
}

WOutput *WOutput::fromHandle(const WOutputViewport *handle)
{
    auto wlr_handle = reinterpret_cast<const QWOutput*>(handle)->handle();
    return reinterpret_cast<WOutput*>(wlr_handle->data);
}

WOutput *WOutput::fromScreen(const QScreen *screen)
{
    return fromHandle(static_cast<QWlrootsScreen*>(screen->handle())->output());
}

void WOutput::setScreen(QWlrootsScreen *screen)
{
    W_D(WOutput);
    d->screen = screen;
}

QWlrootsScreen *WOutput::screen() const
{
    W_DC(WOutput);
    return d->screen;
}

void WOutput::rotate(Transform t)
{
    W_D(WOutput);

    if (d->orientation() == t)
        return;

    d->handle->setTransform(t);
}

void WOutput::setScale(float scale)
{
    W_D(WOutput);

    if (this->scale() == scale)
        return;

    d->handle->setScale(scale);
}

QPoint WOutput::position() const
{
    W_DC(WOutput);

    QPoint p;

    if (Q_UNLIKELY(!d->layout))
        return p;

    auto l_output = d->layout->get(d->handle->handle());

    if (Q_UNLIKELY(!l_output))
        return p;

    return QPoint(l_output->x, l_output->y);
}

QSize WOutput::size() const
{
    W_DC(WOutput);

    return d->size();
}

QSize WOutput::transformedSize() const
{
    W_DC(WOutput);

    return d->handle->transformedResolution();
}

QSize WOutput::effectiveSize() const
{
    W_DC(WOutput);

    return d->handle->effectiveResolution();
}

WOutput::Transform WOutput::orientation() const
{
    W_DC(WOutput);

    return d->orientation();
}

float WOutput::scale() const
{
    W_DC(WOutput);

    return d->handle->handle()->scale;
}

void WOutput::attach(QQuickWindow *window)
{
    W_D(WOutput);
    d->window = window;
}

QQuickWindow *WOutput::attachedWindow() const
{
    W_DC(WOutput);
    return d->window;
}

void WOutput::setLayout(WOutputLayout *layout)
{
    W_D(WOutput);

    if (d->layout == layout)
        return;

    d->layout = layout;
}

WOutputLayout *WOutput::layout() const
{
    W_DC(WOutput);

    return d->layout;
}

void WOutput::addCursor(WCursor *cursor)
{
    static_cast<QWlrootsCursor*>(screen()->cursor())->addCursor(cursor);
}

void WOutput::removeCursor(WCursor *cursor)
{
    static_cast<QWlrootsCursor*>(screen()->cursor())->removeCursor(cursor);
}

QList<WCursor *> WOutput::cursorList() const
{
    return static_cast<QWlrootsCursor*>(screen()->cursor())->cursors;
}

bool WOutput::forceSoftwareCursor() const
{
    W_DC(WOutput);
    return d->forceSoftwareCursor;
}

void WOutput::setForceSoftwareCursor(bool on)
{
    W_D(WOutput);
    if (d->forceSoftwareCursor == on)
        return;
    d->forceSoftwareCursor = on;
    d->handle->lockSoftwareCursors(on);

    Q_EMIT forceSoftwareCursorChanged();
}

WAYLIB_SERVER_END_NAMESPACE
