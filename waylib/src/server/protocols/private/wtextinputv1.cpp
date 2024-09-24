// Copyright (C) 2023 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wtextinputv1_p.h"
#include "winputmethodv2_p.h"
#include "wseat.h"
#include "wsurface.h"
#include "wsocket.h"
#include "private/wglobal_p.h"

#include <qwdisplay.h>
#include <qwseat.h>
#include <qwcompositor.h>
#include <qwsignalconnector.h>

#include <QRect>

extern "C" {
#include "text-input-unstable-v1-protocol.h"
}
QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE
class Q_DECL_HIDDEN WTextInputV1Private : public WTextInputPrivate
{
public:
    W_DECLARE_PUBLIC(WTextInputV1)
    explicit WTextInputV1Private(WTextInputV1 *qq)
        : WTextInputPrivate(qq)
    {
    }

    wl_client *waylandClient() const override
    {
        return wl_resource_get_client(resource);
    }

    wl_resource *resource {nullptr};
    WSeat *seat {nullptr};
    WSurface *focusedSurface {nullptr};
    uint32_t currentSerial {0};
    bool active {false};
    QString surroundingText {};
    uint32_t surroundingCursor {0};
    uint32_t surroundingAnchor {0};
    uint32_t contentHint {0};
    uint32_t contentPurpose {0};
    QRect cursorRectangle {};
};

class Q_DECL_HIDDEN WTextInputManagerV1Private : public WObjectPrivate
{
public:
    W_DECLARE_PUBLIC(WTextInputManagerV1)
    explicit WTextInputManagerV1Private(WTextInputManagerV1 *qq)
        : WObjectPrivate(qq)
    {
    }

    QList<WTextInputV1*> textInputs;
};


WSeat *WTextInputV1::seat() const
{
    return d_func()->seat;
}

QString WTextInputV1::surroundingText() const
{
    return d_func()->surroundingText;
}

int WTextInputV1::surroundingCursor() const
{
    return d_func()->surroundingCursor;
}

int WTextInputV1::surroundingAnchor() const
{
    return d_func()->surroundingAnchor;
}

IME::ContentHints WTextInputV1::contentHints() const
{
    // Note:: Only convert trivial flag
    static const QList<QPair<WTextInputV1::ContentHint, IME::ContentHint>> hintsMap = {
        {WTextInputV1::CH_HiddenText, IME::CH_HiddenText},
        {WTextInputV1::CH_AutoCapitalization, IME::CH_AutoCapitalization},
        {WTextInputV1::CH_AutoCompletion, IME::CH_Completion},
        {WTextInputV1::CH_AutoCorrection, IME::CH_Spellcheck},
        {WTextInputV1::CH_Lowercase, IME::CH_Lowercase},
        {WTextInputV1::CH_Uppercase, IME::CH_Uppercase},
        {WTextInputV1::CH_Latin, IME::CH_Latin},
        {WTextInputV1::CH_Titlecase, IME::CH_Titlecase},
        {WTextInputV1::CH_Multiline, IME::CH_Multiline},
        {WTextInputV1::CH_SensitiveData, IME::CH_SensitiveData}
    };
    auto convertToHints = [](WTextInputV1::ContentHints hints) -> IME::ContentHints {
        IME::ContentHints result;
        for (auto hint : std::as_const(hintsMap)) {
            result.setFlag(hint.second, hints.testFlag(hint.first));
        }
        return result;
    };
    return convertToHints(ContentHints(d_func()->contentHint));
}

IME::ContentPurpose WTextInputV1::contentPurpose() const
{
    switch (d_func()->contentPurpose) {
    case WTextInputV1::CP_Alpha: return IME::CP_Alpha;
    case WTextInputV1::CP_Normal: return IME::CP_Normal;
    case WTextInputV1::CP_Date: return IME::CP_Date;
    case WTextInputV1::CP_Time: return IME::CP_Time;
    case WTextInputV1::CP_Terminal: return IME::CP_Terminal;
    case WTextInputV1::CP_Digits: return IME::CP_Digits;
    case WTextInputV1::CP_Email: return IME::CP_Email;
    case WTextInputV1::CP_Name: return IME::CP_Name;
    case WTextInputV1::CP_Number: return IME::CP_Number;
    case WTextInputV1::CP_Password: return IME::CP_Password;
    case WTextInputV1::CP_Phone: return IME::CP_Phone;
    case WTextInputV1::CP_Datetime: return IME::CP_Datetime;
    case WTextInputV1::CP_Url: return IME::CP_Url;
    default:
        Q_UNREACHABLE();
    }
}

