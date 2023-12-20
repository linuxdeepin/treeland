// Copyright (C) 2023 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquickinputmethodv2_p.h"
#include "wseat.h"
#include "winputdevice.h"
#include "wsurface.h"
#include "wxdgsurface.h"

#include <qwcompositor.h>
#include <qwinputmethodv2.h>
#include <qwseat.h>
#include <qwkeyboard.h>

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

void WQuickInputMethodManagerV2::create()
{
    W_D(WQuickInputMethodManagerV2);
    WQuickWaylandServerInterface::create();
    d->manager = QWInputMethodManagerV2::create(server()->handle());
    connect(d->manager, &QWInputMethodManagerV2::inputMethod, this, [this, d] (QWInputMethodV2 *im) {
        auto quickInputMethod = new WQuickInputMethodV2(im, this);
        connect(quickInputMethod->handle(), &QWInputMethodV2::beforeDestroy, this, [d, quickInputMethod]() {
            d->inputMethods.removeAll(quickInputMethod);
            quickInputMethod->deleteLater();
        });
        d->inputMethods.append(quickInputMethod);
        Q_EMIT this->newInputMethod(quickInputMethod);
    });
}

class WQuickInputPopupSurfaceV2Private : public WObjectPrivate
{
public:
    WQuickInputPopupSurfaceV2Private(QWInputPopupSurfaceV2 *h, WQuickInputPopupSurfaceV2 *qq)
        : WObjectPrivate(qq)
        , handle(h)
        , popupSurface(WSurface::fromHandle(h->surface()))
    {
        if (!popupSurface) {
            popupSurface = new WSurface(h->surface());
            QObject::connect(h->surface(), &QWSurface::beforeDestroy, popupSurface, &WSurface::deleteLater);
        }
    }

    W_DECLARE_PUBLIC(WQuickInputPopupSurfaceV2)
    QWInputPopupSurfaceV2 *handle;
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
    QWInputMethodKeyboardGrabV2 *handle;
    wlr_keyboard_modifiers modifiers;
};

class WQuickInputMethodV2Private : public WObjectPrivate
{
public:
    WQuickInputMethodV2Private(QWInputMethodV2 *h, WQuickInputMethodV2 *qq)
        : WObjectPrivate(qq)
        , handle(h)
        , activeKeyboardGrab(nullptr)
        , state(new WInputMethodV2State(&h->handle()->current, qq))
    { }

    QWInputMethodV2 *handle;
    QList<WQuickInputPopupSurfaceV2 *> popupSurfaces;
    WQuickInputMethodKeyboardGrabV2 *activeKeyboardGrab;
    WInputMethodV2State *const state;

    W_DECLARE_PUBLIC(WQuickInputMethodV2)
};

WQuickInputMethodV2::WQuickInputMethodV2(QWInputMethodV2 *handle, QObject *parent) :
    QObject(parent),
    WObject(*new WQuickInputMethodV2Private(handle, this), nullptr)
{
    W_D(WQuickInputMethodV2);
    connect(d->handle, &QWInputMethodV2::commit, this, &WQuickInputMethodV2::commit);
    connect(d->handle, &QWInputMethodV2::newPopupSurface, this, [this, d](QWInputPopupSurfaceV2 *surface) {
        auto quickPopupSurface = new WQuickInputPopupSurfaceV2(surface, this);
        d->popupSurfaces.append(quickPopupSurface);
        connect(quickPopupSurface->handle(), &QWInputPopupSurfaceV2::beforeDestroy, this, [d, quickPopupSurface]() {
            d->popupSurfaces.removeAll(quickPopupSurface);
            quickPopupSurface->deleteLater();
        });
        Q_EMIT this->newPopupSurface(quickPopupSurface);
    });
    connect(d->handle, &QWInputMethodV2::grabKeybord, this, [this, d](QWInputMethodKeyboardGrabV2 *keyboardGrab){
        auto quickKeyboardGrab = new WQuickInputMethodKeyboardGrabV2(keyboardGrab, this);
        d->activeKeyboardGrab = quickKeyboardGrab;
        connect(quickKeyboardGrab->handle(), &QWInputMethodKeyboardGrabV2::beforeDestroy, this, [d, quickKeyboardGrab]() {
            d->activeKeyboardGrab = nullptr;
            quickKeyboardGrab->deleteLater();
        });
        Q_EMIT this->newKeyboardGrab(quickKeyboardGrab);
    });
}

QList<WQuickInputPopupSurfaceV2 *> WQuickInputMethodV2::popupSurfaces() const
{
    W_DC(WQuickInputMethodV2);
    return d->popupSurfaces;
}

