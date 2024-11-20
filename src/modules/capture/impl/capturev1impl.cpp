// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "capturev1impl.h"

#include <wsocket.h>
#include <wsurface.h>

#include <QDebug>

extern "C" {
#define static
#include "wlr/types/wlr_compositor.h"
#undef static
}

WAYLIB_SERVER_USE_NAMESPACE

QW_USE_NAMESPACE
static const struct treeland_capture_session_v1_interface session_impl = {
    .destroy = handle_treeland_capture_session_v1_destroy,
    .start = handle_treeland_capture_session_v1_start
};

static const struct treeland_capture_manager_v1_interface manager_impl = {
    .destroy = handle_treeland_capture_manager_v1_destroy,
    .get_context = handle_treeland_capture_manager_v1_get_context
};

static const struct treeland_capture_context_v1_interface context_impl = {
    .destroy = handle_treeland_capture_context_v1_destroy,
    .select_source = handle_treeland_capture_context_v1_select_source,
    .capture = handle_treeland_capture_context_v1_capture,
    .create_session = handle_treeland_capture_context_v1_create_session
};

static const struct treeland_capture_frame_v1_interface frame_impl = {
    .destroy = handle_treeland_capture_frame_v1_destroy,
    .copy = handle_treeland_capture_frame_v1_copy
};

void handle_treeland_capture_context_v1_destroy([[maybe_unused]] wl_client *client,
                                                wl_resource *resource)
{
    wl_resource_destroy(resource);
}
struct treeland_capture_context_v1 *capture_context_from_resource(struct wl_resource *resource);

void handle_treeland_capture_manager_v1_destroy([[maybe_unused]] struct wl_client *client,
                                                struct wl_resource *resource)
{
    wl_resource_destroy(resource);
}

treeland_capture_manager_v1 *capture_manager_from_resource(wl_resource *resource)
{
    Q_ASSERT(
        wl_resource_instance_of(resource, &treeland_capture_manager_v1_interface, &manager_impl));
    return static_cast<treeland_capture_manager_v1 *>(wl_resource_get_user_data(resource));
}

treeland_capture_session_v1 *capture_session_from_resource(wl_resource *resource)
{
    Q_ASSERT(
        wl_resource_instance_of(resource, &treeland_capture_session_v1_interface, &session_impl));
    return static_cast<treeland_capture_session_v1 *>(wl_resource_get_user_data(resource));
}

treeland_capture_context_v1 *capture_context_from_resource(wl_resource *resource)
{
    Q_ASSERT(
        wl_resource_instance_of(resource, &treeland_capture_context_v1_interface, &context_impl));
    return static_cast<treeland_capture_context_v1 *>(wl_resource_get_user_data(resource));
}

treeland_capture_frame_v1 *capture_frame_from_resource(wl_resource *resource)
{
    Q_ASSERT(wl_resource_instance_of(resource, &treeland_capture_frame_v1_interface, &frame_impl));
    return static_cast<treeland_capture_frame_v1 *>(wl_resource_get_user_data(resource));
}

void capture_session_resource_destroy(struct wl_resource *resource)
{
    auto session = capture_session_from_resource(resource);
    if (!session) {
        return;
    }
    Q_EMIT session->beforeDestroy();
    delete session;
}

void capture_context_resource_destroy(struct wl_resource *resource)
{
    struct treeland_capture_context_v1 *context = capture_context_from_resource(resource);
    if (!context) {
        return;
    }
    Q_EMIT context->beforeDestroy();
    delete context;
}

void capture_frame_resource_destroy(struct wl_resource *resource)
{
    struct treeland_capture_frame_v1 *frame = capture_frame_from_resource(resource);
    if (!frame) {
        return;
    }
    Q_EMIT frame->beforeDestroy();
    delete frame;
}