QRect WTextInputV1::cursorRect() const
{
    return d_func()->cursorRectangle;
}

WSurface *WTextInputV1::focusedSurface() const
{
    return d_func()->focusedSurface;
}

IME::Features WTextInputV1::features() const
{
    return IME::Features(IME::F_SurroundingText | IME::F_ContentType | IME::F_CursorRect);
}


void WTextInputV1::sendEnter(WSurface *surface)
{
    zwp_text_input_v1_send_enter(d_func()->resource, surface->handle()->handle()->resource);
    Q_EMIT this->enabled();
}

void WTextInputV1::sendLeave()
{
    if (focusedSurface()) {
        zwp_text_input_v1_send_leave(d_func()->resource);
        Q_EMIT disabled();
    }
}

void WTextInputV1::sendDone()
{

}

void WTextInputV1::handleIMCommitted(WInputMethodV2 *im)
{
    if (im->deleteSurroundingBeforeLength() || im->deleteSurroundingAfterLength()) {
        sendDeleteSurroundingText(0, surroundingText().length() - im->deleteSurroundingBeforeLength() - im->deleteSurroundingAfterLength());
    }
    sendCommitString(im->commitString());
    if (!im->preeditString().isEmpty()) {
        sendPreeditCursor(im->preeditCursorEnd() - im->preeditCursorBegin());
        sendPreeditStyling(0, im->preeditString().length(), WTextInputV1::PS_Active);
        sendPreeditString(im->preeditString(), im->commitString());
    }
}


void WTextInputV1::sendPreeditString(QString text, QString commit)
{
    zwp_text_input_v1_send_preedit_string(d_func()->resource, d_func()->currentSerial, text.toStdString().c_str(), commit.toStdString().c_str());
}

void WTextInputV1::sendPreeditStyling(uint index, uint length, PreeditStyle style)
{
    zwp_text_input_v1_send_preedit_styling(d_func()->resource, index, length, style);
}

void WTextInputV1::sendPreeditCursor(int index)
{
    zwp_text_input_v1_send_preedit_cursor(d_func()->resource, index);
}

void WTextInputV1::sendCommitString(QString text)
{
    zwp_text_input_v1_send_commit_string(d_func()->resource, d_func()->currentSerial, text.toStdString().c_str());
}

void WTextInputV1::sendCursorPosition(int index, int anchor)
{
    zwp_text_input_v1_send_cursor_position(d_func()->resource, index, anchor);
}

void WTextInputV1::sendDeleteSurroundingText(int index, uint length)
{
    zwp_text_input_v1_send_delete_surrounding_text(d_func()->resource, index, length);
}

WTextInputV1::WTextInputV1(QObject *parent)
    : WTextInput(*new WTextInputV1Private(this), parent)
{
    W_D(WTextInputV1);
    connect(this, &WTextInputV1::activate, this, &WTextInputV1::requestFocus);
    connect(this, &WTextInputV1::deactivate, this, [this] {
        // Disconnect all signals excluding requestFocus, as text input may be activated again in
        // another seat, leaving signal connected might interfere another seat.
        Q_EMIT this->requestLeave();
        disconnect(SIGNAL(committed()));
        disconnect(SIGNAL(enabled()));
        disconnect(SIGNAL(disabled()));
        disconnect(SIGNAL(requestLeave()));
    });
}

