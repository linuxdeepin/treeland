// Copyright (C) 2023 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wglobal.h"
#include "wserver.h"
#include "wtextinput_p.h"

#include <qwglobal.h>

#include <QObject>
#include <QRect>

extern "C" {
#include <wayland-server.h>
}

WAYLIB_SERVER_BEGIN_NAMESPACE
class WSurface;
class WSeat;
class WTextInputV1;
class WTextInputManagerV1Private;
class WTextInputV1Private;
class WSurface;
class WSeat;
class WTextInputV1;
class WTextInputManagerV1;
class WTextInputV1Private;
class WTextInputManagerV1Private;
class WAYLIB_SERVER_EXPORT WTextInputV1 : public WTextInput
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WTextInputV1)

public:
    enum ContentHint {
        CH_None = 0x0,
        CH_Default = 0x7,
        CH_Password = 0xc0,
        CH_AutoCompletion = 0x1,
        CH_AutoCorrection = 0x2,
        CH_AutoCapitalization = 0x4,
        CH_Lowercase = 0x8,
        CH_Uppercase = 0x10,
        CH_Titlecase = 0x20,
        CH_HiddenText = 0x40,
        CH_SensitiveData = 0x80,
        CH_Latin = 0x100,
        CH_Multiline = 0x200
    };
    Q_FLAG(ContentHint)
    Q_DECLARE_FLAGS(ContentHints, ContentHint)

    enum ContentPurpose {
        CP_Normal,
        CP_Alpha,
        CP_Digits,
        CP_Number,
        CP_Phone,
        CP_Url,
        CP_Email,
        CP_Name,
        CP_Password,
        CP_Date,
        CP_Time,
        CP_Datetime,
        CP_Terminal
    };
    Q_ENUM(ContentPurpose)

    enum PreeditStyle {
        PS_Default,
        PS_None,
        PS_Active,
        PS_Inactive,
        PS_Highlight,
        PS_Underline,
        PS_Selection,
        PS_Incorrect
    };
    Q_ENUM(PreeditStyle)

    explicit WTextInputV1(QObject *parent = nullptr);
    WSeat *seat() const override;
    QString surroundingText() const override;
    int surroundingCursor() const override;
    int surroundingAnchor() const override;
    IME::ContentHints contentHints() const override;
    IME::ContentPurpose contentPurpose() const override;
    QRect cursorRect() const override;
    WSurface *focusedSurface() const override;
    IME::Features features() const override;

Q_SIGNALS:
    void activate();
    void deactivate();

public Q_SLOTS:
    void sendEnter(WSurface *surface) override;
    void sendLeave() override;
    void sendDone() override;
    void handleIMCommitted(WInputMethodV2 *im) override;

private:
    void sendPreeditString(QString text, QString commit);
    void sendPreeditStyling(uint index, uint length, PreeditStyle style);
    void sendPreeditCursor(int index);
    void sendCommitString(QString text);
    void sendCursorPosition(int index, int anchor);
    void sendDeleteSurroundingText(int index, uint length);

    friend void text_input_manager_get_text_input(wl_client *client,
                                                  wl_resource *resource,
                                                  uint32_t id);
    friend void text_input_handle_activate(wl_client *client,
                                           wl_resource *resource,
                                           wl_resource *seat,
                                           wl_resource *surface);
    friend void text_input_handle_deactivate(wl_client *client,
                                             wl_resource *resource,
                                             wl_resource *seat);
    friend void text_input_handle_reset(wl_client *client,
                                        wl_resource *resource);
    friend void text_input_handle_set_surrounding_text(wl_client *client,
                                                       wl_resource *resource,
                                                       const char *text,
                                                       uint32_t cursor,
                                                       uint32_t anchor);
    friend void text_input_handle_set_content_type(wl_client *client,
                                                   wl_resource *resource,
                                                   uint32_t hint,
                                                   uint32_t purpose);
    friend void text_input_handle_set_cursor_rectangle(wl_client *client,
                                                       wl_resource *resource,
                                                       int32_t x, int32_t y,
                                                       int32_t width,
                                                       int32_t height);
    friend void text_input_handle_commit_state(wl_client *client,
                                               wl_resource *resource,
                                               uint32_t serial);
    friend class WTextInputManagerV1;
};

class WAYLIB_SERVER_EXPORT WTextInputManagerV1 : public QObject, public WObject, public WServerInterface
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WTextInputManagerV1)

public:
    explicit WTextInputManagerV1(QObject *parent = nullptr);

    QByteArrayView interfaceName() const override;

Q_SIGNALS:
    void newTextInput(WTextInputV1 *textInput);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override {
        return m_global;
    }

private:
    friend void text_input_manager_get_text_input(wl_client *client, wl_resource *resource, uint32_t id);

    wl_global *m_global = nullptr;
};
WAYLIB_SERVER_END_NAMESPACE
