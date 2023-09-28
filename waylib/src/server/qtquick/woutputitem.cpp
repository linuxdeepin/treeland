// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutputitem.h"
#include "woutputitem_p.h"
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

WOutputItemAttached::WOutputItemAttached(QObject *parent)
    : QObject(parent)
{

}

WOutputItem *WOutputItemAttached::item() const
{
    return m_positioner;
}

void WOutputItemAttached::setItem(WOutputItem *positioner)
{
    if (m_positioner == positioner)
        return;
    m_positioner = positioner;
    Q_EMIT itemChanged();
}

#define DATA_OF_WOUPTUT "_WOutputItem"

class WOutputItemPrivate : public WObjectPrivate
{
public:
    WOutputItemPrivate(WOutputItem *qq)
        : WObjectPrivate(qq)
    {

    }
    ~WOutputItemPrivate() {
        if (layout)
            layout->remove(q_func());
        if (output)
            output->setProperty(DATA_OF_WOUPTUT, QVariant());
    }

    void initForOutput() {
        W_Q(WOutputItem);

        Q_ASSERT(output);
        if (layout)
            layout->add(q);

        QObject::connect(output, &WOutput::transformedSizeChanged, q, [this] {
            updateImplicitSize();
        });

        updateImplicitSize();
    }

    void updateImplicitSize() {
        W_Q(WOutputItem);

        q->privateImplicitWidthChanged();
        q->privateImplicitHeightChanged();

        q->resetWidth();
        q->resetHeight();
    }

    W_DECLARE_PUBLIC(WOutputItem)
    QPointer<WOutput> output;
    QPointer<WQuickOutputLayout> layout;
    qreal devicePixelRatio = 1.0;
};

WOutputItem::WOutputItem(QQuickItem *parent)
    : WQuickObserver(parent)
    , WObject(*new WOutputItemPrivate(this))
{

}

WOutputItem::~WOutputItem()
{

}

WOutputItemAttached *WOutputItem::qmlAttachedProperties(QObject *target)
{
    auto output = qobject_cast<WOutput*>(target);
    if (!output)
        return nullptr;
    auto attached = new WOutputItemAttached(output);
    attached->setItem(qvariant_cast<WOutputItem*>(output->property(DATA_OF_WOUPTUT)));

    return attached;
}

WOutput *WOutputItem::output() const
{
    W_D(const WOutputItem);
    return d->output.get();
}

inline static WOutputItemAttached *getAttached(WOutput *output)
{
    return output->findChild<WOutputItemAttached*>(QString(), Qt::FindDirectChildrenOnly);
}

void WOutputItem::setOutput(WOutput *newOutput)
{
    W_D(WOutputItem);

    Q_ASSERT(!d->output || !newOutput);
    d->output = newOutput;

    if (newOutput) {
        if (auto attached = getAttached(newOutput)) {
            attached->setItem(this);
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

WQuickOutputLayout *WOutputItem::layout() const
{
    Q_D(const WOutputItem);
    return d->layout.get();
}

void WOutputItem::setLayout(WQuickOutputLayout *layout)
{
    Q_D(WOutputItem);

    if (d->layout == layout)
        return;

    if (d->layout)
        d->layout->remove(this);

    d->layout = layout;
    if (isComponentComplete() && d->layout && d->output)
        d->layout->add(this);

    Q_EMIT layoutChanged();
}

qreal WOutputItem::devicePixelRatio() const
{
    W_DC(WOutputItem);
    return d->devicePixelRatio;
}

void WOutputItem::setDevicePixelRatio(qreal newDevicePixelRatio)
{
    W_D(WOutputItem);

    if (qFuzzyCompare(d->devicePixelRatio, newDevicePixelRatio))
        return;
    d->devicePixelRatio = newDevicePixelRatio;

    if (d->output)
        d->updateImplicitSize();

    Q_EMIT devicePixelRatioChanged();
}

void WOutputItem::componentComplete()
{
    W_D(WOutputItem);

    if (d->output)
        d->initForOutput();

    WQuickObserver::componentComplete();
}

void WOutputItem::releaseResources()
{
    W_D(WOutputItem);
    WQuickObserver::releaseResources();
}

qreal WOutputItem::getImplicitWidth() const
{
    W_DC(WOutputItem);
    return d->output->transformedSize().width() / d->devicePixelRatio;
}

qreal WOutputItem::getImplicitHeight() const
{
    W_DC(WOutputItem);
    return d->output->transformedSize().height() / d->devicePixelRatio;
}

WAYLIB_SERVER_END_NAMESPACE