static WTextInputV1 *text_input_from_resource(wl_resource *resource);
void text_input_handle_activate(wl_client *client,
                                wl_resource *resource,
                                wl_resource *seat,
                                wl_resource *surface)
{
    wlr_seat_client *seat_client =  wlr_seat_client_from_resource(seat);
    WSeat *wSeat = WSeat::fromHandle(qw_seat::from(seat_client->seat));
    Q_ASSERT(wSeat);
    WTextInputV1 *text_input = text_input_from_resource(resource);
    auto d = text_input->d_func();
    bool seat_changed = (text_input->seat() != wSeat);
    wlr_surface *focused_surface = wlr_surface_from_resource(surface);
    WSurface *wSurface = WSurface::fromHandle(focused_surface);
    Q_ASSERT(wSurface);
    bool focus_changed = (text_input->focusedSurface() != wSurface);
    if (seat_changed) {
        d->seat = wSeat;
    }
    if (focus_changed) {
        if (text_input->focusedSurface())
            text_input->focusedSurface()->safeDisconnect(text_input);
        d->focusedSurface = wSurface;
        wSurface->safeConnect(&qw_surface::before_destroy, text_input, [d, text_input]{
            d->focusedSurface = nullptr;
        });
    }
    d->active = true;
    Q_EMIT text_input->activate();
}

void text_input_handle_deactivate(wl_client *client,
                                  wl_resource *resource,
                                  wl_resource *seat)
{
    WTextInputV1 *text_input = text_input_from_resource(resource);
    auto d = text_input->d_func();
    wlr_seat_client *seat_client = wlr_seat_client_from_resource(seat);
    WSeat *wSeat = WSeat::fromHandle(qw_seat::from(seat_client->seat));
    if (!wSeat || wSeat != text_input->seat())
        return;

    d->seat = nullptr;
    Q_EMIT text_input->deactivate();
}

void text_input_handle_show_input_panel(wl_client *client,
                                        wl_resource *resource)
{
    Q_UNUSED(client);
    Q_UNUSED(resource);
}

void text_input_handle_hide_input_panel(wl_client *client,
                                        wl_resource *resource)
{
    Q_UNUSED(client);
    Q_UNUSED(resource);
}

void text_input_handle_reset(wl_client *client,
                             wl_resource *resource)
{
    WTextInputV1 *text_input = text_input_from_resource(resource);
    auto d = text_input->d_func();
    d->surroundingText = "";
    d->surroundingAnchor = 0;
    d->surroundingCursor = 0;
    d->cursorRectangle = QRect();
    d->contentHint = 0;
    d->contentPurpose = 0;
}

void text_input_handle_set_surrounding_text(wl_client *client,
                                            wl_resource *resource,
                                            const char *text,
                                            uint32_t cursor,
                                            uint32_t anchor)
{
    WTextInputV1 *text_input = text_input_from_resource(resource);
    auto d = text_input->d_func();
    d->surroundingText = text;
    d->surroundingCursor = cursor;
    d->surroundingAnchor = anchor;
}

void text_input_handle_set_content_type(wl_client *client,
                                        wl_resource *resource,
                                        uint32_t hint,
                                        uint32_t purpose)
{
    WTextInputV1 *text_input = text_input_from_resource(resource);
    auto d = text_input->d_func();
    d->contentHint = hint;
    d->contentPurpose = purpose;
}

void text_input_handle_set_cursor_rectangle(wl_client *client,
                                            wl_resource *resource,
                                            int32_t x,
                                            int32_t y,
                                            int32_t width,
                                            int32_t height)
{
    WTextInputV1 *text_input = text_input_from_resource(resource);
    auto d = text_input->d_func();
    d->cursorRectangle = QRect(x, y, width, height);
}

void text_input_handle_set_preferred_language(wl_client *client,
                                              wl_resource *resource,
                                              const char *language)
{
    Q_UNUSED(client);
    Q_UNUSED(resource);
    Q_UNUSED(language);
}

void text_input_handle_commit_state(wl_client *client,
                                    wl_resource *resource,
                                    uint32_t serial)
{
    WTextInputV1 *text_input = text_input_from_resource(resource);
    auto d = text_input->d_func();
    d->currentSerial = serial; // FIXME: Should we add one?
    Q_EMIT text_input->committed();
}

void text_input_handle_invoke_action(wl_client *client,
                                     wl_resource *resource,
                                     uint32_t button,
                                     uint32_t index)
{
    Q_UNUSED(client);
    Q_UNUSED(resource);
    Q_UNUSED(button);
    Q_UNUSED(index);
}

