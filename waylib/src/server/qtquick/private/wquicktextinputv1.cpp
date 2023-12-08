// Copyright (C) 2023 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquicktextinputv1_p.h"
#include "wseat.h"
#include "wsurface.h"
#include "wtools.h"

#include <qwdisplay.h>
#include <qwsignalconnector.h>
#include <qwseat.h>
#include <qwcompositor.h>

#include <QRect>

#include <text-input-unstable-v1-protocol.h>
#include <wayland-server-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server.h>

extern "C" {
#include <wlr/types/wlr_seat.h>
#define static
#include <wlr/types/wlr_compositor.h>
#undef static
#include <wlr/util/box.h>
}

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE
struct ws_text_input_v1 {
    wl_resource *resource;

    wlr_surface *focused_surface;

    uint32_t current_serial;

    wlr_seat *seat;

    bool active;

    wl_listener surface_destroy;

    struct {
        char *text;
        uint32_t cursor;
        uint32_t anchor;
    } surrounding_text;

    struct {
        uint32_t hint;
        uint32_t purpose;
    } content_type;

    struct {
        uint32_t button;
        uint32_t index;
    } invoke_action;

    wlr_box cursor_rectangle;

    char *preferred_language;

    struct {
        wl_signal activate;
        wl_signal deactivate;
        wl_signal show_input_panel;
        wl_signal hide_input_panel;
        wl_signal reset;
        wl_signal set_surrounding_text;
        wl_signal set_content_type;
        wl_signal set_cursor_rectangle;
        wl_signal set_preferred_language;
        wl_signal commit_state;
        wl_signal invoke_action;
        wl_signal destroy;
    } events;

    wl_list link;

    ws_text_input_v1()
        : resource{nullptr}
        , focused_surface{nullptr}
        , current_serial{0}
        , seat{nullptr}
        , active{false}
        , surface_destroy{}
        , surrounding_text{}
        , content_type{}
        , invoke_action{}
        , cursor_rectangle{}
        , preferred_language{nullptr}
        , events{}
        , link{}
    {
        wl_signal_init(&events.destroy);
        wl_signal_init(&events.activate);
        wl_signal_init(&events.deactivate);
        wl_signal_init(&events.show_input_panel);
        wl_signal_init(&events.hide_input_panel);
        wl_signal_init(&events.set_surrounding_text);
        wl_signal_init(&events.set_content_type);
        wl_signal_init(&events.set_cursor_rectangle);
        wl_signal_init(&events.set_preferred_language);
        wl_signal_init(&events.commit_state);
        wl_signal_init(&events.invoke_action);
        wl_signal_init(&events.reset);
        wl_list_init(&surface_destroy.link);
        wl_list_init(&link);
    }
};

struct ws_text_input_manager_v1
{
    wl_list text_inputs;
    wl_global *global;

    wl_listener display_destroy;

    struct {
        wl_signal text_input;
        wl_signal destroy;
    } events;
};

ws_text_input_manager_v1 *text_input_manager_v1_create(wl_display *display);
class WQuickTextInputV1Private : public WObjectPrivate
{
public:
    W_DECLARE_PUBLIC(WQuickTextInputV1)
    explicit WQuickTextInputV1Private(ws_text_input_v1 *h, WQuickTextInputV1 *qq)
        : WObjectPrivate(qq)
        , handle(h)
    {
        sc.connect(&handle->events.destroy, qq, &WQuickTextInputV1::beforeDestroy);
        sc.connect(&handle->events.activate, qq, &WQuickTextInputV1::activeChanged);
        sc.connect(&handle->events.activate, qq, &WQuickTextInputV1::seatChanged);
        sc.connect(&handle->events.deactivate, qq, &WQuickTextInputV1::activeChanged);
        sc.connect(&handle->events.deactivate, qq, &WQuickTextInputV1::seatChanged);
        sc.connect(&handle->events.hide_input_panel, qq, &WQuickTextInputV1::hideInputPanel);
        sc.connect(&handle->events.show_input_panel, qq, &WQuickTextInputV1::showInputPanel);
        sc.connect(&handle->events.set_surrounding_text, qq, &WQuickTextInputV1::surroundingTextChanged);
        sc.connect(&handle->events.set_content_type, qq, &WQuickTextInputV1::contentTypeChanged);
        sc.connect(&handle->events.set_cursor_rectangle, qq, &WQuickTextInputV1::cursorRectangleChanged);
        sc.connect(&handle->events.set_preferred_language, qq, &WQuickTextInputV1::preferredLanguageChanged);
        sc.connect(&handle->events.commit_state, qq, &WQuickTextInputV1::commit);
        sc.connect(&handle->events.invoke_action, this, &WQuickTextInputV1Private::on_invoke_action);
    }

