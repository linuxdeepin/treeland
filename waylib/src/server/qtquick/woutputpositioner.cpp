// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutputpositioner.h"
#include "woutputpositioner_p.h"
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

WOutputPositionerAttached::WOutputPositionerAttached(QObject *parent)
    : QObject(parent)
{

}

WOutputPositioner *WOutputPositionerAttached::positioner() const
{
    return m_positioner;
}

void WOutputPositionerAttached::setPositioner(WOutputPositioner *positioner)
{
    if (m_positioner == positioner)
        return;
    m_positioner = positioner;
    Q_EMIT positionerChanged();
}

#define DATA_OF_WOUPTUT "_WOutputPositioner"

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
        if (output)
            output->setProperty(DATA_OF_WOUPTUT, QVariant());
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

WOutputPositionerAttached *WOutputPositioner::qmlAttachedProperties(QObject *target)
{
    auto output = qobject_cast<WOutput*>(target);
    if (!output)
        return nullptr;
    auto attached = new WOutputPositionerAttached(output);
    attached->setPositioner(qvariant_cast<WOutputPositioner*>(output->property(DATA_OF_WOUPTUT)));

    return attached;
}

WOutput *WOutputPositioner::output() const
{
    W_D(const WOutputPositioner);
    return d->output.get();
}

inline static WOutputPositionerAttached *getAttached(WOutput *output)
{
    return output->findChild<WOutputPositionerAttached*>(QString(), Qt::FindDirectChildrenOnly);
}

void WOutputPositioner::setOutput(WOutput *newOutput)
{
    W_D(WOutputPositioner);

    Q_ASSERT(!d->output || !newOutput);
    d->output = newOutput;

    if (newOutput) {
        if (auto attached = getAttached(newOutput)) {
            attached->setPositioner(this);
        } else {
            newOutput->setProperty(DATA_OF_WOUPTUT, QVariant::fromValue(this));
        }
    }

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