static const struct zwp_text_input_v1_interface text_input_v1_impl = {
    .activate = text_input_handle_activate,
    .deactivate = text_input_handle_deactivate,
    .show_input_panel = text_input_handle_show_input_panel,
    .hide_input_panel = text_input_handle_hide_input_panel,
    .reset = text_input_handle_reset,
    .set_surrounding_text = text_input_handle_set_surrounding_text,
    .set_content_type = text_input_handle_set_content_type,
    .set_cursor_rectangle = text_input_handle_set_cursor_rectangle,
    .set_preferred_language = text_input_handle_set_preferred_language,
    .commit_state = text_input_handle_commit_state,
    .invoke_action = text_input_handle_invoke_action
};

WTextInputV1 *text_input_from_resource(wl_resource *resource)
{
    assert(wl_resource_instance_of(resource, &zwp_text_input_v1_interface, &text_input_v1_impl));
    return reinterpret_cast<WTextInputV1 *>(wl_resource_get_user_data(resource));
}

static void text_input_resource_destroy(wl_resource * resource)
{
    WTextInputV1 *text_input = text_input_from_resource(resource);
    Q_ASSERT(text_input);
    Q_EMIT text_input->entityAboutToDestroy();
    delete text_input;
}

WTextInputManagerV1 *text_input_manager_from_resource(wl_resource *resource);

void text_input_manager_get_text_input(wl_client *client, wl_resource *resource, uint32_t id)
{
    WTextInputManagerV1 *manager = text_input_manager_from_resource(resource);
    Q_ASSERT(manager);
    int version = wl_resource_get_version(resource);
    WTextInputV1 *text_input = new WTextInputV1;
    auto d = text_input->d_func();
    if (!text_input) {
        wl_client_post_no_memory(client);
        return;
    }
    struct wl_resource *text_input_resource = wl_resource_create(client, &zwp_text_input_v1_interface, version, id);
    if (!text_input_resource) {
        delete text_input;
        wl_client_post_no_memory(client);
        return;
    }
    d->resource = text_input_resource;
    QObject::connect(text_input->waylandClient(), &WClient::destroyed, text_input, [text_input] {
        wl_resource_destroy(text_input->d_func()->resource);
    });
    wl_resource_set_implementation(text_input_resource, &text_input_v1_impl, text_input, text_input_resource_destroy);
    manager->d_func()->textInputs.append(text_input);
    QObject::connect(text_input, &WTextInputV1::entityAboutToDestroy, manager, [manager, text_input]{
        manager->d_func()->textInputs.removeOne(text_input);
    });
    Q_EMIT manager->newTextInput(text_input);
}

static struct zwp_text_input_manager_v1_interface text_input_manager_v1_impl = {
    .create_text_input = text_input_manager_get_text_input
};

WTextInputManagerV1 *text_input_manager_from_resource(wl_resource *resource)
{
    assert(wl_resource_instance_of(resource, &zwp_text_input_manager_v1_interface, &text_input_manager_v1_impl));
    return reinterpret_cast<WTextInputManagerV1 *>(resource->data);
}

static void text_input_manager_bind(wl_client *wl_client, void *data, uint32_t version, uint32_t id)
{
    WTextInputManagerV1 *manager = reinterpret_cast<WTextInputManagerV1 *>(data);
    wl_resource *resource = wl_resource_create(wl_client, &zwp_text_input_manager_v1_interface, version, id);
    WClient *wClient = WClient::get(wl_client);
    QObject::connect(wClient, &WClient::destroyed, manager, [resource]{
        wl_resource_destroy(resource);
    });
    if (!resource) {
        wl_client_post_no_memory(wl_client);
        return;
    }
    wl_resource_set_implementation(resource, &text_input_manager_v1_impl, manager, nullptr);
}

void WTextInputManagerV1::create(WServer *server)
{
    W_D(WTextInputManagerV1);
    m_global = wl_global_create(server->handle()->handle(),
                                &zwp_text_input_manager_v1_interface,
                                1,
                                this,
                                text_input_manager_bind);
    Q_ASSERT(m_global);
    m_handle = this;
}

void WTextInputManagerV1::destroy(WServer *server)
{
    Q_UNUSED(server);
    W_D(WTextInputManagerV1);
    wl_global_destroy(m_global);
    for (auto textInput : std::as_const(d->textInputs)) {
        wl_resource_destroy(textInput->d_func()->resource);
    }
}