WInputMethodV2State *WQuickInputMethodV2::state() const
{
    W_DC(WQuickInputMethodV2);
    return d->state;
}

QWInputMethodV2 *WQuickInputMethodV2::handle() const
{
    return d_func()->handle;
}

WSeat *WQuickInputMethodV2::seat() const
{
    W_DC(WQuickInputMethodV2);
    return WSeat::fromHandle(QWSeat::from(d->handle->handle()->seat));
}

void WQuickInputMethodV2::sendContentType(quint32 hint, quint32 purpose)
{
    W_D(WQuickInputMethodV2);
    d->handle->sendContentType(hint, purpose);
}

void WQuickInputMethodV2::sendActivate()
{
    W_D(WQuickInputMethodV2);
    d->handle->sendActivate();
}

void WQuickInputMethodV2::sendDeactivate()
{
    W_D(WQuickInputMethodV2);
    d->handle->sendDeactivate();
}

void WQuickInputMethodV2::sendDone()
{
    W_D(WQuickInputMethodV2);
    d->handle->sendDone();
}

void WQuickInputMethodV2::sendSurroundingText(const QString &text, quint32 cursor, quint32 anchor)
{
    W_D(WQuickInputMethodV2);
    d->handle->sendSurroundingText(qPrintable(text), cursor, anchor);
}

void WQuickInputMethodV2::sendTextChangeCause(quint32 cause)
{
    W_D(WQuickInputMethodV2);
    d->handle->sendTextChangeCause(cause);
}

void WQuickInputMethodV2::sendUnavailable()
{
    W_D(WQuickInputMethodV2);
    d->handle->sendUnavailable();
}

void WQuickInputMethodV2::sendTextInputRectangle(const QRect &rect)
{
    W_D(WQuickInputMethodV2);
    // FIXME: Just send text input rectangle to all
    for (auto popupSurface : d->popupSurfaces) {
        popupSurface->sendTextInputRectangle(rect);
    }
}

WQuickInputPopupSurfaceV2::WQuickInputPopupSurfaceV2(QWInputPopupSurfaceV2 *handle, QObject *parent) :
    QObject(parent),
    WObject(*new WQuickInputPopupSurfaceV2Private(handle, this), nullptr)
{ }

WSurface *WQuickInputPopupSurfaceV2::surface() const
{
    W_DC(WQuickInputPopupSurfaceV2);
    return d->popupSurface;
}

QWInputPopupSurfaceV2 *WQuickInputPopupSurfaceV2::handle() const
{
    return d_func()->handle;
}


