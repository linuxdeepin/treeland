// Copyright (C) 2024 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wtextinputv2_p.h"
#include "wsocket.h"
#include "wsurface.h"
#include "winputmethodv2_p.h"
#include "wseat.h"

#include <qwcompositor.h>
#include <qwdisplay.h>
#include <qwseat.h>

#include <QLoggingCategory>

extern "C" {
#include "text-input-unstable-v2-protocol.h"
}
QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE
Q_LOGGING_CATEGORY(qLcTextInputV2, "waylib.server.im.ti2", QtInfoMsg)
using namespace tiv2;
static struct zwp_text_input_manager_v2_interface manager_impl = {
    .destroy = handle_manager_destroy, .get_text_input = handle_manager_get_text_input
};

static struct zwp_text_input_v2_interface text_input_impl = {
    .destroy = handle_text_input_destroy,
    .enable = handle_text_input_enable,
    .disable = handle_text_input_disable,
    .show_input_panel = handle_text_input_show_input_panel,
    .hide_input_panel = handle_text_input_hide_input_panel,
    .set_surrounding_text = handle_text_input_set_surrounding_text,
    .set_content_type = handle_text_input_set_content_type,
    .set_cursor_rectangle = handle_text_input_set_cursor_rectangle,
    .set_preferred_language = handle_text_input_set_preferred_language,
    .update_state = handle_text_input_update_state
};
class Q_DECL_HIDDEN WTextInputV2Private : public WTextInputPrivate
{
public:
    W_DECLARE_PUBLIC(WTextInputV2)

    explicit WTextInputV2Private(WTextInputV2 *qq)
        : WTextInputPrivate(qq)
        , resource(nullptr)
        , seat(nullptr)
        , client(nullptr)
        , enabledSurface(nullptr)
        , focusedSurface(nullptr)
        , surroundingText()
        , surroundingCursor(0)
        , surroundingAnchor(0)
    {
    }

    wl_resource *resource;
    WSeat *seat;
    WClient *client;
    WSurface *enabledSurface;
    WSurface *focusedSurface;
    QString surroundingText;
    int32_t surroundingCursor;
    int32_t surroundingAnchor;
    uint32_t contentHint;
    uint32_t contentPurpose;
    QRect cursorRectangle;

    inline void reset()
    {
        // seat = nullptr; //FIXME
        // enabledSurface = nullptr; //FIXME
        // focusedSurface = nullptr; //FIXME
        surroundingText = "";
        surroundingAnchor = 0;
        surroundingCursor = 0;
        contentHint = 0;
        contentPurpose = 0;
        cursorRectangle = QRect();
    }

    wl_client *waylandClient() const override
    {
        return wl_resource_get_client(resource);
    }
};

class Q_DECL_HIDDEN WTextInputManagerV2Private : public WObjectPrivate
{
public:
    W_DECLARE_PUBLIC(WTextInputManagerV2)

    explicit WTextInputManagerV2Private(WTextInputManagerV2 *qq)
        : WObjectPrivate(qq)
        , global(nullptr)
    {
    }

