// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquickoutputlayout.h"
#include "private/woutputlayout_p.h"
#include "woutputitem.h"
#include "woutput.h"
#include "woutputlayout.h"

#include <qwoutput.h>
#include <qwoutputlayout.h>

#include <QQuickWindow>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WQuickOutputLayoutPrivate : public WOutputLayoutPrivate
{
public:
    WQuickOutputLayoutPrivate(WQuickOutputLayout *qq)
        : WOutputLayoutPrivate(qq)
    {

    }

    W_DECLARE_PUBLIC(WQuickOutputLayout)

    QList<WOutputItem*> outputs;
};

WQuickOutputLayout::WQuickOutputLayout(WServer *server)
    : WOutputLayout(*new WQuickOutputLayoutPrivate(this), server)
{

}

const QList<WOutputItem*> &WQuickOutputLayout::outputs() const
{
    W_DC(WQuickOutputLayout);
    return d->outputs;
}

void WQuickOutputLayout::add(WOutputItem *output)
{
    W_D(WQuickOutputLayout);
    Q_ASSERT(!d->outputs.contains(output));
    d->outputs.append(output);
    add(output->output(), output->globalPosition().toPoint());

    auto updateOutput = [d, this] {
        auto *output = qobject_cast<WOutputItem*>(sender());
        if (!output) // Maybe output has destroyed but event still in queue
            return;
        Q_ASSERT(d->outputs.contains(output));
        move(output->output(), output->globalPosition().toPoint());
        Q_EMIT maybeLayoutChanged();
    };

    connect(output, &WOutputItem::maybeGlobalPositionChanged, this, updateOutput, Qt::QueuedConnection);
    connect(output, &WOutputItem::transformChanged, this, &WQuickOutputLayout::maybeLayoutChanged);
    output->output()->setLayout(this);

    Q_EMIT outputsChanged();
    Q_EMIT maybeLayoutChanged();
}

void WQuickOutputLayout::remove(WOutputItem *output)
{
    W_D(WQuickOutputLayout);

    if (!d->outputs.removeOne(output))
        return;
    output->disconnect(this);

    if (auto o = output->output()) {
        remove(o);
    }

    Q_EMIT outputsChanged();
    Q_EMIT maybeLayoutChanged();
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_wquickoutputlayout.cpp"