    void on_invoke_action()
    {
        Q_EMIT q_func()->invoke(handle->invoke_action.button, handle->invoke_action.index);
    }

    ws_text_input_v1 *handle;
    QWSignalConnector sc;
};
class WQuickTextInputManagerV1Private : public WObjectPrivate
{
public:
    W_DECLARE_PUBLIC(WQuickTextInputManagerV1)
    explicit WQuickTextInputManagerV1Private(WQuickTextInputManagerV1 *qq)
        : WObjectPrivate(qq)
        , handle(nullptr)
    { }

    void create(QWDisplay *display)
    {
        handle = text_input_manager_v1_create(display->handle());
        sc.connect(&handle->events.text_input, this, &WQuickTextInputManagerV1Private::on_text_input);
    }

    void on_text_input(ws_text_input_v1 *text_input)
    {
        W_Q(WQuickTextInputManagerV1);
        WQuickTextInputV1 *textInputV1 = new WQuickTextInputV1(text_input, q);
        textInputs.append(textInputV1);
        QObject::connect(textInputV1, &WQuickTextInputV1::beforeDestroy, q, [this, q, textInputV1](){
            textInputs.removeAll(textInputV1);
            textInputV1->deleteLater();
        });
        Q_EMIT q->newTextInput(textInputV1);
    }

    ws_text_input_manager_v1 *handle;
    QList<WQuickTextInputV1 *> textInputs;
    QWSignalConnector sc;
};

WQuickTextInputManagerV1::WQuickTextInputManagerV1(QObject *parent)
    : WQuickWaylandServerInterface(parent)
    , WObject(*new WQuickTextInputManagerV1Private(this))
{ }

QList<WQuickTextInputV1 *> WQuickTextInputManagerV1::textInputs() const
{
    return d_func()->textInputs;
}

WSeat *WQuickTextInputV1::seat() const
{
    return d_func()->handle->seat ? WSeat::fromHandle(QWSeat::from(d_func()->handle->seat)) : nullptr;
}

bool WQuickTextInputV1::active() const
{
    return d_func()->handle->active;
}

QString WQuickTextInputV1::surroundingText() const
{
    return QString(d_func()->handle->surrounding_text.text);
}

quint32 WQuickTextInputV1::surroundingTextCursor() const
{
    return d_func()->handle->surrounding_text.cursor;
}

quint32 WQuickTextInputV1::surroundingTextAnchor() const
{
    return d_func()->handle->surrounding_text.anchor;
}

WQuickTextInputV1::ContentHint WQuickTextInputV1::contentHint() const
{
    return static_cast<ContentHint>(d_func()->handle->content_type.hint);
}

WQuickTextInputV1::ContentPurpose WQuickTextInputV1::contentPurpose() const
{
    return static_cast<ContentPurpose>(d_func()->handle->content_type.purpose);
}

QRect WQuickTextInputV1::cursorRectangle() const
{
    return WTools::fromWLRBox(&d_func()->handle->cursor_rectangle);
}

QString WQuickTextInputV1::preferredLanguage() const
{
    return QString(d_func()->handle->preferred_language);
}

WSurface *WQuickTextInputV1::focusedSurface() const
{
    return WSurface::fromHandle(d_func()->handle->focused_surface);
}

void WQuickTextInputV1::sendEnter(WSurface *surface)
{
    auto ti = d_func()->handle;
    ti->focused_surface = surface->handle()->handle();
    zwp_text_input_v1_send_enter(ti->resource, ti->focused_surface->resource);
}