    wl_global *global;
    QList<WTextInputV2 *> textInputs;
};
namespace tiv2 {
WTextInputManagerV2 *manager_from_resource(wl_resource *resource)
{
    Q_ASSERT(wl_resource_instance_of(resource, &zwp_text_input_manager_v2_interface, &manager_impl));
    return static_cast<WTextInputManagerV2 *>(wl_resource_get_user_data(resource));
}

WTextInputV2 *text_input_from_resource(wl_resource *resource)
{
    Q_ASSERT(wl_resource_instance_of(resource, &zwp_text_input_v2_interface, &text_input_impl));
    return static_cast<WTextInputV2 *>(wl_resource_get_user_data(resource));
}

void text_input_manager_bind(wl_client *client,
                             void *data,
                             uint32_t version,
                             uint32_t id)
{
    WTextInputManagerV2 *manager = static_cast<WTextInputManagerV2 *>(data);
    Q_ASSERT(manager);
    wl_resource *resource = wl_resource_create(client,
                                               &zwp_text_input_manager_v2_interface
                                               ,1
                                               ,id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    WClient *wClient = WClient::get(client);
    QObject::connect(wClient, &WClient::destroyed, manager, [resource]{
        wl_resource_destroy(resource);
    });
    wl_resource_set_implementation(resource, &manager_impl, manager, nullptr);
}

void handle_manager_destroy(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client);
    wl_resource_destroy(resource);
}

void handle_text_input_resource_destroy(wl_resource *resource)
{
    auto text_input = text_input_from_resource(resource);
    if (!text_input)
        return;
    Q_EMIT text_input->entityAboutToDestroy();
    delete text_input;
}

void handle_manager_get_text_input(wl_client *client,
                                   wl_resource *resource,
                                   uint32_t id,
                                   wl_resource *seat)
{
    auto seat_client = wlr_seat_client_from_resource(seat);
    if (!seat_client) {
        wl_client_post_implementation_error(client, "Not a valid seat.");
        return;
    }
    auto manager = manager_from_resource(resource);
    Q_ASSERT(manager);
    auto text_input = new WTextInputV2;
    if (!text_input) {
        wl_client_post_no_memory(client);
        return;
    }
    auto version = wl_resource_get_version(resource);
    auto text_input_resource = wl_resource_create(client,
                                                  &zwp_text_input_v2_interface,
                                                  version,
                                                  id);
    if (!text_input_resource) {
        wl_client_post_no_memory(client);
        delete text_input;
        return;
    }
    text_input->d_func()->resource = text_input_resource;
    auto wClient = WClient::get(client);
    auto wSeat = WSeat::fromHandle(qw_seat::from(seat_client->seat));
    Q_ASSERT(wClient);
    Q_ASSERT(wSeat);
    text_input->d_func()->client = wClient;
    text_input->d_func()->seat = wSeat;
    QObject::connect(wClient, &WClient::destroyed, text_input, [text_input_resource] {
        wl_resource_destroy(text_input_resource);
    });
    wl_resource_set_implementation(text_input_resource, &text_input_impl, text_input, handle_text_input_resource_destroy);
    manager->d_func()->textInputs.append(text_input);
    QObject::connect(text_input, &WTextInput::entityAboutToDestroy, manager, [manager, text_input]{
        manager->d_func()->textInputs.removeOne(text_input);
    });
    Q_EMIT manager->newTextInput(text_input);
}

void handle_text_input_destroy(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    wl_resource_destroy(resource);
}

void handle_text_input_enable(wl_client *client, wl_resource *resource, wl_resource *surface)
{
    Q_UNUSED(client)
    auto text_input = text_input_from_resource(resource);
    Q_ASSERT(text_input);
    auto wSurface = WSurface::fromHandle(wlr_surface_from_resource(surface));
    // Surface must be existent already, this means wSurface should always be non-null.
    if (!wSurface) {
        wl_client_post_implementation_error(client, "Enabled surface not found.");
        return;
    }
    auto d = text_input->d_func();
    if (d->enabledSurface) {
        // FIXME: Is this necessary? As protocol does not guarantee disable is ought to happen after enable, we may still need this.
        qCWarning(qLcTextInputV2) << "Client" << client << "does emit disable on surface"
                                 << d->enabledSurface << "before enable on surface" << wSurface;
        Q_EMIT text_input->disableOnSurface(d->enabledSurface);
        d->enabledSurface->safeDisconnect(text_input);
    }
    d->enabledSurface = wSurface;
    wSurface->safeConnect(&qw_surface::before_destroy, text_input, [d, text_input]{
        Q_EMIT text_input->disableOnSurface(d->enabledSurface);
        d->enabledSurface->safeDisconnect(text_input);
        d->enabledSurface = nullptr;
    });
    Q_EMIT text_input->enableOnSurface(wSurface);
}

void handle_text_input_disable(wl_client *client, wl_resource *resource, wl_resource *surface)
{
    Q_UNUSED(client)
    auto text_input = text_input_from_resource(resource);
    Q_ASSERT(text_input);
    auto wSurface = WSurface::fromHandle(wlr_surface_from_resource(surface));
    if (!wSurface) {
        wl_client_post_implementation_error(client, "Disabled surface not found, may be already destroyed.");
        return;
    }
    auto d = text_input->d_func();
    if (!d->enabledSurface) {
        qCWarning(qLcTextInputV2) << "Client" << client
                                    << "try to disable surface" << wSurface
                                    << "on a text input" << text_input
                                    << "that is not enabled on this surface. Do nothing!";
        return;
    }
    if (d->enabledSurface != wSurface) {
        qCWarning(qLcTextInputV2) << "Client" << client
                                    << "try to disable surface" << wSurface
                                    << "on a text input" << text_input
                                    << "which is enabled on another surface" << d->enabledSurface;
        return;
    }
    d->enabledSurface->safeDisconnect(text_input);
    Q_EMIT text_input->disableOnSurface(wSurface);
    d->enabledSurface = nullptr;
}

void handle_text_input_show_input_panel(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    Q_UNUSED(resource)
}

void handle_text_input_hide_input_panel(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    Q_UNUSED(resource)
}

void handle_text_input_set_surrounding_text(
    wl_client *client, wl_resource *resource, const char *text, int32_t cursor, int32_t anchor)
{
    Q_UNUSED(client)
    auto text_input = text_input_from_resource(resource);
    Q_ASSERT(text_input);
    auto d = text_input->d_func();
    d->surroundingText = text;
    d->surroundingCursor = cursor;
    d->surroundingAnchor = anchor;
}

void handle_text_input_set_content_type(wl_client *client,
                                        wl_resource *resource,
                                        uint32_t hint,
                                        uint32_t purpose)
{
    Q_UNUSED(client)
    auto text_input = text_input_from_resource(resource);
    Q_ASSERT(text_input);
    auto d = text_input->d_func();
    d->contentHint = hint;
    d->contentPurpose = purpose;
}

void handle_text_input_set_cursor_rectangle(
    wl_client *client, wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height)
{
    Q_UNUSED(client)
    auto text_input = text_input_from_resource(resource);
    Q_ASSERT(text_input);
    auto d = text_input->d_func();
    d->cursorRectangle = QRect(x, y, width, height);
}

void handle_text_input_set_preferred_language(wl_client *client,
                                              wl_resource *resource,
                                              const char *language)
{
    Q_UNUSED(client)
    Q_UNUSED(resource)
    Q_UNUSED(language)
}

void handle_text_input_update_state(wl_client *client,
                                    wl_resource *resource,
                                    uint32_t serial,
                                    uint32_t reason)
{
    Q_UNUSED(client)
    auto text_input = text_input_from_resource(resource);
    Q_ASSERT(text_input);
    Q_EMIT text_input->stateUpdated(WTextInputV2::UpdateReason(reason));
}
}

