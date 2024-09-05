// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutputviewport.h"
#include "woutputviewport_p.h"
#include "woutput.h"
#include "wsgtextureprovider.h"
#include "wbufferrenderer_p.h"

#include <qwbuffer.h>
#include <qwswapchain.h>

#include <QDebug>
#include <private/qquickitem_p.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

void WOutputViewportPrivate::init()
{
    Q_ASSERT(!bufferRenderer);
    Q_Q(WOutputViewport);

    bufferRenderer = new WBufferRenderer(q);
    QQuickItemPrivate::get(bufferRenderer)->anchors()->setFill(q);
    QObject::connect(bufferRenderer, &WBufferRenderer::cacheBufferChanged,
                     q, &WOutputViewport::cacheBufferChanged);
    QObject::connect(bufferRenderer, &WBufferRenderer::afterRendering,
                     q, [this] {
        forceRender = false;
    });
}

void WOutputViewportPrivate::initForOutput()
{
    W_Q(WOutputViewport);

    updateRenderBufferSource();
    bufferRenderer->setOutput(output);
    if (window) {
        outputWindow()->attach(q);
        attached = true;
    }

    output->safeConnect(&WOutput::modeChanged, q, [this] {
        updateImplicitSize();
    });

    updateImplicitSize();
}

void WOutputViewportPrivate::update()
{
    if (Q_UNLIKELY(!componentComplete || !output))
        return;

    auto window = outputWindow();
    if (Q_LIKELY(window))
        window->update(q_func());
}

qreal WOutputViewportPrivate::calculateImplicitWidth() const
{
    return output->size().width() / devicePixelRatio;
}

qreal WOutputViewportPrivate::calculateImplicitHeight() const
{
    return output->size().height() / devicePixelRatio;
}

void WOutputViewportPrivate::updateImplicitSize()
{
    q_func()->setImplicitSize(calculateImplicitWidth(),
                              calculateImplicitHeight());
}

void WOutputViewportPrivate::updateRenderBufferSource()
{
    W_Q(WOutputViewport);

    QList<QQuickItem*> sources;

    if (input) {
        sources.append(input);
    } else {
        // the "nullptr" is on behalf of the window's contentItem
        sources.append(nullptr);
    }

    if (extraRenderSource)
        sources.append(extraRenderSource);

    forceRender = true;
    bufferRenderer->setSourceList(sources, true);
}

void WOutputViewportPrivate::setExtraRenderSource(QQuickItem *source)
{
    if (extraRenderSource == source)
        return;
    extraRenderSource = source;
    updateRenderBufferSource();
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
        d->attached = false;
    }
}

bool WOutputViewport::isTextureProvider() const
{
    W_DC(WOutputViewport);
    if (QQuickItem::isTextureProvider())
        return true;

    return d->bufferRenderer->isTextureProvider();
}

QSGTextureProvider *WOutputViewport::textureProvider() const
{
    W_DC(WOutputViewport);
    if (auto tp = QQuickItem::textureProvider())
        return tp;

    return d->bufferRenderer->textureProvider();
}

WSGTextureProvider *WOutputViewport::wTextureProvider() const
{
    W_DC(WOutputViewport);
    return qobject_cast<WSGTextureProvider*>(d->bufferRenderer->textureProvider());
}

WOutputRenderWindow *WOutputViewport::outputRenderWindow() const
{
    return qobject_cast<WOutputRenderWindow*>(window());
}

QQuickItem *WOutputViewport::input() const
{
    W_DC(WOutputViewport);
    return d->input;
}

void WOutputViewport::setInput(QQuickItem *item)
{
    W_D(WOutputViewport);
    if (d->input == item)
        return;

    d->input = item;

    if (d->output) {
        d->updateRenderBufferSource();
    }

    Q_EMIT inputChanged();
}

void WOutputViewport::resetInput()
{
    setInput(nullptr);
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
    Q_EMIT outputChanged();
}

QQuickTransform *WOutputViewport::viewportTransform() const
{
    W_DC(WOutputViewport);
    return d->viewportTransform;
}