void WQuickTextInputV1::sendLeave()
{
    zwp_text_input_v1_send_leave(d_func()->handle->resource);
}

void WQuickTextInputV1::sendModifiersMap(QStringList modifiersMap)
{
    wl_array modifiers_map_arr;
    wl_array_init(&modifiers_map_arr);
    for (const QString &modifier : modifiersMap) {
        QByteArray ba = modifier.toLatin1();
        char *p = static_cast<char *>(wl_array_add(&modifiers_map_arr, ba.length() + 1));
        strncpy(p, ba.data(), ba.length());
    }
    zwp_text_input_v1_send_modifiers_map(d_func()->handle->resource, &modifiers_map_arr);
    wl_array_release(&modifiers_map_arr);
}

void WQuickTextInputV1::sendInputPanelState(quint32 state)
{
    zwp_text_input_v1_send_input_panel_state(d_func()->handle->resource, state);
}

void WQuickTextInputV1::sendPreeditString(QString text, QString commit)
{
    zwp_text_input_v1_send_preedit_string(d_func()->handle->resource, d_func()->handle->current_serial++, text.toStdString().c_str(), commit.toStdString().c_str());
}

void WQuickTextInputV1::sendPreeditStyling(quint32 index, quint32 length, PreeditStyle style)
{
    zwp_text_input_v1_send_preedit_styling(d_func()->handle->resource, index, length, style);
}

void WQuickTextInputV1::sendPreeditCursor(qint32 index)
{
    zwp_text_input_v1_send_preedit_cursor(d_func()->handle->resource, index);
}

void WQuickTextInputV1::sendCommitString(QString text)
{
    zwp_text_input_v1_send_commit_string(d_func()->handle->resource, d_func()->handle->current_serial++, text.toStdString().c_str());
}

void WQuickTextInputV1::sendCursorPosition(qint32 index, qint32 anchor)
{
    zwp_text_input_v1_send_cursor_position(d_func()->handle->resource, index, anchor);
}

void WQuickTextInputV1::sendDeleteSurroundingText(qint32 index, quint32 length)
{
    zwp_text_input_v1_send_delete_surrounding_text(d_func()->handle->resource, index, length);
}

void WQuickTextInputV1::sendKeySym(quint32 time, quint32 sym, quint32 state, quint32 modifiers)
{
    zwp_text_input_v1_send_keysym(d_func()->handle->resource, d_func()->handle->current_serial++, time, sym, state, modifiers);
}

void WQuickTextInputV1::sendLanguage(QString language)
{
    zwp_text_input_v1_send_language(d_func()->handle->resource, d_func()->handle->current_serial++, language.toStdString().c_str());
}

void WQuickTextInputV1::sendTextDirection(TextDirection textDirection)
{
    zwp_text_input_v1_send_text_direction(d_func()->handle->resource, d_func()->handle->current_serial++, textDirection);
}

WQuickTextInputV1::WQuickTextInputV1(ws_text_input_v1 *handle, QObject *parent)
    : QObject(parent)
    , WObject(*new WQuickTextInputV1Private(handle, this))
{ }

ws_text_input_v1 *text_input_from_resource(wl_resource *resource);
void text_input_handle_activate(struct wl_client *client,
                                struct wl_resource *resource,
                                struct wl_resource *seat,
                                struct wl_resource *surface)
{
    // assert(client == resource->client);
    wlr_seat_client *seat_client =  wlr_seat_client_from_resource(seat);
    ws_text_input_v1 *text_input = text_input_from_resource(resource);
    text_input->seat = seat_client->seat;
    text_input->active = true;
    wlr_surface *focused_surface = wlr_surface_from_resource(surface);
    wl_signal_add(&focused_surface->events.destroy, &text_input->surface_destroy);
    // FIXME: Should we use a pending state?
    text_input->focused_surface = focused_surface;
    wl_signal_emit_mutable(&text_input->events.activate, text_input);
    // TODO: As soon as activated, compositor should send enter to client.
}

