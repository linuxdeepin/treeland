// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wlr_fwd.h>
#include "wglobal.h"
#include "wserver.h"
#include "wtoplevelsurface.h"

#include <QObject>
#include <QQmlEngine>

Q_MOC_INCLUDE("wsurface.h")

WAYLIB_SERVER_BEGIN_NAMESPACE
class WInputMethodV2Private;
class WSeat;

class WAYLIB_SERVER_EXPORT WInputMethodV2 : public QObject, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WInputMethodV2)
public:
    explicit WInputMethodV2(wlr_input_method_v2 *handle);
    ~WInputMethodV2();
    WSeat *seat() const;
    QString commitString() const;
    uint deleteSurroundingBeforeLength() const;
    uint deleteSurroundingAfterLength() const;
    QString preeditString() const;
    int preeditCursorBegin() const;
    int preeditCursorEnd() const;
    wlr_input_method_v2 *handle() const;

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
    void newPopupSurface(wlr_input_popup_surface_v2 *surface);
    void newKeyboardGrab(wlr_input_method_keyboard_grab_v2 *keyboardGrab);

private:
    friend class WInputMethodManagerV2;
};

class WInputMethodManagerV2Private;
class WAYLIB_SERVER_EXPORT WInputMethodManagerV2 : public QObject, public WObject, public WServerInterface
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WInputMethodManagerV2)

public:
    explicit WInputMethodManagerV2(QObject *parent = nullptr);

    QByteArrayView interfaceName() const override;
    wlr_input_method_manager_v2 *handle() const;

Q_SIGNALS:
    void newInputMethod(wlr_input_method_v2 *inputMethod);

private:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
};

WAYLIB_SERVER_END_NAMESPACE
