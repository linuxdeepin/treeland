// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquickoutputlayout.h"
#include "private/woutputlayout_p.h"
#include "woutputviewport.h"
#include "woutputrenderwindow.h"
#include "woutput.h"
#include "woutputlayout.h"

#include <qwoutput.h>
#include <qwoutputlayout.h>

#include <QQuickWindow>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class WQuickOutputLayoutPrivate : public WOutputLayoutPrivate
{
public:
    WQuickOutputLayoutPrivate(WQuickOutputLayout *qq)
        : WOutputLayoutPrivate(qq)
    {

    }

    W_DECLARE_PUBLIC(WQuickOutputLayout)

    QList<WOutputViewport*> outputs;
};

WQuickOutputLayout::WQuickOutputLayout(QObject *parent)
    : WOutputLayout(*new WQuickOutputLayoutPrivate(this), parent)
{

}

QList<WOutputViewport*> WQuickOutputLayout::outputs() const
{
    W_DC(WQuickOutputLayout);
    return d->outputs;
}

void WQuickOutputLayout::add(WOutputViewport *output)
{
    W_D(WQuickOutputLayout);
    Q_ASSERT(!d->outputs.contains(output));
    d->outputs.append(output);
    add(output->output(), output->globalPosition().toPoint());

    auto updateOutput = [d, output, this] {
        Q_ASSERT(d->outputs.contains(output));
        move(output->output(), output->globalPosition().toPoint());
    };

    connect(output, &WOutputViewport::maybeGlobalPositionChanged, this, updateOutput, Qt::QueuedConnection);
    output->output()->setLayout(this);

    Q_EMIT outputsChanged();
}

void WQuickOutputLayout::remove(WOutputViewport *output)
{
    W_D(WQuickOutputLayout);
    Q_ASSERT(d->outputs.contains(output));
    d->outputs.removeOne(output);
    remove(output->output());

    disconnect(output->window(), nullptr, this, nullptr);
    disconnect(output, nullptr, this, nullptr);

    output->output()->setLayout(nullptr);

    Q_EMIT outputsChanged();
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_wquickoutputlayout.cpp"
