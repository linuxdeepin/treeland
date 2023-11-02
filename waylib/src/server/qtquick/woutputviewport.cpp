// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutputviewport.h"
#include "woutputviewport_p.h"
#include "woutput.h"

#include <qwbuffer.h>
#include <qwswapchain.h>

#include <QDebug>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

void WOutputViewportPrivate::init()
{
    Q_ASSERT(!renderBuffer);
    Q_Q(WOutputViewport);

    renderBuffer = new WBufferRenderer(q);
    QQuickItemPrivate::get(renderBuffer)->anchors()->setFill(q);
    QObject::connect(renderBuffer, &WBufferRenderer::cacheBufferChanged,
                     q, &WOutputViewport::cacheBufferChanged);
}

void WOutputViewportPrivate::initForOutput()
{
    W_Q(WOutputViewport);

    if (root) {
        renderBuffer->setSource(q, true);
    } else {
        renderBuffer->setSource(nullptr, true);
    }
    renderBuffer->setOutput(output);
    outputWindow()->attach(q);

    QObject::connect(output, &WOutput::modeChanged, q, [this] {
        updateImplicitSize();
    });

    updateImplicitSize();
}

qreal WOutputViewportPrivate::getImplicitWidth() const
{
    return output->size().width() / devicePixelRatio;
}

qreal WOutputViewportPrivate::getImplicitHeight() const
{
    return output->size().height() / devicePixelRatio;
}

void WOutputViewportPrivate::updateImplicitSize()
{
    implicitWidthChanged();
    implicitHeightChanged();

    W_Q(WOutputViewport);
    q->resetWidth();
    q->resetHeight();
}

WOutputViewport::WOutputViewport(QQuickItem *parent)
    : QQuickItem(*new WOutputViewportPrivate(), parent)
{
    d_func()->init();
}

WOutputViewport::~WOutputViewport()
{
    invalidate();
}

void WOutputViewport::invalidate()
{
    W_D(WOutputViewport);
    if (d->componentComplete && d->output && d->window) {
        d->outputWindow()->detach(this);
        d->output = nullptr;
    }
}

bool WOutputViewport::isTextureProvider() const
{
    W_DC(WOutputViewport);
    if (QQuickItem::isTextureProvider())
        return true;

    return d->renderBuffer->isTextureProvider();
}

QSGTextureProvider *WOutputViewport::textureProvider() const
{
    W_DC(WOutputViewport);
    if (auto tp = QQuickItem::textureProvider())
        return tp;

    return d->renderBuffer->textureProvider();
}

WOutput *WOutputViewport::output() const
{
    W_D(const WOutputViewport);
    return d->output;
}

void WOutputViewport::setOutput(WOutput *newOutput)
{
    W_D(WOutputViewport);

    Q_ASSERT(!d->output || !newOutput);

    if (d->output && newOutput) {
        qmlWarning(this) << "The \"output\" property is non-null, Not allow change it.";
        return;
    }

    d->output = newOutput;

    if (d->componentComplete) {
        if (newOutput)
            d->initForOutput();
    }
}

qreal WOutputViewport::devicePixelRatio() const
{
    W_DC(WOutputViewport);
    return d->devicePixelRatio;
}

void WOutputViewport::setDevicePixelRatio(qreal newDevicePixelRatio)
{
    W_D(WOutputViewport);

    if (qFuzzyCompare(d->devicePixelRatio, newDevicePixelRatio))
        return;
    d->devicePixelRatio = newDevicePixelRatio;

    if (d->output)
        d->updateImplicitSize();

    Q_EMIT devicePixelRatioChanged();
}

bool WOutputViewport::offscreen() const
{
    W_DC(WOutputViewport);
    return d->offscreen;
}

void WOutputViewport::setOffscreen(bool newOffscreen)
{
    W_D(WOutputViewport);
    if (d->offscreen == newOffscreen)
        return;
    d->offscreen = newOffscreen;
    Q_EMIT offscreenChanged();
}

bool WOutputViewport::isRoot() const
{
    W_DC(WOutputViewport);
    return d->root;
}

void WOutputViewport::setRoot(bool newRoot)
{
    W_D(WOutputViewport);
    if (d->root == newRoot)
        return;
    d->root = newRoot;

    if (d->output) {
        if (newRoot) {
            d->renderBuffer->setSource(this, true);
        } else if (d->output) {
            d->renderBuffer->setSource(nullptr, true);
        }
    }

    Q_EMIT rootChanged();
}

bool WOutputViewport::cacheBuffer() const
{
    W_DC(WOutputViewport);
    return d->renderBuffer->cacheBuffer();
}

void WOutputViewport::setCacheBuffer(bool newCacheBuffer)
{
    W_D(WOutputViewport);
    d->renderBuffer->setCacheBuffer(newCacheBuffer);
}

WOutputViewport::LayerFlags WOutputViewport::layerFlags() const
{
    W_DC(WOutputViewport);
    return d->layerFlags;
}

void WOutputViewport::setLayerFlags(const LayerFlags &newLayerFlags)
{
    W_D(WOutputViewport);
    if (d->layerFlags == newLayerFlags)
        return;
    d->layerFlags = newLayerFlags;
    Q_EMIT layerFlagsChanged();
}

void WOutputViewport::setOutputScale(float scale)
{
    W_D(WOutputViewport);
    if (auto window = d->outputWindow())
        window->setOutputScale(this, scale);
}

void WOutputViewport::rotateOutput(WOutput::Transform t)
{
    W_D(WOutputViewport);
    if (auto window = d->outputWindow())
        window->rotateOutput(this, t);
}

void WOutputViewport::componentComplete()
{
    W_D(WOutputViewport);

    if (d->output)
        d->initForOutput();

    QQuickItem::componentComplete();
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_woutputviewport.cpp"
