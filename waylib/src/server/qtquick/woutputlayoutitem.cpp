// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutputlayoutitem.h"
#include "woutput.h"
#include "wquickoutputlayout.h"
#include "private/wglobal_p.h"

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WOutputLayoutItemPrivate : public WObjectPrivate
{
public:
    WOutputLayoutItemPrivate(WOutputLayoutItem *qq)
        : WObjectPrivate(qq)
    {

    }

    void updateOutputs() {
        if (!layout)
            return;

        W_Q(WOutputLayoutItem);
        auto oldOutputs = outputs;
        outputs.clear();

        for (auto output: layout->getIntersectedOutputs(QRectF(q->globalPosition(), q->size()).toRect())) {
            outputs.append(output);
        };

        bool changed = false;

        for (int i = 0; i < outputs.count(); i++) {
            if (oldOutputs.removeOne(outputs.at(i)))
                continue;
            Q_EMIT q->enterOutput(outputs.at(i));
            changed = true;
        }

        oldOutputs.removeAll(nullptr);
        for (auto o : std::as_const(oldOutputs)) {
            Q_EMIT q->leaveOutput(o);
            changed = true;
        }

        if (changed)
            Q_EMIT q->outputsChanged();
    }

    W_DECLARE_PUBLIC(WOutputLayoutItem)
    QList<QPointer<WOutput>> outputs;
    WQuickOutputLayout *layout = nullptr;
};

WOutputLayoutItem::WOutputLayoutItem(QQuickItem *parent)
    : WQuickObserver(parent)
    , WObject(*new WOutputLayoutItemPrivate(this))
{
    connect(this, SIGNAL(transformChanged(QQuickItem*)), this, SLOT(updateOutputs()));
    connect(this, SIGNAL(maybeGlobalPositionChanged()), this, SLOT(updateOutputs()));
}

WQuickOutputLayout *WOutputLayoutItem::layout() const
{
    W_DC(WOutputLayoutItem);
    return d->layout;
}

void WOutputLayoutItem::setLayout(WQuickOutputLayout *newLayout)
{
    W_D(WOutputLayoutItem);
    if (d->layout == newLayout)
        return;

    if (d->layout)
        d->layout->safeDisconnect(this);

    d->layout = newLayout;

    if (d->layout)
        connect(d->layout, SIGNAL(maybeLayoutChanged()), this, SLOT(updateOutputs()));

    if (isComponentComplete())
        d->updateOutputs();

    Q_EMIT layoutChanged();
}

void WOutputLayoutItem::resetOutput()
{
    setLayout(nullptr);
}

QList<WOutput*> WOutputLayoutItem::outputs() const
{
    W_DC(WOutputLayoutItem);
    QList<WOutput*> outputs;
    for (const auto &output : std::as_const(d->outputs)) {
        outputs.append(output);
    }
    return outputs;
}

void WOutputLayoutItem::componentComplete()
{
    WQuickObserver::componentComplete();

    d_func()->updateOutputs();
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_woutputlayoutitem.cpp"