void WQuickInputPopupSurfaceV2::sendTextInputRectangle(const QRect &sbox)
{
    W_D(WQuickInputPopupSurfaceV2);
    d->handle->send_text_input_rectangle(sbox);
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

wlr_input_method_keyboard_grab_v2 *WQuickInputMethodKeyboardGrabV2::nativeHandle() const
{
    W_DC(WQuickInputMethodKeyboardGrabV2);
    return d->handle->handle();
}

WQuickInputMethodKeyboardGrabV2::KeyboardModifiers WQuickInputMethodKeyboardGrabV2::depressedModifiers() const
{
    W_DC(WQuickInputMethodKeyboardGrabV2);
    return {d->modifiers.depressed};
}

WQuickInputMethodKeyboardGrabV2::KeyboardModifiers WQuickInputMethodKeyboardGrabV2::latchedModifiers() const
{
    W_DC(WQuickInputMethodKeyboardGrabV2);
    return {d->modifiers.latched};
}

WQuickInputMethodKeyboardGrabV2::KeyboardModifiers WQuickInputMethodKeyboardGrabV2::lockedModifiers() const
{
    W_DC(WQuickInputMethodKeyboardGrabV2);
    return {d->modifiers.locked};
}

WQuickInputMethodKeyboardGrabV2::KeyboardModifiers WQuickInputMethodKeyboardGrabV2::group() const
{
    W_DC(WQuickInputMethodKeyboardGrabV2);
    return {d->modifiers.group};
}

void WQuickInputMethodKeyboardGrabV2::setDepressedModifiers(KeyboardModifiers modifiers)
{
    W_D(WQuickInputMethodKeyboardGrabV2);
    d->modifiers.depressed = modifiers;
}

void WQuickInputMethodKeyboardGrabV2::setLatchedModifiers(KeyboardModifiers modifiers)
{
    W_D(WQuickInputMethodKeyboardGrabV2);
    d->modifiers.latched = modifiers;
}

void WQuickInputMethodKeyboardGrabV2::setLockedModifiers(KeyboardModifiers modifiers)
{
    W_D(WQuickInputMethodKeyboardGrabV2);
    d->modifiers.locked = modifiers;
}

void WQuickInputMethodKeyboardGrabV2::setGroup(KeyboardModifiers group)
{
    W_D(WQuickInputMethodKeyboardGrabV2);
    d->modifiers.group = group;
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

void WQuickInputMethodKeyboardGrabV2::sendModifiers()
{
    W_D(WQuickInputMethodKeyboardGrabV2);
    d->handle->sendModifiers(&d->modifiers);
}

void WQuickInputMethodKeyboardGrabV2::setKeyboard(WInputDevice *keyboard)
{
    W_D(WQuickInputMethodKeyboardGrabV2);
    if (keyboard) {
        auto qwKeyboard = qobject_cast<QWKeyboard *>(keyboard->handle());
        d->handle->setKeyboard(qwKeyboard);
    } else {
        d->handle->setKeyboard(nullptr);
    }
}

class WInputMethodV2StatePrivate : public WObjectPrivate
{
    W_DECLARE_PUBLIC(WInputMethodV2State)
public:
    WInputMethodV2StatePrivate(wlr_input_method_v2_state *h, WInputMethodV2State *qq)
        : WObjectPrivate(qq)
        , handle(h)
    {}
    wlr_input_method_v2_state *const handle;
};

QString WInputMethodV2State::commitString() const
{
    W_DC(WInputMethodV2State);
    return d->handle->commit_text;
}

quint32 WInputMethodV2State::deleteSurroundingBeforeLength() const
{
    W_DC(WInputMethodV2State);
    return d->handle->delete_c.before_length;
}

quint32 WInputMethodV2State::deleteSurroundingAfterLength() const
{
    W_DC(WInputMethodV2State);
    return d->handle->delete_c.after_length;
}

QString WInputMethodV2State::preeditStringText() const
{
    W_DC(WInputMethodV2State);
    return d->handle->preedit.text;
}

qint32 WInputMethodV2State::preeditStringCursorBegin() const
{
    W_DC(WInputMethodV2State);
    return d->handle->preedit.cursor_begin;
}

qint32 WInputMethodV2State::preeditStringCursorEnd() const
{
    W_DC(WInputMethodV2State);
    return d->handle->preedit.cursor_end;
}

WInputMethodV2State::WInputMethodV2State(wlr_input_method_v2_state *handle, QObject *parent)
    : QObject(parent)
    , WObject(*new WInputMethodV2StatePrivate(handle, this))
{}

class WInputPopupSurfacePrivate : public WObjectPrivate
{
public:
    W_DECLARE_PUBLIC(WInputPopupV2)
    explicit WInputPopupSurfacePrivate(WQuickInputPopupSurfaceV2 *surface, WSurface *parentSurface, WInputPopupV2 *qq)
        : WObjectPrivate(qq)
        , handle(surface)
        , parent(parentSurface)
    { }

    QSize size() const
    {
        return {handle->surface()->handle()->handle()->current.width, handle->surface()->handle()->handle()->current.height};
    }

    WQuickInputPopupSurfaceV2 *handle;
    WSurface *const parent;
};

WInputPopupV2::WInputPopupV2(WQuickInputPopupSurfaceV2 *surface, WSurface *parentSurface, QObject *parent)
    : WToplevelSurface(parent)
    , WObject(*new WInputPopupSurfacePrivate(surface, parentSurface, this))
{ }

bool WInputPopupV2::doesNotAcceptFocus() const
{
    return true;
}

WSurface *WInputPopupV2::surface() const
{
    W_DC(WInputPopupSurface);
    return d->handle->surface();
}

WQuickInputPopupSurfaceV2 *WInputPopupV2::handle() const
{
    return d_func()->handle;
}

QWInputPopupSurfaceV2 *WInputPopupV2::qwHandle() const
{
    return d_func()->handle->handle();
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
    , m_inputPopupSurface(nullptr)
{ }

WInputPopupV2 *WInputPopupV2Item::surface() const
{
    return m_inputPopupSurface;
}

void WInputPopupV2Item::setSurface(WInputPopupV2 *surface)
{
    if (m_inputPopupSurface == surface)
        return;
    m_inputPopupSurface = surface;
    WSurfaceItem::setSurface(surface ? surface->surface() : nullptr);
    Q_EMIT this->surfaceChanged();
}

WAYLIB_SERVER_END_NAMESPACE