void treeland_capture_manager_bind(wl_client *client, void *data, uint32_t version, uint32_t id)
{
    auto *manager = static_cast<treeland_capture_manager_v1 *>(data);
    Q_ASSERT(client && manager);
    wl_resource *resource =
        wl_resource_create(client, &treeland_capture_manager_v1_interface, version, id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    manager->addClientResource(client, resource);
    wl_resource_set_implementation(resource, &manager_impl, manager, nullptr);
}

treeland_capture_manager_v1::treeland_capture_manager_v1(wl_display *display, QObject *parent)
    : QObject(parent)
    , global(wl_global_create(display,
                              &treeland_capture_manager_v1_interface,
                              1,
                              this,
                              treeland_capture_manager_bind))
{
}

void treeland_capture_manager_v1::addClientResource(wl_client *client, wl_resource *resource)
{
    WClient *wClient = WClient::get(client);
    connect(wClient, &WClient::destroyed, this, [this, wClient]() {
        for (const auto &pair : clientResources) {
            if (pair.first == wClient) {
                wl_resource_destroy(pair.second);
                clientResources.removeOne(pair);
            }
        }
    });
    clientResources.push_back({ wClient, resource });
}

void handle_treeland_capture_session_v1_destroy(wl_client *client, wl_resource *resource)
{
    wl_resource_destroy(resource);
}

void handle_treeland_capture_session_v1_start([[maybe_unused]] wl_client *client,
                                              wl_resource *resource)
{
    struct treeland_capture_session_v1 *session = capture_session_from_resource(resource);
    Q_ASSERT(session);
    Q_EMIT session->start();
}

void handle_treeland_capture_manager_v1_get_context(wl_client *client,
                                                    wl_resource *resource,
                                                    uint32_t context)
{
    struct treeland_capture_manager_v1 *manager = capture_manager_from_resource(resource);
    Q_ASSERT(manager);
    auto *capture_context = new treeland_capture_context_v1;

    int version = wl_resource_get_version(resource);

    struct wl_resource *context_resource =
        wl_resource_create(client, &treeland_capture_context_v1_interface, version, context);

    if (!context_resource) {
        wl_client_post_no_memory(client);
        delete capture_context;
        return;
    }
    wl_resource_set_implementation(context_resource,
                                   &context_impl,
                                   capture_context,
                                   capture_context_resource_destroy);

    capture_context->setResource(client, context_resource);
    Q_EMIT manager->newCaptureContext(capture_context);
}

void handle_treeland_capture_context_v1_create_session(wl_client *client,
                                                       wl_resource *resource,
                                                       uint32_t session)
{
    struct treeland_capture_context_v1 *context = capture_context_from_resource(resource);
    Q_ASSERT(context);

    auto *capture_session = new treeland_capture_session_v1;

    int version = wl_resource_get_version(resource);

    struct wl_resource *capture_session_resource =
        wl_resource_create(client, &treeland_capture_session_v1_interface, version, session);
    if (!capture_session_resource) {
        wl_client_post_no_memory(client);
        delete capture_session;
        return;
    }
    capture_session->setResource(client, capture_session_resource);
    wl_resource_set_implementation(capture_session_resource,
                                   &session_impl,
                                   capture_session,
                                   capture_session_resource_destroy);

    Q_EMIT context->newSession(capture_session);
}

void handle_treeland_capture_context_v1_select_source(wl_client *client,
                                                      wl_resource *resource,
                                                      uint32_t source_hint,
                                                      uint32_t freeze,
                                                      uint32_t with_cursor,
                                                      wl_resource *mask)
{
    struct treeland_capture_context_v1 *context = capture_context_from_resource(resource);
    Q_ASSERT(context);
    if (source_hint) {
        context->sourceHint = source_hint;
    } else {
        context->sourceHint = 0x7; // Contains all source type
    }
    context->freeze = freeze;
    context->withCursor = with_cursor;
    if (mask) {
        context->mask = WSurface::fromHandle(wlr_surface_from_resource(mask));
        Q_ASSERT(context->mask);
    }
    Q_EMIT context->selectSource();
}

void handle_treeland_capture_context_v1_capture(wl_client *client,
                                                wl_resource *resource,
                                                uint32_t frame)
{
    treeland_capture_context_v1 *context = capture_context_from_resource(resource);
    Q_ASSERT(context);
    auto *capture_frame = new treeland_capture_frame_v1;

    int version = wl_resource_get_version(resource);
    struct wl_resource *capture_frame_resource =
        wl_resource_create(client, &treeland_capture_frame_v1_interface, version, frame);

    if (!capture_frame_resource) {
        wl_client_post_no_memory(client);
        delete capture_frame;
        return;
    }

    capture_frame->setResource(client, capture_frame_resource);

    wl_resource_set_implementation(capture_frame_resource,
                                   &frame_impl,
                                   capture_frame,
                                   capture_frame_resource_destroy);

    Q_EMIT context->capture(capture_frame);
}

void treeland_capture_context_v1::sendSourceFailed(uint32_t reason)
{
    Q_ASSERT(resource);
    treeland_capture_context_v1_send_source_failed(resource, reason);
}

void treeland_capture_context_v1::sendSourceReady(QRect region, uint32_t source_type)
{
    Q_ASSERT(resource);
    treeland_capture_context_v1_send_source_ready(resource,
                                                  region.x(),
                                                  region.y(),
                                                  region.width(),
                                                  region.height(),
                                                  source_type);
}

void treeland_capture_context_v1::setResource(wl_client *client, wl_resource *resource)
{
    WClient *wClient = WClient::get(client);
    connect(wClient, &WClient::destroyed, this, [this] {
        wl_resource_destroy(this->resource);
    });
    this->resource = resource;
}

void handle_treeland_capture_frame_v1_destroy([[maybe_unused]] wl_client *client,
                                              wl_resource *resource)
{
    wl_resource_destroy(resource);
}

void handle_treeland_capture_frame_v1_copy(wl_client *client,
                                           wl_resource *resource,
                                           wl_resource *buffer)
{
    treeland_capture_frame_v1 *frame = capture_frame_from_resource(resource);
    Q_ASSERT(frame);
    qw_buffer *qwBuffer = qw_buffer::try_from_resource(buffer);
    if (!qwBuffer) {
        wl_client_post_implementation_error(client, "Buffer not created!");
        return;
    }
    Q_EMIT frame->copy(qwBuffer);
}

void treeland_capture_session_v1::setResource(wl_client *client, wl_resource *resource)
{
    WClient *wClient = WClient::get(client);
    connect(wClient, &WClient::destroyed, this, [this] {
        wl_resource_destroy(this->resource);
    });
    this->resource = resource;
}

void treeland_capture_frame_v1::setResource(wl_client *client, wl_resource *resource)
{
    WClient *wClient = WClient::get(client);
    connect(wClient, &WClient::destroyed, this, [this] {
        wl_resource_destroy(this->resource);
    });
    this->resource = resource;
}

void treeland_capture_frame_v1::sendBuffer(uint32_t format,
                                           uint32_t width,
                                           uint32_t height,
                                           uint32_t stride)
{
    treeland_capture_frame_v1_send_buffer(resource, format, width, height, stride);
}

void treeland_capture_frame_v1::sendBufferDone()
{
    treeland_capture_frame_v1_send_buffer_done(resource);
}

void treeland_capture_frame_v1::sendReady()
{
    treeland_capture_frame_v1_send_ready(resource);
}

void treeland_capture_frame_v1::sendFailed()
{
    treeland_capture_frame_v1_send_failed(resource);
}