void WOutputViewport::setViewportTransform(QQuickTransform *newViewportTransform)
{
    W_D(WOutputViewport);
    if (d->viewportTransform == newViewportTransform)
        return;
    d->viewportTransform = newViewportTransform;
    d->update();
    Q_EMIT viewportTransformChanged();
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

bool WOutputViewport::cacheBuffer() const
{
    W_DC(WOutputViewport);
    return d->bufferRenderer->cacheBuffer();
}

void WOutputViewport::setCacheBuffer(bool newCacheBuffer)
{
    W_D(WOutputViewport);
    d->bufferRenderer->setCacheBuffer(newCacheBuffer);
}

bool WOutputViewport::preserveColorContents() const
{
    W_DC(WOutputViewport);
    return d->preserveColorContents;
}

void WOutputViewport::setPreserveColorContents(bool newPreserveColorContents)
{
    W_D(WOutputViewport);
    if (d->preserveColorContents == newPreserveColorContents)
        return;
    d->preserveColorContents = newPreserveColorContents;
    Q_EMIT preserveColorContentsChanged();
}

bool WOutputViewport::live() const
{
    W_DC(WOutputViewport);
    return d->live;
}

void WOutputViewport::setLive(bool newLive)
{
    W_D(WOutputViewport);
    if (d->live == newLive)
        return;

    d->live = newLive;
    Q_EMIT liveChanged();
}

QRectF WOutputViewport::effectiveSourceRect() const
{
    const auto s = sourceRect();
    if (s.isValid())
        return s;
    if (ignoreViewport())
        return {};
    return QRectF(QPointF(0, 0), size());
}

QRectF WOutputViewport::sourceRect() const
{
    W_DC(WOutputViewport);
    return d->sourceRect;
}

void WOutputViewport::setSourceRect(const QRectF &newSourceRect)
{
    W_D(WOutputViewport);
    if (d->sourceRect == newSourceRect)
        return;
    d->sourceRect = newSourceRect;
    d->update();
    Q_EMIT sourceRectChanged();
}

void WOutputViewport::resetSourceRect()
{
    setSourceRect({});
}

bool WOutputViewport::disableHardwareLayers() const
{
    W_DC(WOutputViewport);
    return d->disableHardwareLayers;
}

void WOutputViewport::setDisableHardwareLayers(bool newDisableHardwareLayers)
{
    W_D(WOutputViewport);
    if (d->disableHardwareLayers == newDisableHardwareLayers)
        return;
    d->disableHardwareLayers = newDisableHardwareLayers;
    d->update();
    Q_EMIT disableHardwareLayersChanged();
    Q_EMIT hardwareLayersChanged();
}

bool WOutputViewport::ignoreSoftwareLayers() const
{
    W_DC(WOutputViewport);
    return d->ignoreSoftwareLayers;
}

void WOutputViewport::setIgnoreSoftwareLayers(bool newIgnoreSoftwareLayers)
{
    W_D(WOutputViewport);
    if (d->ignoreSoftwareLayers == newIgnoreSoftwareLayers)
        return;
    d->ignoreSoftwareLayers = newIgnoreSoftwareLayers;
    d->update();
    Q_EMIT ignoreSoftwareLayersChanged();
}

QRectF WOutputViewport::targetRect() const
{
    W_DC(WOutputViewport);
    return d->targetRect;
}

void WOutputViewport::setTargetRect(const QRectF &newTargetRect)
{
    W_D(WOutputViewport);
    if (d->targetRect == newTargetRect)
        return;
    d->targetRect = newTargetRect;
    d->update();
    Q_EMIT targetRectChanged();
}

void WOutputViewport::resetTargetRect()
{
    setTargetRect({});
}

QTransform WOutputViewport::sourceRectToTargetRectTransfrom() const
{
    return WBufferRenderer::inputMapToOutput(effectiveSourceRect(),
                                             targetRect(),
                                             output()->size(),
                                             devicePixelRatio());
}

QMatrix4x4 WOutputViewport::renderMatrix() const
{
    QMatrix4x4 renderMatrix;

    if (auto customTransform = viewportTransform()) {
        customTransform->applyTo(&renderMatrix);
    } else if (parentItem() && !ignoreViewport() && input() != this) {
        auto d = QQuickItemPrivate::get(const_cast<WOutputViewport*>(this));
        auto viewportMatrix = d->itemNode()->matrix().inverted();
        if (auto inputItem = input()) {
            QMatrix4x4 matrix = QQuickItemPrivate::get(parentItem())->itemToWindowTransform();
            matrix *= QQuickItemPrivate::get(inputItem)->windowToItemTransform();
            renderMatrix = viewportMatrix * matrix.inverted();
        } else { // the input item is window's contentItem
            auto pd = QQuickItemPrivate::get(parentItem());
            QMatrix4x4 parentMatrix = pd->itemToWindowTransform().inverted();
            renderMatrix = viewportMatrix * parentMatrix;
        }
    }

    return renderMatrix;
}

QMatrix4x4 WOutputViewport::mapToViewport(QQuickItem *item) const
{
    if (item == input())
        return renderMatrix();
    auto sd = QQuickItemPrivate::get(item);
    auto matrix = sd->itemToWindowTransform();

    if (auto i = input()) {
        matrix *= QQuickItemPrivate::get(i)->windowToItemTransform();
        return renderMatrix() * matrix;
    } else { // the input item is window's contentItem
        matrix *= QQuickItemPrivate::get(this)->windowToItemTransform();
        return matrix;
    }
}

QRectF WOutputViewport::mapToOutput(QQuickItem *source, const QRectF &geometry) const
{
    return (mapToViewport(source) * sourceRectToTargetRectTransfrom()).mapRect(geometry);
}

QPointF WOutputViewport::mapToOutput(QQuickItem *source, const QPointF &position) const
{
    return (mapToViewport(source) * sourceRectToTargetRectTransfrom()).map(position);
}

bool WOutputViewport::ignoreViewport() const
{
    W_DC(WOutputViewport);
    return d->ignoreViewport;
}

void WOutputViewport::setIgnoreViewport(bool newIgnoreViewport)
{
    W_D(WOutputViewport);
    if (d->ignoreViewport == newIgnoreViewport)
        return;
    d->ignoreViewport = newIgnoreViewport;
    Q_EMIT ignoreViewportChanged();
}

QList<WOutputLayer *> WOutputViewport::layers() const
{
    W_DC(WOutputViewport);

    if (!d->attached)
        return {};

    Q_ASSERT(d->window);
    return d->outputWindow()->layers(this);
}

QList<WOutputLayer *> WOutputViewport::hardwareLayers() const
{
    W_DC(WOutputViewport);

    if (d->disableHardwareLayers || !d->attached)
        return {};

    Q_ASSERT(d->window);
    return d->outputWindow()->hardwareLayers(this);
}

QList<WOutputViewport *> WOutputViewport::depends() const
{
    W_DC(WOutputViewport);
    return d->depends;
}

void WOutputViewport::setDepends(const QList<WOutputViewport *> &newDepends)
{
    W_D(WOutputViewport);
    if (d->depends == newDepends)
        return;
    d->depends = newDepends;
    Q_EMIT dependsChanged();
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

void WOutputViewport::render(bool doCommit)
{
    W_D(WOutputViewport);
    if (auto window = d->outputWindow())
        window->render(this, doCommit);
}

void WOutputViewport::componentComplete()
{
    W_D(WOutputViewport);

    if (d->output)
        d->initForOutput();

    QQuickItem::componentComplete();
}

void WOutputViewport::releaseResources()
{
    invalidate();
    QQuickItem::releaseResources();
}

void WOutputViewport::itemChange(ItemChange change, const ItemChangeData &data)
{
    QQuickItem::itemChange(change, data);

    if (change == ItemSceneChange && data.window) {
        if (!qobject_cast<WOutputRenderWindow*>(data.window)) {
            qFatal() << "OutputViewport must using in OutputRenderWindow.";
        }
        W_D(WOutputViewport);

        if (d->output && isComponentComplete()) {
            d->outputWindow()->attach(this);
            d->attached = true;
        }
    }
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_woutputviewport.cpp"
