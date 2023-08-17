// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutput.h"
#include "wbackend.h"
#include "wcursor.h"
#include "wseat.h"
#include "wtools.h"
#include "platformplugin/qwlrootscreen.h"

#include <qwoutput.h>
#include <qwoutputlayout.h>
#include <qwrenderer.h>
#include <qwswapchain.h>
#include <qwallocator.h>

extern "C" {
#define static
#include <wlr/types/wlr_output.h>
#include <wlr/render/interface.h>
#undef static
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
    WOutputPrivate(WOutput *qq, QWOutput *handle)
        : WObjectPrivate(qq)
        , handle(handle)
    {
        this->handle->setData(this, qq);

        QObject::connect(this->handle.get(), qOverload<wlr_output_event_commit*>(&QWOutput::commit),
                         qq, [qq] (wlr_output_event_commit *event) {
            if (event->committed & WLR_OUTPUT_STATE_SCALE) {
                Q_EMIT qq->scaleChanged();
                Q_EMIT qq->effectiveSizeChanged();
            }

            if (event->committed & WLR_OUTPUT_STATE_MODE) {
                Q_EMIT qq->modeChanged();
                Q_EMIT qq->transformedSizeChanged();
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
        handle->setData(this, nullptr);
        if (layout)
            layout->remove(q_func());
    }

    inline wlr_output *nativeHandle() const {
        Q_ASSERT(handle);
        return handle->handle();
    };

    inline QSize size() const {
        Q_ASSERT(handle);
        return QSize(nativeHandle()->width, nativeHandle()->height);
    }

    inline WOutput::Transform orientation() const {
        return static_cast<WOutput::Transform>(nativeHandle()->transform);
    }

    W_DECLARE_PUBLIC(WOutput)

    QPointer<QWOutput> handle;
    bool forceSoftwareCursor = false;
    QWlrootsScreen *screen = nullptr;
    QQuickWindow *window = nullptr;

    WBackend *backend = nullptr;
    WOutputLayout *layout = nullptr;
};

WOutput::WOutput(QWOutput *handle, WBackend *backend)
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
    return QWRenderer::from(d->nativeHandle()->renderer);
}

QWSwapchain *WOutput::swapchain() const
{
    W_DC(WOutput);
    return QWSwapchain::from(d->nativeHandle()->swapchain);
}

QWAllocator *WOutput::allocator() const
{
    W_DC(WOutput);
    return QWAllocator::from(d->nativeHandle()->allocator);
}

QWOutput *WOutput::handle() const
{
    W_DC(WOutput);
    return d->handle.data();
}

WOutput *WOutput::fromHandle(const QWOutput *handle)
{
    return handle->getData<WOutput>();
}

WOutput *WOutput::fromScreen(const QScreen *screen)
{
    return static_cast<QWlrootsScreen*>(screen->handle())->output();
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

    auto l_output = d->layout->get(d->handle);

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

    return d->nativeHandle()->scale;
}

QImage::Format WOutput::preferredReadFormat() const
{
    W_DC(WOutput);

    auto renderer = d->nativeHandle()->renderer;
    // ###: The wlr_output_preferred_read_format force request
    // attach the renderer to wlr_output, but maybe the renderer
    // is rendering, you will get a crash at renderer_bind_buffer.
    // So, if it's rendering, we direct get the preferred read
    // format of current buffer by renderer->impl->preferred_read_format.
    if (renderer && renderer->rendering) {
        if (!renderer->impl->preferred_read_format
            || !renderer->impl->read_pixels) {
            return QImage::Format_Invalid;
        }

        return WTools::toImageFormat(renderer->impl->preferred_read_format(renderer));
    }

    return WTools::toImageFormat(d->handle->preferredReadFormat());
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