void text_input_handle_deactivate(struct wl_client *client,
                                  struct wl_resource *resource,
                                  struct wl_resource *seat)
{
    ws_text_input_v1 *text_input = text_input_from_resource(resource);
    text_input->seat = nullptr;
    text_input->active = false;
    wl_signal_emit_mutable(&text_input->events.deactivate, text_input);
}

void text_input_handle_show_input_panel(struct wl_client *client,
                                        struct wl_resource *resource)
{
    ws_text_input_v1 *text_input = text_input_from_resource(resource);
    wl_signal_emit_mutable(&text_input->events.show_input_panel, text_input);
}

void text_input_handle_hide_input_panel(struct wl_client *client,
                                        struct wl_resource *resource)
{
    ws_text_input_v1 *text_input = text_input_from_resource(resource);
    wl_signal_emit_mutable(&text_input->events.hide_input_panel, text_input);
}

void text_input_handle_reset(struct wl_client *client,
                             struct wl_resource *resource)
{
    ws_text_input_v1 *text_input = text_input_from_resource(resource);
    free(text_input->surrounding_text.text);
    text_input->surrounding_text = {};
    text_input->cursor_rectangle = {};
    text_input->content_type = {};
    // TODO: Refer to weston-editor, these states must be cleared.
    wl_signal_emit(&text_input->events.reset, text_input);
}

void text_input_handle_set_surrounding_text(struct wl_client *client,
                                            struct wl_resource *resource,
                                            const char *text,
                                            uint32_t cursor,
                                            uint32_t anchor)
{
    ws_text_input_v1 *text_input = text_input_from_resource(resource);
    free(text_input->surrounding_text.text);
    text_input->surrounding_text.text = strdup(text);
    text_input->surrounding_text.cursor = cursor;
    text_input->surrounding_text.anchor = anchor;
    wl_signal_emit_mutable(&text_input->events.set_surrounding_text, text_input);
}

void text_input_handle_set_content_type(struct wl_client *client,
                                        struct wl_resource *resource,
                                        uint32_t hint,
                                        uint32_t purpose)
{
    ws_text_input_v1 *text_input = text_input_from_resource(resource);
    text_input->content_type.hint = hint;
    text_input->content_type.purpose = purpose;
    wl_signal_emit_mutable(&text_input->events.set_content_type, text_input);
}

void text_input_handle_set_cursor_rectangle(struct wl_client *client,
                                            struct wl_resource *resource,
                                            int32_t x,
                                            int32_t y,
                                            int32_t width,
                                            int32_t height)
{
    ws_text_input_v1 *text_input = text_input_from_resource(resource);
    text_input->cursor_rectangle.x = x;
    text_input->cursor_rectangle.y = y;
    text_input->cursor_rectangle.width = width;
    text_input->cursor_rectangle.height = height;
    wl_signal_emit_mutable(&text_input->events.set_cursor_rectangle, text_input);
}

void text_input_handle_set_preferred_language(struct wl_client *client,
                                              struct wl_resource *resource,
                                              const char *language)
{
    ws_text_input_v1 *text_input = text_input_from_resource(resource);
    free(text_input->preferred_language);
    text_input->preferred_language = strdup(language);
    wl_signal_emit_mutable(&text_input->events.set_preferred_language, text_input);
}

void text_input_handle_commit_state(struct wl_client *client,
                                    struct wl_resource *resource,
                                    uint32_t serial)
{
    ws_text_input_v1 *text_input = text_input_from_resource(resource);
    text_input->current_serial = serial;
    wl_signal_emit_mutable(&text_input->events.commit_state, text_input);
}

