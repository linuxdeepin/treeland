// Copyright (C) 2023 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquickvirtualkeyboardv1_p.h"
#include "winputdevice.h"

#include <qwvirtualkeyboardv1.h>

#include <QLoggingCategory>

extern "C" {
#include <wlr/types/wlr_virtual_keyboard_v1.h>
}

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE
Q_DECLARE_LOGGING_CATEGORY(qLcInputMethod)
class WQuickVirtualKeyboardV1Private : public WObjectPrivate
{
    W_DECLARE_PUBLIC(WQuickVirtualKeyboardV1)
public:
    WQuickVirtualKeyboardV1Private(QWVirtualKeyboardV1 *h, WQuickVirtualKeyboardV1 *qq)
        : WObjectPrivate(qq)
        , handle(h)
        , keyboard(WInputDevice::fromHandle(QWKeyboard::from(&h->handle()->keyboard)))
        , ownKeyboard(false)
    {
        if (!keyboard) {
            keyboard = new WInputDevice(QWKeyboard::from(&h->handle()->keyboard));
            ownKeyboard = true;
        }
    }

    ~WQuickVirtualKeyboardV1Private()
    {
        if (ownKeyboard)
            delete keyboard;
    }

    QWVirtualKeyboardV1 *const handle;
    WInputDevice *keyboard;
    bool ownKeyboard;
};

class WQuickVirtualKeyboardManagerV1Private : public WObjectPrivate
{
    W_DECLARE_PUBLIC(WQuickVirtualKeyboardManagerV1)
public:
    explicit WQuickVirtualKeyboardManagerV1Private(WQuickVirtualKeyboardManagerV1 *qq)
        : WObjectPrivate(qq)
        , manager(nullptr)
    { }

    QWVirtualKeyboardManagerV1 *manager;
};

WQuickVirtualKeyboardManagerV1::WQuickVirtualKeyboardManagerV1(QObject *parent)
    : WQuickWaylandServerInterface(parent)
    , WObject(*new WQuickVirtualKeyboardManagerV1Private(this))
{}

void WQuickVirtualKeyboardManagerV1::create()
{
    W_D(WQuickVirtualKeyboardManagerV1);
    WQuickWaylandServerInterface::create();
    d->manager = QWVirtualKeyboardManagerV1::create(server()->handle());
    Q_ASSERT(d->manager);
    connect(d->manager, &QWVirtualKeyboardManagerV1::newVirtualKeyboard, this, &WQuickVirtualKeyboardManagerV1::newVirtualKeyboard);
}

QWVirtualKeyboardV1 *WQuickVirtualKeyboardV1::handle() const
{
    W_DC(WQuickVirtualKeyboardV1);
    return d->handle;
}

WInputDevice *WQuickVirtualKeyboardV1::keyboard() const
{
    W_DC(WQuickVirtualKeyboardV1);
    return d->keyboard;
}

WQuickVirtualKeyboardV1::WQuickVirtualKeyboardV1(QWVirtualKeyboardV1 *handle, QObject *parent)
    : QObject(parent)
    , WObject(*new WQuickVirtualKeyboardV1Private(handle, this))
{ }

WAYLIB_SERVER_END_NAMESPACE
