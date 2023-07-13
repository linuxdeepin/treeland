// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutputpositioner.h"
#include "woutputrenderwindow.h"
#include "woutput.h"
#include "woutputlayout.h"
#include "wquickseat_p.h"
#include "wquickoutputlayout.h"
#include "wseat.h"

#include <qwoutput.h>
#include <qwoutputlayout.h>

#include <private/qquickitem_p.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutputPositionerPrivate : public WObjectPrivate
{
public:
    WOutputPositionerPrivate(WOutputPositioner *qq)
        : WObjectPrivate(qq)
    {

    }
    ~WOutputPositionerPrivate() {
        if (layout)
            layout->remove(q_func());
    }

    void initForOutput() {
        W_Q(WOutputPositioner);

        Q_ASSERT(output);
        if (layout)
            layout->add(q);

        QObject::connect(output, &WOutput::transformedSizeChanged, q, [this] {
            updateImplicitSize();
        });

        updateImplicitSize();
    }

    void updateImplicitSize() {
        W_Q(WOutputPositioner);

        q->privateImplicitWidthChanged();
        q->privateImplicitHeightChanged();

        q->resetWidth();
        q->resetHeight();
    }

    W_DECLARE_PUBLIC(WOutputPositioner)
    QPointer<WOutput> output;
    QPointer<WQuickOutputLayout> layout;
    qreal devicePixelRatio = 1.0;
};

WOutputPositioner::WOutputPositioner(QQuickItem *parent)
    : WQuickObserver(parent)
    , WObject(*new WOutputPositionerPrivate(this))
{

}

WOutputPositioner::~WOutputPositioner()
{

}

WOutput *WOutputPositioner::output() const
{
    W_D(const WOutputPositioner);
    return d->output.get();
}

void WOutputPositioner::setOutput(WOutput *newOutput)
{
    W_D(WOutputPositioner);

    Q_ASSERT(!d->output || !newOutput);
    d->output = newOutput;

    if (isComponentComplete()) {
        if (newOutput) {
            d->initForOutput();
        }
    }
}

WQuickOutputLayout *WOutputPositioner::layout() const
{
    Q_D(const WOutputPositioner);
    return d->layout.get();
}

void WOutputPositioner::setLayout(WQuickOutputLayout *layout)
{
    Q_D(WOutputPositioner);

    if (d->layout == layout)
        return;

    if (d->layout)
        d->layout->remove(this);

    d->layout = layout;
    if (isComponentComplete() && d->layout && d->output)
        d->layout->add(this);

    Q_EMIT layoutChanged();
}

qreal WOutputPositioner::devicePixelRatio() const
{
    W_DC(WOutputPositioner);
    return d->devicePixelRatio;
}

void WOutputPositioner::setDevicePixelRatio(qreal newDevicePixelRatio)
{
    W_D(WOutputPositioner);

    if (qFuzzyCompare(d->devicePixelRatio, newDevicePixelRatio))
        return;
    d->devicePixelRatio = newDevicePixelRatio;

    if (d->output)
        d->updateImplicitSize();

    Q_EMIT devicePixelRatioChanged();
}

void WOutputPositioner::componentComplete()
{
    W_D(WOutputPositioner);

    if (d->output)
        d->initForOutput();

    WQuickObserver::componentComplete();
}

void WOutputPositioner::releaseResources()
{
    W_D(WOutputPositioner);
    WQuickObserver::releaseResources();
}

qreal WOutputPositioner::getImplicitWidth() const
{
    W_DC(WOutputPositioner);
    return d->output->transformedSize().width() / d->devicePixelRatio;
}

qreal WOutputPositioner::getImplicitHeight() const
{
    W_DC(WOutputPositioner);
    return d->output->transformedSize().height() / d->devicePixelRatio;
}

WAYLIB_SERVER_END_NAMESPACE
