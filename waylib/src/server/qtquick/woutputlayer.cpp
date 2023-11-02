// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutputlayer.h"
#include "woutputrenderwindow.h"
#include "wrenderhelper.h"
#include "woutputviewport.h"

#include <QQuickItem>
#include <private/qquickitem_p.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutputLayerPrivate : public WObjectPrivate
{
public:
    WOutputLayerPrivate(WOutputLayer *qq)
        : WObjectPrivate(qq) {

    }

    void updateWindow();
    void doEnable(bool enable);
    void setRefItem(bool on);

    W_DECLARE_PUBLIC(WOutputLayer)

    WOutputRenderWindow *window = nullptr;
    bool enabled = false;
    bool actualEnabled = false;
    bool refItem = false;
    WOutputLayer::Flags flags = {0};
    int z = 0;
    QList<WOutputViewport*> outputs;
};

void WOutputLayerPrivate::updateWindow()
{
    W_Q(WOutputLayer);

    auto newWindow = qobject_cast<WOutputRenderWindow*>(q->parent()->window());
    if (newWindow == window)
        return;
    if (window)
        doEnable(false);
    window = newWindow;
    if (window && enabled)
        doEnable(true);
}

void WOutputLayerPrivate::doEnable(bool enable)
{
    W_Q(WOutputLayer);

    if (actualEnabled == enable)
        return;
    actualEnabled = enable;

    for (auto o : outputs) {
        if (enable) {
            if (o->window() && o->window() != window) {
                qWarning() << "OutputLayer: OutputViewport and OutputLayer's target item "
                              "must both be children of the same window.";
                continue;
            }

            window->attach(q, o);
        } else {
            window->detach(q, o);
        }
    }

    setRefItem(enable);
}

void WOutputLayerPrivate::setRefItem(bool on)
{
    if (refItem == on)
        return;
    refItem = on;

    W_Q(WOutputLayer);
    auto d = QQuickItemPrivate::get(q->parent());
    if (on)
        d->refFromEffectItem(true);
    else
        d->derefFromEffectItem(true);
}

WOutputLayer::WOutputLayer(QQuickItem *parent)
    : QObject(parent)
    , WObject(*new WOutputLayerPrivate(this))
{
    Q_ASSERT(parent);
    connect(parent, &QQuickItem::windowChanged, this, [this] {
        d_func()->updateWindow();
    });
    d_func()->updateWindow();
}

WOutputLayer *WOutputLayer::qmlAttachedProperties(QObject *target)
{
    if (!target->isQuickItemType())
        return nullptr;
    return new WOutputLayer(qobject_cast<QQuickItem*>(target));
}

QQuickItem *WOutputLayer::parent() const
{
    Q_ASSERT(QObject::parent()->isQuickItemType());
    return qobject_cast<QQuickItem*>(QObject::parent());
}

bool WOutputLayer::enabled() const
{
    W_DC(WOutputLayer);
    return d->enabled;
}

void WOutputLayer::setEnabled(bool newEnabled)
{
    W_D(WOutputLayer);
    if (d->enabled == newEnabled)
        return;
    d->enabled = newEnabled;

    if (d->window) {
        d->doEnable(newEnabled);
    }

    Q_EMIT enabledChanged();
}

WOutputLayer::Flags WOutputLayer::flags() const
{
    W_DC(WOutputLayer);
    return d->flags;
}

void WOutputLayer::setFlags(const Flags &newFlags)
{
    W_D(WOutputLayer);
    if (d->flags == newFlags)
        return;
    d->flags = newFlags;
    Q_EMIT flagsChanged();
}

QList<WOutputViewport *> WOutputLayer::outputs() const
{
    W_DC(WOutputLayer);
    return d->outputs;
}

void WOutputLayer::setOutputs(const QList<WOutputViewport*> &newOutputList)
{
    W_D(WOutputLayer);
    if (d->outputs == newOutputList)
        return;

    WOutput *output = nullptr;
    for (auto viewport : newOutputList) {
        if (d->window && viewport->window() != d->window) {
            qmlWarning(this) << "OutputViewport and OutputLayer's target item "
                                "must both be children of the same window.";
            return;
        }

        if (output && viewport->output() != output) {
            qmlWarning(this) << "OutputViewport and OutputLayer's target item "
                                "must both in the same WaylandOutput.";
            return;
        }

        output = viewport->output();
    }

    if (d->enabled && d->window)
        d->doEnable(false);

    d->outputs = newOutputList;

    if (d->enabled && d->window)
        d->doEnable(true);

    Q_EMIT outputsChanged();
}

int WOutputLayer::z() const
{
    W_DC(WOutputLayer);
    return d->z;
}

void WOutputLayer::setZ(int newZ)
{
    W_D(WOutputLayer);

    if (d->z == newZ)
        return;
    d->z = newZ;
    Q_EMIT zChanged();
}

void WOutputLayer::setAccepted(bool accepted)
{
    W_D(WOutputLayer);
    d->setRefItem(accepted);
}

bool WOutputLayer::isAccepted() const
{
    W_DC(WOutputLayer);
    return d->refItem;
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_woutputlayer.cpp"