WTextInputManagerV2::WTextInputManagerV2(QObject *parent)
    : QObject(parent)
    , WObject(*new WTextInputManagerV2Private(this))
{ }

QByteArrayView WTextInputManagerV2::interfaceName() const
{
    return zwp_text_input_manager_v2_interface.name;
}

void WTextInputManagerV2::create(WServer *server)
{
    W_D(WTextInputManagerV2);
    m_global = wl_global_create(server->handle()->handle(), &zwp_text_input_manager_v2_interface, 1, this, text_input_manager_bind);
    Q_ASSERT(m_global);
    m_handle = this;
}

void WTextInputManagerV2::destroy(WServer *server)
{
    Q_UNUSED(server);
    W_D(WTextInputManagerV2);
    wl_global_destroy(m_global);
    for (auto textInput : std::as_const(d->textInputs)) {
        wl_resource_destroy(textInput->d_func()->resource);
    }
}

WSeat *WTextInputV2::seat() const
{
    W_DC(WTextInputV2);
    return d->seat;
}

WSurface *WTextInputV2::focusedSurface() const
{
    W_DC(WTextInputV2);
    return d->focusedSurface;
}

QString WTextInputV2::surroundingText() const
{
    W_DC(WTextInputV2);
    return d->surroundingText;
}

