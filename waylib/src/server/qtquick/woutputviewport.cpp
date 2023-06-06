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

        QObject::connect(output, &WOutput::effectiveSizeChanged, q, [this] {
            updateImplicitSize();
        });

        updateImplicitSize();
    }

    qreal getImplicitWidth() const override {
        return output->transformedSize().width() / output->scale();
    }
    qreal getImplicitHeight() const override {
        return output->transformedSize().height() / output->scale();
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

    W_DECLARE_PUBLIC(WOutputViewport)
    WOutput *output = nullptr;
    WQuickSeat *seat = nullptr;
};

WOutputViewport::WOutputViewport(QQuickItem *parent)
    : QQuickItem(*new WOutputViewportPrivate(), parent)
{

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

    QQuickItem::componentComplete();
}

void WOutputViewport::releaseResources()
{
    W_D(WOutputViewport);
    if (d->seat)
        d->seat->removeOutput(d_func()->output);
    QQuickItem::releaseResources();
}

WAYLIB_SERVER_END_NAMESPACE
