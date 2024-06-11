// Copyright (C) 2023 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquickinputmethodv2_p.h"
#include "wseat.h"
#include "winputdevice.h"
#include "wsurface.h"
#include "wxdgsurface.h"
#include "private/wglobal_p.h"
#include "private/wtoplevelsurface_p.h"

#include <qwcompositor.h>
#include <qwinputmethodv2.h>
#include <qwseat.h>
#include <qwkeyboard.h>
#include <qwvirtualkeyboardv1.h>

#include <QKeySequence>
#include <QLoggingCategory>
#include <QRect>

extern "C" {
#define delete delete_c
#include <wlr/types/wlr_input_method_v2.h>
#undef delete
#include <wlr/types/wlr_keyboard.h>
#define static
#include <wlr/types/wlr_compositor.h>
#undef static
#include <wlr/types/wlr_virtual_keyboard_v1.h>
}

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE
Q_DECLARE_LOGGING_CATEGORY(qLcInputMethod)
class WQuickInputMethodManagerV2Private : public WObjectPrivate
{
public:
    explicit WQuickInputMethodManagerV2Private(WQuickInputMethodManagerV2 *qq)
        : WObjectPrivate(qq)
        , manager(nullptr)
    {}
    W_DECLARE_PUBLIC(WQuickInputMethodManagerV2)

    QWInputMethodManagerV2 *manager;
    QList<WQuickInputMethodV2 *> inputMethods;
};

WQuickInputMethodManagerV2::WQuickInputMethodManagerV2(QObject *parent) :
    WQuickWaylandServerInterface(parent),
    WObject(*new WQuickInputMethodManagerV2Private(this), nullptr)
{ }

WServerInterface *WQuickInputMethodManagerV2::create()
{
    W_D(WQuickInputMethodManagerV2);
    d->manager = QWInputMethodManagerV2::create(server()->handle());
    Q_ASSERT(d->manager);
    connect(d->manager, &QWInputMethodManagerV2::inputMethod, this, &WQuickInputMethodManagerV2::newInputMethod);

    return new WServerInterface(d->manager, d->manager->handle()->global);
}

class WQuickInputPopupSurfaceV2Private : public WWrapObjectPrivate
{
public:
    WQuickInputPopupSurfaceV2Private(QWInputPopupSurfaceV2 *h, WQuickInputPopupSurfaceV2 *qq)
        : WWrapObjectPrivate(qq)
        , popupSurface(WSurface::fromHandle(h->surface()))
    {
        initHandle(h);
        if (!popupSurface) {
            popupSurface = new WSurface(h->surface());
            popupSurface->safeConnect(&QWSurface::beforeDestroy,
                                     popupSurface, &WSurface::safeDeleteLater);
        }
    }

    WWRAP_HANDLE_FUNCTIONS(QWInputPopupSurfaceV2, wlr_input_popup_surface_v2)

    W_DECLARE_PUBLIC(WQuickInputPopupSurfaceV2)
    WSurface *popupSurface;
};

class WQuickInputMethodKeyboardGrabV2Private : public WObjectPrivate
{
public:
    WQuickInputMethodKeyboardGrabV2Private(QWInputMethodKeyboardGrabV2 *h, WQuickInputMethodKeyboardGrabV2 *qq)
        : WObjectPrivate(qq)
        , handle(h)
    { }
    W_DECLARE_PUBLIC(WQuickInputMethodKeyboardGrabV2)

    wl_client *waylandClient() const override
    {
        Q_ASSERT(handle);
        return wl_resource_get_client(handle->handle()->resource);
    }

    QWInputMethodKeyboardGrabV2 *const handle;
};

class WQuickInputMethodV2Private : public WWrapObjectPrivate
{
public:
    WQuickInputMethodV2Private(QWInputMethodV2 *h, WQuickInputMethodV2 *qq)
        : WWrapObjectPrivate(qq)
    {
        initHandle(h);
    }

    WWRAP_HANDLE_FUNCTIONS(QWInputMethodV2, wlr_input_method_v2)

    W_DECLARE_PUBLIC(WQuickInputMethodV2)
};

