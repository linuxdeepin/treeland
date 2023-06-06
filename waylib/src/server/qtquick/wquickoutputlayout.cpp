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

    void onOutputLayoutChanged();

    W_DECLARE_PUBLIC(WQuickOutputLayout)

    QList<WOutputViewport*> outputs;
};

void WQuickOutputLayoutPrivate::onOutputLayoutChanged()
{
    auto renderWindow = qobject_cast<WOutputRenderWindow*>(q_func()->QObject::sender());

    for (auto o : outputs) {
        if (o->window() == renderWindow)
            q_func()->move(o->output(), o->mapToGlobal(QPointF(0, 0)).toPoint());
    }
}

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
    add(output->output(), output->mapToGlobal(QPointF(0, 0)).toPoint());

    auto updateOutput = [d, output, this] {
        Q_ASSERT(d->outputs.contains(output));
        move(output->output(), output->mapToGlobal(QPointF(0, 0)).toPoint());
    };

    connect(output->window(), &QQuickWindow::xChanged, this, updateOutput);
    connect(output->window(), &QQuickWindow::yChanged, this, updateOutput);

    auto renderWindow = qobject_cast<WOutputRenderWindow*>(output->window());
    if (renderWindow) {
        connect(renderWindow, SIGNAL(outputLayoutChanged()),
                this, SLOT(onOutputLayoutChanged()), Qt::UniqueConnection);
    } else {
        connect(output, &WOutputViewport::xChanged, this, updateOutput, Qt::QueuedConnection);
        connect(output, &WOutputViewport::yChanged, this, updateOutput, Qt::QueuedConnection);
    }

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
