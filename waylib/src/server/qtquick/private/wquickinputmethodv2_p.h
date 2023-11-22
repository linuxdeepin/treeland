// Copyright (C) 2023 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wquickwaylandserver.h>
#include <wtoplevelsurface.h>
#include <wsurfaceitem.h>

#include <qwglobal.h>
#include <qwinputmethodv2.h>

#include <QObject>
#include <QQmlEngine>

Q_MOC_INCLUDE("wquicktextinputv3_p.h")
Q_MOC_INCLUDE("wsurface.h")

struct wlr_input_method_v2_state;
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
class WQuickInputMethodV2;
class WQuickInputMethodManagerV2Private;
class WQuickTextInputV3;
class WSurface;
class WInputPopupSurfacePrivate;

class WQuickInputMethodManagerV2 : public WQuickWaylandServerInterface, public WObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(InputMethodManagerV2)
    W_DECLARE_PRIVATE(WQuickInputMethodManagerV2)

public:
    explicit WQuickInputMethodManagerV2(QObject *parent = nullptr);

Q_SIGNALS:
    void newInputMethod(WQuickInputMethodV2 *im);

private:
    void create() override;
};

class WQuickInputPopupSurfaceV2 : public QObject, public WObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(InputPopupSurfaceV2)
    QML_UNCREATABLE("Only created by InputMethodV2 in C++")
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
    W_DECLARE_PRIVATE(WInputPopupSurface)
    QML_NAMED_ELEMENT(WaylandInputPopupSurface)
    QML_UNCREATABLE("Only created in C++")

public:
    WInputPopupV2(WQuickInputPopupSurfaceV2 *surface, WSurface *parentSurface, QObject *parent = nullptr);
    WSurface *surface() const override;
    WQuickInputPopupSurfaceV2 *handle() const;
    QW_NAMESPACE::QWInputPopupSurfaceV2 *qwHandle() const;
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
    Q_PROPERTY(WInputPopupV2* surface READ surface WRITE setSurface NOTIFY surfaceChanged REQUIRED)
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
    QML_NAMED_ELEMENT(InputMethodKeyboardGrabV2)
    QML_UNCREATABLE("Only created by InputMethodV2 in C++")
    W_DECLARE_PRIVATE(WQuickInputMethodKeyboardGrabV2)
    Q_PROPERTY(KeyboardModifiers depressedModifiers READ depressedModifiers WRITE setDepressedModifiers NOTIFY depressedModifiersChanged FINAL)
    Q_PROPERTY(KeyboardModifiers latchedModifiers READ latchedModifiers WRITE setLatchedModifiers NOTIFY latchedModifiersChanged FINAL)
    Q_PROPERTY(KeyboardModifiers lockedModifiers READ lockedModifiers WRITE setLockedModifiers NOTIFY lockedModifiersChanged FINAL)
    Q_PROPERTY(KeyboardModifiers group READ group WRITE setGroup NOTIFY groupChanged FINAL)
    Q_PROPERTY(WInputDevice *keyboard READ keyboard FINAL)

public:
    enum KeyboardModifier {
        ModifierShift = 1 << 0,
        ModifierCaps = 1 << 1,
        ModifierCtrl = 1 << 2,
        ModifierAlt = 1 << 3,
        ModifierMod2 = 1 << 4,
        ModifierMod3 = 1 << 5,
        ModifierLogo = 1 << 6,
        ModifierMod5 = 1 << 7,
    };
    Q_FLAG(KeyboardModifier)
    Q_DECLARE_FLAGS(KeyboardModifiers, KeyboardModifier)

    explicit WQuickInputMethodKeyboardGrabV2(QW_NAMESPACE::QWInputMethodKeyboardGrabV2 *handle, QObject *parent = nullptr);
    QW_NAMESPACE::QWInputMethodKeyboardGrabV2 *handle() const;
    wlr_input_method_keyboard_grab_v2 *nativeHandle() const;

    KeyboardModifiers depressedModifiers() const;
    KeyboardModifiers latchedModifiers() const;
    KeyboardModifiers lockedModifiers() const;
    KeyboardModifiers group() const;
    void setDepressedModifiers(KeyboardModifiers modifiers);
    void setLatchedModifiers(KeyboardModifiers modifiers);
    void setLockedModifiers(KeyboardModifiers modifiers);
    void setGroup(KeyboardModifiers group);
    WInputDevice *keyboard() const;

