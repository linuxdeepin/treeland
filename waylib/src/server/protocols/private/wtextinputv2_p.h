// Copyright (C) 2024 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wglobal.h"
#include "wserver.h"
#include "wtextinput_p.h"

#include <qwglobal.h>

#include <QObject>
#include <QRect>

WAYLIB_SERVER_BEGIN_NAMESPACE
class WTextInputV2Private;
namespace tiv2 {
void handle_manager_destroy(wl_client *client, wl_resource *resource);
void handle_manager_get_text_input(wl_client *client,
                                   wl_resource *resource,
                                   uint32_t id,
                                   wl_resource *seat);
void handle_text_input_destroy(wl_client *client, wl_resource *resource);
void handle_text_input_enable(wl_client *client, wl_resource *resource, wl_resource *surface);
void handle_text_input_disable(wl_client *client, wl_resource *resource, wl_resource *surface);
void handle_text_input_show_input_panel(wl_client *client, wl_resource *resource);
void handle_text_input_hide_input_panel(wl_client *client, wl_resource *resource);
void handle_text_input_set_surrounding_text(
    wl_client *client, wl_resource *resource, const char *text, int32_t cursor, int32_t anchor);
void handle_text_input_set_content_type(wl_client *client,
                                        wl_resource *resource,
                                        uint32_t hint,
                                        uint32_t purpose);
void handle_text_input_set_cursor_rectangle(
    wl_client *client, wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height);
void handle_text_input_set_preferred_language(wl_client *client,
                                              wl_resource *resource,
                                              const char *language);
void handle_text_input_update_state(wl_client *client,
                                    wl_resource *resource,
                                    uint32_t serial,
                                    uint32_t reason);

}
class WAYLIB_SERVER_EXPORT WTextInputV2 : public WTextInput
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WTextInputV2)
public:
    WSeat *seat() const override;
    WSurface *focusedSurface() const override;
    QString surroundingText() const override;
    int surroundingCursor() const override;
    int surroundingAnchor() const override;
    IME::ContentHints contentHints() const override;
    IME::ContentPurpose contentPurpose() const override;
    QRect cursorRect() const override;
    IME::Features features() const override;
    enum UpdateReason {
        Change,
        Full,
        Reset,
        Enter
    };
    Q_ENUM(UpdateReason)

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


Q_SIGNALS:
    // As protocol says, enable will be before or after enter event. This is not the same as our logic.
    // We consider text input v2 enabled only if it emits enable and receives enter event and the focused
    // surface is the same as enabled surface.
    void enableOnSurface(WSurface *surface);
    void disableOnSurface(WSurface *surface);
    void stateUpdated(UpdateReason reason);

public Q_SLOTS:
    void sendEnter(WSurface *surface) override;
    void sendLeave() override;
    void sendDone() override;
    void handleIMCommitted(WInputMethodV2 *im) override;

private:
    explicit WTextInputV2(QObject *parent = nullptr);
    friend class WTextInputManagerV2;
    friend void tiv2::handle_manager_get_text_input(wl_client *client,
                                              wl_resource *resource,
                                              uint32_t id,
                                              wl_resource *seat);
    friend void tiv2::handle_text_input_enable(wl_client *client,
                                         wl_resource *resource,
                                         wl_resource *surface);
    friend void tiv2::handle_text_input_disable(wl_client *client,
                                          wl_resource *resource,
                                          wl_resource *surface);
    friend void tiv2::handle_text_input_set_surrounding_text(
        wl_client *client, wl_resource *resource, const char *text, int32_t cursor, int32_t anchor);
    friend void tiv2::handle_text_input_set_content_type(wl_client *client,
                                                   wl_resource *resource,
                                                   uint32_t hint,
                                                   uint32_t purpose);
    friend void tiv2::handle_text_input_set_cursor_rectangle(wl_client *client,
                                                       wl_resource *resource,
                                                       int32_t x,
                                                       int32_t y,
                                                       int32_t width,
                                                       int32_t height);
    friend void tiv2::handle_text_input_update_state(wl_client *client,
                                               wl_resource *resource,
                                               uint32_t serial,
                                               uint32_t reason);
};

class WTextInputManagerV2Private;

class WAYLIB_SERVER_EXPORT WTextInputManagerV2 : public QObject, public WObject, public WServerInterface
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WTextInputManagerV2)
public:
    explicit WTextInputManagerV2(QObject *parent = nullptr);

    QByteArrayView interfaceName() const override;

Q_SIGNALS:
    void newTextInput(WTextInputV2 *textInput);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override {
        return m_global;
    }

private:
    friend void tiv2::handle_manager_get_text_input(wl_client *client,
                                              wl_resource *resource,
                                              uint32_t id,
                                              wl_resource *seat);

    wl_global *m_global = nullptr;
};

WAYLIB_SERVER_END_NAMESPACE
