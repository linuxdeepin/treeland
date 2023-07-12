// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutputviewport.h"
#include "woutputrenderwindow.h"
#include "woutput.h"
#include "wquickseat_p.h"
#include "wseat.h"

#include <qwoutput.h>

#include <private/qquickitem_p.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutputViewportPrivate : public QQuickItemPrivate
{
public:
    WOutputViewportPrivate()
    {

    }
    ~WOutputViewportPrivate() {

    }

    inline WOutputRenderWindow *outputWindow() const {
        auto ow = qobject_cast<WOutputRenderWindow*>(window);
        Q_ASSERT(ow);
        return ow;
    }

    void initForOutput() {
        W_Q(WOutputViewport);

        auto qwoutput = output->nativeInterface<QWOutput>();
        auto mode = qwoutput->preferredMode();
        if (mode)
            qwoutput->setMode(mode);
        qwoutput->enable(q->isVisible());
        qwoutput->commit();

        outputWindow()->attachOutput(q);

        QObject::connect(output, &WOutput::transformedSizeChanged, q, [this] {
            updateImplicitSize();
        });

        updateImplicitSize();
    }

    qreal getImplicitWidth() const override {
        return output->transformedSize().width() / devicePixelRatio;
    }
    qreal getImplicitHeight() const override {
        return output->transformedSize().height() / devicePixelRatio;
    }

    void updateImplicitSize() {
        implicitWidthChanged();
        implicitHeightChanged();

        W_Q(WOutputViewport);
        q->resetWidth();
        q->resetHeight();
    }

    void setVisible(bool visible) override {
        QQuickItemPrivate::setVisible(visible);
        if (output)
            output->nativeInterface<QWOutput>()->enable(visible);
    }

    bool transformChanged(QQuickItem *transformedItem) override {
        Q_EMIT q_func()->maybeGlobalPositionChanged();
        return QQuickItemPrivate::transformChanged(transformedItem);
    }

    W_DECLARE_PUBLIC(WOutputViewport)
    WOutput *output = nullptr;
    WQuickSeat *seat = nullptr;
    qreal devicePixelRatio = 1.0;
    QMetaObject::Connection windowXChangeConnection;
    QMetaObject::Connection windowYChangeConnection;
};

WOutputViewport::WOutputViewport(QQuickItem *parent)
    : QQuickItem(*new WOutputViewportPrivate(), parent)
{
    setFlag(ItemObservesViewport);
}

WOutputViewport::~WOutputViewport()
{
    W_D(WOutputViewport);
    if (d->componentComplete && d->output && d->window)
        d->outputWindow()->detachOutput(this);
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

    if (d->componentComplete) {
        if (newOutput) {
            d->initForOutput();
            if (d->seat)
                d->seat->addOutput(d->output);
        } else {
            if (d->seat)
                d->seat->removeOutput(d->output);
        }
    }

    d->output = newOutput;
}

WQuickSeat *WOutputViewport::seat() const
{
    W_D(const WOutputViewport);
    return d->seat;
}

// Bind this seat to output
void WOutputViewport::setSeat(WQuickSeat *newSeat)
{
    W_D(WOutputViewport);

    if (d->seat == newSeat)
        return;
    d->seat = newSeat;

    if (d->componentComplete && d->output)
        d->seat->addOutput(d->output);

    Q_EMIT seatChanged();
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

const QPointF WOutputViewport::globalPosition() const
{
    if (!parentItem())
        return position();
    return parentItem()->mapToGlobal(position());
}

void WOutputViewport::classBegin()
{
    W_D(WOutputViewport);

    QQuickItem::classBegin();
}

void WOutputViewport::componentComplete()
{
    W_D(WOutputViewport);

    if (d->output) {
        d->initForOutput();
        if (d->seat)
            d->seat->addOutput(d->output);
    }

    if (d->window) {
        d->windowXChangeConnection = connect(d->window, &QQuickWindow::xChanged,
                                             this, &WOutputViewport::maybeGlobalPositionChanged);
        d->windowYChangeConnection = connect(d->window, &QQuickWindow::yChanged,
                                             this, &WOutputViewport::maybeGlobalPositionChanged);
    }

    QQuickItem::componentComplete();
}

void WOutputViewport::releaseResources()
{
    W_D(WOutputViewport);
    if (d->seat)
        d->seat->removeOutput(d_func()->output);
    QQuickItem::releaseResources();
}

void WOutputViewport::itemChange(ItemChange change, const ItemChangeData &data)
{
    if (change == ItemChange::ItemParentHasChanged) {
        Q_EMIT maybeGlobalPositionChanged();
    } else if (change == ItemChange::ItemSceneChange) {
        Q_D(WOutputViewport);

        if (d->windowXChangeConnection)
            disconnect(d->windowXChangeConnection);
        if (d->windowYChangeConnection)
            disconnect(d->windowYChangeConnection);

        if (data.window) {
            d->windowXChangeConnection = connect(data.window, &QQuickWindow::xChanged,
                                                 this, &WOutputViewport::maybeGlobalPositionChanged);
            d->windowYChangeConnection = connect(data.window, &QQuickWindow::yChanged,
                                                 this, &WOutputViewport::maybeGlobalPositionChanged);
        }

        Q_EMIT maybeGlobalPositionChanged();
    }

    QQuickItem::itemChange(change, data);
}

void WOutputViewport::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    if (newGeometry.topLeft() != oldGeometry.topLeft())
        Q_EMIT maybeGlobalPositionChanged();

    QQuickItem::geometryChange(newGeometry, oldGeometry);
}

WAYLIB_SERVER_END_NAMESPACE
