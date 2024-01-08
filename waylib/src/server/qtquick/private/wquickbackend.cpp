// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquickbackend_p.h"
#include "wbackend.h"
#include "woutput.h"

#include <qwbackend.h>
#include <qwoutput.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class Backend : public WBackend
{
public:
    Backend(WQuickBackend *qq)
        : qq(qq) {}

    void outputAdded(WOutput *output) override;
    void outputRemoved(WOutput *output) override;
    void inputAdded(WInputDevice *input) override;
    void inputRemoved(WInputDevice *input) override;

    WQuickBackend *qq;
};

class WQuickBackendPrivate : public WObjectPrivate, public WBackend
{
public:
    WQuickBackendPrivate(WQuickBackend *qq)
        : WObjectPrivate(qq)
    {
    }

    W_DECLARE_PUBLIC(WQuickBackend)

    Backend *backend = nullptr;
};

void Backend::outputAdded(WOutput *output)
{
    WBackend::outputAdded(output);
    Q_EMIT qq->outputAdded(output);
}

void Backend::outputRemoved(WOutput *output)
{
    WBackend::outputRemoved(output);
    Q_EMIT qq->outputRemoved(output);
}

void Backend::inputAdded(WInputDevice *input)
{
    WBackend::inputAdded(input);
    Q_EMIT qq->inputAdded(input);
}

void Backend::inputRemoved(WInputDevice *input)
{
    WBackend::inputRemoved(input);
    Q_EMIT qq->inputRemoved(input);
}

WQuickBackend::WQuickBackend(QObject *parent)
    : WQuickWaylandServerInterface(parent)
    , WObject(*new WQuickBackendPrivate(this), nullptr)
{

}

QWBackend *WQuickBackend::backend() const
{
    W_DC(WQuickBackend);
    return d->backend->nativeInterface<QWBackend>();
}

template<class T>
static bool hasBackend(QWBackend *handle)
{
    if (qobject_cast<T*>(handle))
        return true;
    if (auto multiBackend = qobject_cast<QWMultiBackend*>(handle)) {
        bool exists = false;
        multiBackend->forEachBackend([] (wlr_backend *backend, void *userData) {
            bool &exists = *reinterpret_cast<bool*>(userData);
            if (T::from(backend))
                exists = true;
        }, &exists);

        return exists;
    }

    return false;
}

bool WQuickBackend::hasDrm() const
{
    return hasBackend<QWDrmBackend>(backend());
}

bool WQuickBackend::hasX11() const
{
    return hasBackend<QWX11Backend>(backend());
}

bool WQuickBackend::hasWayland() const
{
    return hasBackend<QWWaylandBackend>(backend());
}

void WQuickBackend::create()
{
    WQuickWaylandServerInterface::create();

    W_D(WQuickBackend);
    Q_ASSERT(!d->backend);
    d->backend = server()->attach<Backend>(this);
}

void WQuickBackend::polish()
{
    W_D(WQuickBackend);

    for (auto output : d->backend->outputList()) {
        Q_EMIT outputAdded(output);
    }

    for (auto device : d->backend->inputDeviceList()) {
        Q_EMIT inputAdded(device);
    }

    if (!backend()->start()) {
        qFatal("Start wlr backend falied");
    }

    WQuickWaylandServerInterface::polish();
}

WAYLIB_SERVER_END_NAMESPACE
