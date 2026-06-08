// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "weventjunkman.h"
#include "wseat.h"
#include "winputdevice.h"

#include <QQuickWindow>

WAYLIB_SERVER_BEGIN_NAMESPACE

WEventJunkman::WEventJunkman(QQuickItem *parent)
    : QQuickItem(parent)
{
    setAcceptedMouseButtons(Qt::AllButtons);
}

bool WEventJunkman::event(QEvent *event)
{
    switch(event->type()) {
        using enum QEvent::Type;
    case MouseButtonPress: Q_FALLTHROUGH();
    case MouseButtonRelease: Q_FALLTHROUGH();
    case KeyPress: Q_FALLTHROUGH();
    case KeyRelease: {
        auto e = static_cast<QInputEvent*>(event);
        auto inputDevice = WInputDevice::from(e->device());
        if (Q_UNLIKELY(!inputDevice))
            return false;

        auto seat = inputDevice->seat();
        if (!seat)
            return false;
        seat->filterUnacceptedEvent(window(), e);
        return true;
    }
    default:
        break;
    }

    return QQuickItem::event(event);
}

WAYLIB_SERVER_END_NAMESPACE