void text_input_handle_invoke_action(struct wl_client *client,
                                     struct wl_resource *resource,
                                     uint32_t button,
                                     uint32_t index)
{
    ws_text_input_v1 *text_input = text_input_from_resource(resource);
    text_input->invoke_action.button = button;
    text_input->invoke_action.index = index;
    wl_signal_emit(&text_input->events.invoke_action, text_input);
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

ws_text_input_v1 *text_input_from_resource(wl_resource *resource)
{
    assert(wl_resource_instance_of(resource, &zwp_text_input_v1_interface, &text_input_v1_impl));
    return reinterpret_cast<ws_text_input_v1 *>(wl_resource_get_user_data(resource));
}

static void text_input_resource_destroy(wl_resource * resource)
{
    ws_text_input_v1 *text_input = text_input_from_resource(resource);
    wl_signal_emit_mutable(&text_input->events.destroy, text_input);
    // TODO: Release resource and recycle.
}

static void text_input_handle_focused_surface_destroy(struct wl_listener *listener, void *data)
{
    // FIXME: What should we do when surface is destroyed but deactivate signal not received
    // Should we deactivate the text input automatically or just send leave
    ws_text_input_v1 *text_input = wl_container_of(listener, text_input, surface_destroy);
    wl_list_remove(&text_input->surface_destroy.link);
    wl_list_init(&text_input->surface_destroy.link);
    text_input->focused_surface = nullptr;
    zwp_text_input_v1_send_leave(text_input->resource);
}

ws_text_input_manager_v1 *text_input_manager_from_resource(wl_resource *resource);

static void text_input_manager_get_text_input(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
    int version = wl_resource_get_version(resource);
    struct wl_resource *text_input_resource = wl_resource_create(client, &zwp_text_input_v1_interface, version, id);
    if (!text_input_resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(text_input_resource, &text_input_v1_impl, nullptr, text_input_resource_destroy);
    ws_text_input_v1 *text_input = new ws_text_input_v1;
    text_input->resource = text_input_resource;
    wl_resource_set_user_data(text_input_resource, text_input);
    text_input->surface_destroy.notify = text_input_handle_focused_surface_destroy;

    ws_text_input_manager_v1 *manager = text_input_manager_from_resource(resource);
    wl_list_insert(&manager->text_inputs, &text_input->link);
    wl_signal_emit_mutable(&manager->events.text_input, text_input);
}

static struct zwp_text_input_manager_v1_interface text_input_manager_v1_impl = {
    .create_text_input = text_input_manager_get_text_input
};

ws_text_input_manager_v1 *text_input_manager_from_resource(wl_resource *resource)
{
    assert(wl_resource_instance_of(resource, &zwp_text_input_manager_v1_interface, &text_input_manager_v1_impl));
    return reinterpret_cast<ws_text_input_manager_v1 *>(resource->data);
}

static void text_input_manager_bind(struct wl_client *wl_client, void *data, uint32_t version, uint32_t id)
{
    ws_text_input_manager_v1 *manager = reinterpret_cast<ws_text_input_manager_v1 *>(data);
    wl_resource *resource = wl_resource_create(wl_client, &zwp_text_input_manager_v1_interface, version, id);
    if (!resource) {
        wl_client_post_no_memory(wl_client);
        return;
    }
    wl_resource_set_implementation(resource, &text_input_manager_v1_impl, manager, nullptr);
}

static void handle_display_destroy(wl_listener *listener, void *data)
{
    ws_text_input_manager_v1 *manager = wl_container_of(listener, manager, display_destroy);
    wl_signal_emit_mutable(&manager->events.destroy, manager);
    wl_list_remove(&manager->display_destroy.link);
    wl_global_destroy(manager->global);
    delete manager;
}

ws_text_input_manager_v1 *text_input_manager_v1_create(wl_display *display)
{
    ws_text_input_manager_v1 *manager = new ws_text_input_manager_v1;

    wl_list_init(&manager->text_inputs);
    wl_signal_init(&manager->events.text_input);
    wl_signal_init(&manager->events.destroy);

    manager->global = wl_global_create(display,
                                       &zwp_text_input_manager_v1_interface,
                                       1,
                                       manager,
                                       text_input_manager_bind);
    if (!manager->global) {
        delete manager;
        return nullptr;
    }

    manager->display_destroy.notify = handle_display_destroy;
    wl_display_add_destroy_listener(display, &manager->display_destroy);

    return manager;
}

void WQuickTextInputManagerV1::create()
{
    W_D(WQuickTextInputManagerV1);
    WQuickWaylandServerInterface::create();
    d->create(server()->handle());
}

WAYLIB_SERVER_END_NAMESPACE



