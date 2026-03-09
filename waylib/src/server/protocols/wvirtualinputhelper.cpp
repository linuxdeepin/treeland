// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wvirtualinputhelper.h"

#include "private/wglobal_p.h"
#include "private/wvirtualkeyboardv1_p.h"
#include "private/wvirtualpointerv1_p.h"
#include "wcursor.h"
#include "winputdevice.h"
#include "woutput.h"
#include "wseat.h"
#include "wserver.h"

#include <qwcursor.h>
#include <qwinputdevice.h>
#include <qwoutput.h>
#include <qwvirtualkeyboardv1.h>
#include <qwvirtualpointerv1.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WVirtualInputHelperPrivate : public WObjectPrivate
{
    W_DECLARE_PUBLIC(WVirtualInputHelper)

public:
    explicit WVirtualInputHelperPrivate(WServer *s, WSeat *st, WVirtualInputHelper *qq)
        : WObjectPrivate(qq)
        , server(s)
        , seat(st)
        , virtualKeyboardManagerV1(server->attach<WVirtualKeyboardManagerV1>())
        , virtualPointerManagerV1(server->attach<WVirtualPointerManagerV1>())
    {
        Q_ASSERT(server);
        Q_ASSERT(seat);
        Q_ASSERT(virtualKeyboardManagerV1);
        Q_ASSERT(virtualPointerManagerV1);
    }

    const QPointer<WServer> server;
    const QPointer<WSeat> seat;
    const QPointer<WVirtualKeyboardManagerV1> virtualKeyboardManagerV1;
    const QPointer<WVirtualPointerManagerV1> virtualPointerManagerV1;
    QList<WInputDevice *> virtualKeyboards;
    QList<WInputDevice *> virtualPointers;
};

WVirtualInputHelper::WVirtualInputHelper(WServer *server, WSeat *seat)
    : QObject(server)
    , WObject(*new WVirtualInputHelperPrivate(server, seat, this))
{
    W_D(WVirtualInputHelper);
    connect(d->virtualKeyboardManagerV1, &WVirtualKeyboardManagerV1::newVirtualKeyboard,
            this, &WVirtualInputHelper::handleNewVKV1);
    connect(d->virtualPointerManagerV1, &WVirtualPointerManagerV1::newVirtualPointer,
            this, &WVirtualInputHelper::handleNewVPV1);
}

WVirtualInputHelper::~WVirtualInputHelper()
{
    W_D(WVirtualInputHelper);
    if (d->virtualKeyboardManagerV1)
        d->virtualKeyboardManagerV1->disconnect(this);
    if (d->virtualPointerManagerV1)
        d->virtualPointerManagerV1->disconnect(this);
}

bool WVirtualInputHelper::shouldAcceptSeat(::wlr_seat *suggestedSeat) const
{
    W_DC(WVirtualInputHelper);
    return !suggestedSeat || (d->seat && d->seat->nativeHandle() == suggestedSeat);
}

WInputDevice *WVirtualInputHelper::ensureDevice(qw_input_device *handle) const
{
    if (!handle)
        return nullptr;

    if (auto *device = WInputDevice::fromHandle(handle))
        return device;

    return new WInputDevice(handle);
}

void WVirtualInputHelper::attachDevice(WInputDevice *device)
{
    W_D(WVirtualInputHelper);
    if (!device || !d->seat || device->seat() == d->seat)
        return;

    if (device->seat())
        return;

    d->seat->attachInputDevice(device);
}

void WVirtualInputHelper::detachDevice(WInputDevice *device)
{
    W_D(WVirtualInputHelper);
    if (!device || !d->seat || device->seat() == d->seat)
        return;

    if (device->seat())
        return;

    d->seat->detachInputDevice(device);
}

void WVirtualInputHelper::maybeMapToOutput(WInputDevice *device, ::wlr_output *output) const
{
    W_DC(WVirtualInputHelper);
    if (!device || !output || !d->seat || !d->seat->cursor())
        return;

    if (!WOutput::fromHandle(qw_output::from(output)))
        return;

    d->seat->cursor()->handle()->map_input_to_output(device->handle()->handle(), output);
}

void WVirtualInputHelper::handleNewVKV1(::wlr_virtual_keyboard_v1 *virtualKeyboard)
{
    W_D(WVirtualInputHelper);
    auto *device = ensureDevice(qw_input_device::from(&virtualKeyboard->keyboard.base));
    if (!device || device->seat() || d->virtualKeyboards.contains(device))
        return;

    d->virtualKeyboards.append(device);
    attachDevice(device);
    device->safeConnect(&qw_input_device::before_destroy, this, [this, d, device] {
        detachDevice(device);
        d->virtualKeyboards.removeOne(device);
        device->safeDeleteLater();
    });
}

void WVirtualInputHelper::handleNewVPV1(::wlr_virtual_pointer_v1_new_pointer_event *event)
{
    W_D(WVirtualInputHelper);
    if (!event || !shouldAcceptSeat(event->suggested_seat))
        return;

    auto *device = ensureDevice(qw_input_device::from(&event->new_pointer->pointer.base));
    if (!device || device->seat() || d->virtualPointers.contains(device))
        return;

    d->virtualPointers.append(device);
    attachDevice(device);
    maybeMapToOutput(device, event->suggested_output);
    device->safeConnect(&qw_input_device::before_destroy, this, [this, d, device] {
        detachDevice(device);
        d->virtualPointers.removeOne(device);
        device->safeDeleteLater();
    });
}

WAYLIB_SERVER_END_NAMESPACE