public Q_SLOTS:
    void sendKey(quint32 time, Qt::Key key, quint32 state);
    void sendModifiers();
    void setKeyboard(WInputDevice *keyboard);

Q_SIGNALS:
    void depressedModifiersChanged(KeyboardModifiers);
    void latchedModifiersChanged(KeyboardModifiers);
    void lockedModifiersChanged(KeyboardModifiers);
    void groupChanged(KeyboardModifiers);
};

class WInputMethodV2StatePrivate;
class WInputMethodV2State : public QObject, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WInputMethodV2State)
    Q_PROPERTY(QString commitString READ commitString FINAL)
    Q_PROPERTY(quint32 deleteSurroundingBeforeLength READ deleteSurroundingBeforeLength FINAL)
    Q_PROPERTY(quint32 deleteSurroundingAfterLength READ deleteSurroundingAfterLength FINAL)
    Q_PROPERTY(QString preeditStringText READ preeditStringText FINAL)
    Q_PROPERTY(qint32 preeditStringCursorBegin READ preeditStringCursorBegin FINAL)
    Q_PROPERTY(qint32 preeditStringCursorEnd READ preeditStringCursorEnd FINAL)
public:
    QString commitString() const;
    quint32 deleteSurroundingBeforeLength() const;
    quint32 deleteSurroundingAfterLength() const;
    QString preeditStringText() const;
    qint32 preeditStringCursorBegin() const;
    qint32 preeditStringCursorEnd() const;

private:
    explicit WInputMethodV2State(wlr_input_method_v2_state *handle, QObject *parent = nullptr);
    friend class WQuickInputMethodV2Private;
};

class WQuickInputMethodV2 : public QObject, public WObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(InputMethodV2)
    QML_UNCREATABLE("Only created by InputMethodManagerV2 in C++")
    W_DECLARE_PRIVATE(WQuickInputMethodV2)
    Q_PROPERTY(WSeat *seat READ seat CONSTANT FINAL)
    Q_PROPERTY(QList<WQuickInputPopupSurfaceV2 *> popupSurfaces READ popupSurfaces FINAL)
    Q_PROPERTY(WInputMethodV2State* state READ state FINAL)

public:
    WSeat *seat() const;
    QList<WQuickInputPopupSurfaceV2 *> popupSurfaces() const;
    WInputMethodV2State *state() const;
    QW_NAMESPACE::QWInputMethodV2 *handle() const;

public Q_SLOTS:
    void sendContentType(quint32 hint, quint32 purpose);
    void sendActivate();
    void sendDeactivate();
    void sendDone();
    void sendSurroundingText(const QString &text, quint32 cursor, quint32 anchor);
    void sendTextChangeCause(quint32 cause);
    void sendUnavailable();
    void sendTextInputRectangle(const QRect &rect);

Q_SIGNALS:
    void commit();
    void newPopupSurface(WQuickInputPopupSurfaceV2 *surface);
    void newKeyboardGrab(WQuickInputMethodKeyboardGrabV2 *keyboardGrab);

private:
    explicit WQuickInputMethodV2(QW_NAMESPACE::QWInputMethodV2 *handle, QObject *parent = nullptr);
    friend class WQuickInputMethodManagerV2;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(WQuickInputMethodKeyboardGrabV2::KeyboardModifiers)
WAYLIB_SERVER_END_NAMESPACE
Q_DECLARE_METATYPE(WAYLIB_SERVER_NAMESPACE::WQuickInputMethodKeyboardGrabV2::KeyboardModifiers)