WTextInputManagerV1::WTextInputManagerV1(QObject *parent)
    : QObject(parent)
    , WObject(*new WTextInputManagerV1Private(this))
{ }

QByteArrayView WTextInputManagerV1::interfaceName() const
{
    return zwp_text_input_manager_v1_interface.name;
}

CHECK_ENUM(WTextInputV1::CH_None, ZWP_TEXT_INPUT_V1_CONTENT_HINT_NONE);
CHECK_ENUM(WTextInputV1::CH_Default, ZWP_TEXT_INPUT_V1_CONTENT_HINT_DEFAULT);
CHECK_ENUM(WTextInputV1::CH_Password, ZWP_TEXT_INPUT_V1_CONTENT_HINT_PASSWORD);
CHECK_ENUM(WTextInputV1::CH_AutoCompletion, ZWP_TEXT_INPUT_V1_CONTENT_HINT_AUTO_COMPLETION);
CHECK_ENUM(WTextInputV1::CH_AutoCorrection, ZWP_TEXT_INPUT_V1_CONTENT_HINT_AUTO_CORRECTION);
CHECK_ENUM(WTextInputV1::CH_AutoCapitalization, ZWP_TEXT_INPUT_V1_CONTENT_HINT_AUTO_CAPITALIZATION);
CHECK_ENUM(WTextInputV1::CH_Lowercase, ZWP_TEXT_INPUT_V1_CONTENT_HINT_LOWERCASE);
CHECK_ENUM(WTextInputV1::CH_Uppercase, ZWP_TEXT_INPUT_V1_CONTENT_HINT_UPPERCASE);
CHECK_ENUM(WTextInputV1::CH_Titlecase, ZWP_TEXT_INPUT_V1_CONTENT_HINT_TITLECASE);
CHECK_ENUM(WTextInputV1::CH_HiddenText, ZWP_TEXT_INPUT_V1_CONTENT_HINT_HIDDEN_TEXT);
CHECK_ENUM(WTextInputV1::CH_SensitiveData, ZWP_TEXT_INPUT_V1_CONTENT_HINT_SENSITIVE_DATA);
CHECK_ENUM(WTextInputV1::CH_Latin, ZWP_TEXT_INPUT_V1_CONTENT_HINT_LATIN);
CHECK_ENUM(WTextInputV1::CH_Multiline, ZWP_TEXT_INPUT_V1_CONTENT_HINT_MULTILINE);
CHECK_ENUM(WTextInputV1::CP_Normal, ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_NORMAL);
CHECK_ENUM(WTextInputV1::CP_Alpha, ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_ALPHA);
CHECK_ENUM(WTextInputV1::CP_Digits, ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_DIGITS);
CHECK_ENUM(WTextInputV1::CP_Number, ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_NUMBER);
CHECK_ENUM(WTextInputV1::CP_Phone, ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_PHONE);
CHECK_ENUM(WTextInputV1::CP_Url, ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_URL);
CHECK_ENUM(WTextInputV1::CP_Email, ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_EMAIL);
CHECK_ENUM(WTextInputV1::CP_Name, ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_NAME);
CHECK_ENUM(WTextInputV1::CP_Password, ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_PASSWORD);
CHECK_ENUM(WTextInputV1::CP_Date, ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_DATE);
CHECK_ENUM(WTextInputV1::CP_Time, ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_TIME);
CHECK_ENUM(WTextInputV1::CP_Datetime, ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_DATETIME);
CHECK_ENUM(WTextInputV1::CP_Terminal, ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_TERMINAL);
CHECK_ENUM(WTextInputV1::PS_Default, ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_DEFAULT);
CHECK_ENUM(WTextInputV1::PS_None, ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_NONE);
CHECK_ENUM(WTextInputV1::PS_Active, ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_ACTIVE);
CHECK_ENUM(WTextInputV1::PS_Inactive, ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_INACTIVE);
CHECK_ENUM(WTextInputV1::PS_Highlight, ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_HIGHLIGHT);
CHECK_ENUM(WTextInputV1::PS_Underline, ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_UNDERLINE);
CHECK_ENUM(WTextInputV1::PS_Selection, ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_SELECTION);
CHECK_ENUM(WTextInputV1::PS_Incorrect, ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_INCORRECT);
WAYLIB_SERVER_END_NAMESPACE