WQuickInputMethodV2::WQuickInputMethodV2(QWInputMethodV2 *h, QObject *parent) :
    WWrapObject(*new WQuickInputMethodV2Private(h, this), parent)
{
    W_D(WQuickInputMethodV2);
    connect(handle(), &QWInputMethodV2::commit, this, &WQuickInputMethodV2::committed);
    connect(handle(), &QWInputMethodV2::grabKeybord, this, &WQuickInputMethodV2::newKeyboardGrab);
    connect(handle(), &QWInputMethodV2::newPopupSurface, this, &WQuickInputMethodV2::newPopupSurface);
}

QWInputMethodV2 *WQuickInputMethodV2::handle() const
{
    return d_func()->handle();
}

WSeat *WQuickInputMethodV2::seat() const
{
    W_DC(WQuickInputMethodV2);
    return WSeat::fromHandle(QWSeat::from(d->nativeHandle()->seat));
}

void WQuickInputMethodV2::sendContentType(quint32 hint, quint32 purpose)
{
    W_D(WQuickInputMethodV2);
    d->handle()->sendContentType(hint, purpose);
}

void WQuickInputMethodV2::sendActivate()
{
    W_D(WQuickInputMethodV2);
    d->handle()->sendActivate();
}

void WQuickInputMethodV2::sendDeactivate()
{
    W_D(WQuickInputMethodV2);
    d->handle()->sendDeactivate();
}

void WQuickInputMethodV2::sendDone()
{
    W_D(WQuickInputMethodV2);
    d->handle()->sendDone();
}

void WQuickInputMethodV2::sendSurroundingText(const QString &text, quint32 cursor, quint32 anchor)
{
    W_D(WQuickInputMethodV2);
    d->handle()->sendSurroundingText(qPrintable(text), cursor, anchor);
}

void WQuickInputMethodV2::sendTextChangeCause(quint32 cause)
{
    W_D(WQuickInputMethodV2);
    d->handle()->sendTextChangeCause(cause);
}

void WQuickInputMethodV2::sendUnavailable()
{
    W_D(WQuickInputMethodV2);
    d->handle()->sendUnavailable();
}

WQuickInputPopupSurfaceV2::WQuickInputPopupSurfaceV2(QWInputPopupSurfaceV2 *handle, QObject *parent) :
    WWrapObject(*new WQuickInputPopupSurfaceV2Private(handle, this), parent)
{ }

WSurface *WQuickInputPopupSurfaceV2::surface() const
{
    W_DC(WQuickInputPopupSurfaceV2);
    return d->popupSurface;
}

QWInputPopupSurfaceV2 *WQuickInputPopupSurfaceV2::handle() const
{
    return d_func()->handle();
}

void WQuickInputPopupSurfaceV2::sendTextInputRectangle(const QRect &sbox)
{
    W_D(WQuickInputPopupSurfaceV2);
    d->handle()->send_text_input_rectangle(sbox);
}

WQuickInputMethodKeyboardGrabV2::WQuickInputMethodKeyboardGrabV2(QWInputMethodKeyboardGrabV2 *handle, QObject *parent):
    QObject(parent),
    WObject(*new WQuickInputMethodKeyboardGrabV2Private(handle, this), nullptr)
{ }

QWInputMethodKeyboardGrabV2 *WQuickInputMethodKeyboardGrabV2::handle() const
{
    W_DC(WQuickInputMethodKeyboardGrabV2);
    return d->handle;
}

WInputDevice *WQuickInputMethodKeyboardGrabV2::keyboard() const
{
    W_DC(WQuickInputMethodKeyboardGrabV2);
    if (d->handle->handle()->keyboard) {
        return WInputDevice::fromHandle(QWInputDevice::from(&d->handle->handle()->keyboard->base));
    } else {
        return nullptr;
    }
}

void WQuickInputMethodKeyboardGrabV2::sendKey(quint32 time, Qt::Key key, quint32 state)
{
    W_D(WQuickInputMethodKeyboardGrabV2);
    d->handle->sendKey(time, key, state);
}

