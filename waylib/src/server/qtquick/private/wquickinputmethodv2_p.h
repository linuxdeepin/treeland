// Copyright (C) 2023 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wglobal.h"
#include "wquickwaylandserver.h"
#include "wtoplevelsurface.h"
#include "wsurfaceitem.h"

#include <qwglobal.h>
#include <qwinputmethodv2.h>

#include <QObject>
#include <QQmlEngine>

Q_MOC_INCLUDE(<qwinputmethodv2.h>)
Q_MOC_INCLUDE("wsurface.h")

QW_BEGIN_NAMESPACE
class QWInputMethodV2;
class QWInputPopupSurfaceV2;
class QWInputMethodKeyboardGrabV2;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE
class WQuickInputPopupSurfaceV2Private;
class WQuickInputMethodKeyboardGrabV2Private;
class WQuickInputMethodV2Private;
class WInputDevice;
class WSeat;
class WQuickInputMethodManagerV2Private;
class WQuickTextInput;
class WSurface;
class WInputPopupV2Private;

class WQuickInputMethodManagerV2 : public WQuickWaylandServerInterface, public WObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(InputMethodManagerV2)
    W_DECLARE_PRIVATE(WQuickInputMethodManagerV2)

public:
    explicit WQuickInputMethodManagerV2(QObject *parent = nullptr);

Q_SIGNALS:
    void newInputMethod(QW_NAMESPACE::QWInputMethodV2 *inputMethod);

private:
    void create() override;
};

class WQuickInputPopupSurfaceV2 : public QObject, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WQuickInputPopupSurfaceV2)
    Q_PROPERTY(WSurface* surface READ surface CONSTANT FINAL)

public:
    explicit WQuickInputPopupSurfaceV2(QW_NAMESPACE::QWInputPopupSurfaceV2 *handle, QObject *parent = nullptr);
    WSurface *surface() const;
    QW_NAMESPACE::QWInputPopupSurfaceV2 *handle() const;

public Q_SLOTS:
    void sendTextInputRectangle(const QRect &sbox);
};

class WInputPopupV2 : public WToplevelSurface, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WInputPopupV2)
    QML_NAMED_ELEMENT(WaylandInputPopupSurface)
    QML_UNCREATABLE("Only created in C++")

public:
    WInputPopupV2(QW_NAMESPACE::QWInputPopupSurfaceV2 *surface, WSurface *parentSurface, QObject *parent = nullptr);
    WSurface *surface() const override;
    QW_NAMESPACE::QWInputPopupSurfaceV2 *handle() const;
    QRect getContentGeometry() const override;
    bool doesNotAcceptFocus() const override;
    bool isActivated() const override;
    WSurface *parentSurface() const override;

public Q_SLOTS:
    bool checkNewSize(const QSize &size) override;
};

class WInputPopupV2Item : public WSurfaceItem
{
    Q_OBJECT
    Q_PROPERTY(WInputPopupV2* surface READ surface WRITE setSurface NOTIFY surfaceChanged FINAL REQUIRED)
    QML_NAMED_ELEMENT(InputPopupSurfaceItem)

public:
    explicit WInputPopupV2Item(QQuickItem *parent = nullptr);

    WInputPopupV2 *surface() const;
    void setSurface(WInputPopupV2 *surface);

Q_SIGNALS:
    void surfaceChanged();

private:
    WInputPopupV2 *m_inputPopupSurface;
};

class WQuickInputMethodKeyboardGrabV2 : public QObject, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WQuickInputMethodKeyboardGrabV2)

public:
    explicit WQuickInputMethodKeyboardGrabV2(QW_NAMESPACE::QWInputMethodKeyboardGrabV2 *handle, QObject *parent = nullptr);
    QW_NAMESPACE::QWInputMethodKeyboardGrabV2 *handle() const;
    WInputDevice *keyboard() const;

public Q_SLOTS:
    void sendKey(uint time, Qt::Key key, uint state);
    void sendModifiers(uint depressed, uint latched, uint locked, uint group);
    void setKeyboard(WInputDevice *keyboard);
};

class WQuickInputMethodV2 : public QObject, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WQuickInputMethodV2)
public:
    explicit WQuickInputMethodV2(QW_NAMESPACE::QWInputMethodV2 *handle, QObject *parent = nullptr);
    WSeat *seat() const;
    QString commitString() const;
    uint deleteSurroundingBeforeLength() const;
    uint deleteSurroundingAfterLength() const;
    QString preeditString() const;
    int preeditCursorBegin() const;
    int preeditCursorEnd() const;
    QW_NAMESPACE::QWInputMethodV2 *handle() const;

public Q_SLOTS:
    void sendContentType(quint32 hint, quint32 purpose);
    void sendActivate();
    void sendDeactivate();
    void sendDone();
    void sendSurroundingText(const QString &text, quint32 cursor, quint32 anchor);
    void sendTextChangeCause(quint32 cause);
    void sendUnavailable();

Q_SIGNALS:
    void committed();
    void newPopupSurface(QW_NAMESPACE::QWInputPopupSurfaceV2 *surface);
    void newKeyboardGrab(QW_NAMESPACE::QWInputMethodKeyboardGrabV2 *keyboardGrab);
};
WAYLIB_SERVER_END_NAMESPACE