int WTextInputV2::surroundingCursor() const
{
    W_DC(WTextInputV2);
    return d->surroundingCursor;
}

int WTextInputV2::surroundingAnchor() const
{
    W_DC(WTextInputV2);
    return d->surroundingAnchor;
}

IME::ContentHints WTextInputV2::contentHints() const
{
    W_DC(WTextInputV2);
    return IME::ContentHints();
}

IME::ContentPurpose WTextInputV2::contentPurpose() const
{
    W_DC(WTextInputV2);
    return IME::ContentPurpose();
}

QRect WTextInputV2::cursorRect() const
{
    W_DC(WTextInputV2);
    return d->cursorRectangle;
}

IME::Features WTextInputV2::features() const
{
    return IME::Features(IME::F_SurroundingText | IME::F_ContentType | IME::F_CursorRect);
}

void WTextInputV2::sendEnter(WSurface *surface)
{
    W_D(WTextInputV2);
    d->focusedSurface = surface;
    zwp_text_input_v2_send_enter(d->resource, 0, surface->handle()->handle()->resource);
    if (d->enabledSurface == d->focusedSurface) {
        Q_EMIT enabled();
    }
}

void WTextInputV2::sendLeave()
{
    W_D(WTextInputV2);
    zwp_text_input_v2_send_leave(d->resource, 0, d->focusedSurface->handle()->handle()->resource);
    if (d->enabledSurface == d->focusedSurface) {
        Q_EMIT disabled();
    }
    d->focusedSurface = nullptr;
}

void WTextInputV2::sendDone()
{
}

void WTextInputV2::handleIMCommitted(WInputMethodV2 *im)
{
    W_D(WTextInputV2);
    if (im->deleteSurroundingBeforeLength() || im->deleteSurroundingAfterLength()) {
        zwp_text_input_v2_send_delete_surrounding_text(d->resource, im->deleteSurroundingBeforeLength(), im->deleteSurroundingAfterLength());
    }
    if (!im->commitString().isEmpty()) {
        zwp_text_input_v2_send_commit_string(d->resource, im->commitString().toStdString().c_str());
    }
    if (!im->preeditString().isEmpty()) {
        zwp_text_input_v2_send_preedit_cursor(d->resource, im->preeditCursorEnd() - im->preeditCursorBegin());
        zwp_text_input_v2_send_preedit_styling(d->resource, 0, im->preeditString().length(), PS_Active);
        zwp_text_input_v2_send_preedit_string(d->resource, im->preeditString().toStdString().c_str(), im->commitString().toStdString().c_str());
    }
}

WTextInputV2::WTextInputV2(QObject *parent)
    : WTextInput(*new WTextInputV2Private(this), parent)
{
    connect(this, &WTextInputV2::enableOnSurface, this, [this] {
        if (focusedSurface()) {
            Q_EMIT enabled();
        }
    });
    connect(this, &WTextInputV2::disableOnSurface, this, [this] {
        if (!focusedSurface()) {
            Q_EMIT disabled();
        }
    });
    connect(this, &WTextInput::enabled, this, [this]{
        qCDebug(qLcTextInputV2()) << "text input v2" << this << "enabled";
    });
    connect(this, &WTextInput::disabled, this, [this] {
        qCDebug(qLcTextInputV2()) << "text input v2" << this << "disabled";
    });
    connect(this, &WTextInputV2::stateUpdated, this, [this] (UpdateReason reason){
        qCInfo(qLcTextInputV2()) << "state updated:" << reason;
        switch (reason) {
        case Change:
            break;
        case Enter:
        case Full:
            qCDebug(qLcTextInputV2()) << "commit text input v2" << this;
            Q_EMIT this->committed();
            break;
        case Reset:
            d_func()->reset();
            break;
        default:
            Q_UNREACHABLE();
        }
    });
}
WAYLIB_SERVER_END_NAMESPACE