void WQuickInputMethodKeyboardGrabV2::sendModifiers(uint depressed, uint latched, uint locked, uint group)
{
    W_D(WQuickInputMethodKeyboardGrabV2);
    wlr_keyboard_modifiers wlrModifiers = {.depressed = depressed, .latched = latched, .locked = locked, .group = group};
    d->handle->sendModifiers(&wlrModifiers);
}

void WQuickInputMethodKeyboardGrabV2::setKeyboard(WInputDevice *keyboard)
{
    W_D(WQuickInputMethodKeyboardGrabV2);
    if (keyboard) {
        auto qwKeyboard = qobject_cast<QWKeyboard *>(keyboard->handle());
        auto *virtualKeyboard = qobject_cast<QWVirtualKeyboardV1*>(qwKeyboard);
        // refer to: https://github.com/swaywm/sway/blob/master/sway/input/keyboard.c#L391
        if (virtualKeyboard &&
            wl_resource_get_client(virtualKeyboard->handle()->resource) == d->waylandClient()) {
            return;
        }
        d->handle->setKeyboard(qwKeyboard);
    } else {
        d->handle->setKeyboard(nullptr);
    }
}

class WInputPopupV2Private : public WToplevelSurfacePrivate
{
public:
    W_DECLARE_PUBLIC(WInputPopupV2)
    explicit WInputPopupV2Private(QWInputPopupSurfaceV2 *surface, WSurface *parentSurface, WInputPopupV2 *qq)
        : WToplevelSurfacePrivate(qq)
        , parent(parentSurface)
    {
        initHandle(surface);
    }

    WWRAP_HANDLE_FUNCTIONS(QWInputPopupSurfaceV2, wlr_input_popup_surface_v2)

    QSize size() const
    {
        return {handle()->surface()->handle()->current.width, handle()->surface()->handle()->current.height};
    }

    WSurface *const parent;
};

WInputPopupV2::WInputPopupV2(QWInputPopupSurfaceV2 *surface, WSurface *parentSurface, QObject *parent)
    : WToplevelSurface(*new WInputPopupV2Private(surface, parentSurface, this), parent)
{ }

bool WInputPopupV2::doesNotAcceptFocus() const
{
    return true;
}

WSurface *WInputPopupV2::surface() const
{
    auto wSurface = WSurface::fromHandle(handle()->surface());
    if (!wSurface) {
        wSurface = new WSurface(handle()->surface());
        wSurface->safeConnect(&QWSurface::beforeDestroy, wSurface, &WSurface::safeDeleteLater);
    }
    return wSurface;
}

QWInputPopupSurfaceV2 *WInputPopupV2::handle() const
{
    return d_func()->handle();
}

bool WInputPopupV2::isActivated() const
{
    return true;
}

QRect WInputPopupV2::getContentGeometry() const
{
    return {0, 0, d_func()->size().width(), d_func()->size().height()};
}

WSurface *WInputPopupV2::parentSurface() const
{
    return d_func()->parent;
}

bool WInputPopupV2::checkNewSize(const QSize &size)
{
    return size == d_func()->size();
}

WInputPopupV2Item::WInputPopupV2Item(QQuickItem *parent)
    : WSurfaceItem(parent)
{ }

QString WQuickInputMethodV2::commitString() const
{
    return d_func()->nativeHandle()->current.commit_text;
}

uint WQuickInputMethodV2::deleteSurroundingBeforeLength() const
{
    return d_func()->nativeHandle()->current.delete_c.before_length;
}

uint WQuickInputMethodV2::deleteSurroundingAfterLength() const
{
    return d_func()->nativeHandle()->current.delete_c.after_length;
}

QString WQuickInputMethodV2::preeditString() const
{
    return d_func()->nativeHandle()->current.preedit.text;
}

int WQuickInputMethodV2::preeditCursorBegin() const
{
    return d_func()->nativeHandle()->current.preedit.cursor_begin;
}

int WQuickInputMethodV2::preeditCursorEnd() const
{
    return d_func()->nativeHandle()->current.preedit.cursor_end;
}
WAYLIB_SERVER_END_NAMESPACE
