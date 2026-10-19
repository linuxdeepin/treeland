// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "outputuuidmanagerinterfacev1.h"

#include "core/waylandserver.h"
#include "output.h"
#include "qwayland-server-treeland-output-uuid-unstable-v1.h"
#include "qwayland-server-treeland-output-uuid-head-unstable-v1.h"

#include <woutput.h>
#include <wlr_all.h>

WAYLIB_SERVER_USE_NAMESPACE

class OutputUuidV1 : public QtWaylandServer::treeland_output_uuid_v1
{
public:
    OutputUuidV1(wl_resource *resource, const QString &uuid)
        : QtWaylandServer::treeland_output_uuid_v1(resource)
    {
        send_uuid(resource, uuid);
    }

protected:
    void destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void destroy_resource(Resource *resource) override
    {
        Q_UNUSED(resource);
        delete this;
    }
};

class OutputUuidManagerV1Private
    : public QObject
    , public QtWaylandServer::treeland_output_uuid_manager_v1
{
    Q_OBJECT
public:
    explicit OutputUuidManagerV1Private(QObject *parent = nullptr)
        : QObject(parent)
        , QtWaylandServer::treeland_output_uuid_manager_v1()
    {
    }

    wl_global *globalHandle() const
    {
        return m_global;
    }

protected:
    void destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void get_uuid(Resource *resource, uint32_t id, struct ::wl_resource *output) override
    {
        auto *wlr_o = wlr_output_from_resource(output);
        if (!wlr_o) {
            wl_resource_post_error(resource->handle,
                                   WL_DISPLAY_ERROR_INVALID_OBJECT,
                                   "Invalid wl_output resource");
            return;
        }

        auto *o = WaylandServer::instance()->getOutput(WOutput::fromHandle(wlr_o));
        if (!o) {
            wl_resource_post_error(resource->handle,
                                   WL_DISPLAY_ERROR_INVALID_OBJECT,
                                   "Output not found");
            return;
        }

        auto *objRes = wl_resource_create(resource->client(),
                                          QtWaylandServer::treeland_output_uuid_v1::interface(),
                                          resource->version(),
                                          id);
        if (!objRes) {
            wl_resource_post_no_memory(resource->handle);
            return;
        }

        new OutputUuidV1(objRes, o->uuid()); // sends uuid during construction
    }
};

class OutputUuidHeadManagerV1Private
    : public QObject
    , public QtWaylandServer::treeland_output_uuid_head_manager_v1
{
    Q_OBJECT
public:
    explicit OutputUuidHeadManagerV1Private(QObject *parent = nullptr)
        : QObject(parent)
        , QtWaylandServer::treeland_output_uuid_head_manager_v1()
    {
    }

    wl_global *globalHandle() const
    {
        return m_global;
    }

protected:
    void destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void get_uuid(Resource *resource, uint32_t id, struct ::wl_resource *head) override
    {
        // wlroots stores the owning wlr_output_head_v1 as the head resource user data;
        // a destroyed/inert head has it reset to NULL, caught by the !headObj check.
        auto *headObj = static_cast<wlr_output_head_v1 *>(wl_resource_get_user_data(head));
        if (!headObj || !headObj->state.output) {
            wl_resource_post_error(resource->handle,
                                   WL_DISPLAY_ERROR_INVALID_OBJECT,
                                   "Invalid wlr output head resource");
            return;
        }

        auto *o = WaylandServer::instance()->getOutput(
            WOutput::fromHandle(headObj->state.output));
        if (!o) {
            wl_resource_post_error(resource->handle,
                                   WL_DISPLAY_ERROR_INVALID_OBJECT,
                                   "Output not found");
            return;
        }

        auto *objRes = wl_resource_create(resource->client(),
                                          QtWaylandServer::treeland_output_uuid_v1::interface(),
                                          resource->version(),
                                          id);
        if (!objRes) {
            wl_resource_post_no_memory(resource->handle);
            return;
        }

        new OutputUuidV1(objRes, o->uuid()); // sends uuid during construction
    }
};

OutputUuidManagerV1::OutputUuidManagerV1(QObject *parent)
    : QObject(parent)
    , WServerInterface()
    , d(std::make_unique<OutputUuidManagerV1Private>())
{
}

OutputUuidManagerV1::~OutputUuidManagerV1() = default;

QByteArrayView OutputUuidManagerV1::interfaceName() const
{
    return QtWaylandServer::treeland_output_uuid_manager_v1::interfaceName();
}

void OutputUuidManagerV1::create(WServer *server)
{
    d->init(server->handle(), InterfaceVersion);
}

void OutputUuidManagerV1::destroy([[maybe_unused]] WServer *server)
{
    d->globalRemove();
}

wl_global *OutputUuidManagerV1::global() const
{
    return d->globalHandle();
}

OutputUuidHeadManagerV1::OutputUuidHeadManagerV1(QObject *parent)
    : QObject(parent)
    , WServerInterface()
    , d(std::make_unique<OutputUuidHeadManagerV1Private>())
{
}

OutputUuidHeadManagerV1::~OutputUuidHeadManagerV1() = default;

QByteArrayView OutputUuidHeadManagerV1::interfaceName() const
{
    return QtWaylandServer::treeland_output_uuid_head_manager_v1::interfaceName();
}

void OutputUuidHeadManagerV1::create(WServer *server)
{
    d->init(server->handle(), InterfaceVersion);
}

void OutputUuidHeadManagerV1::destroy([[maybe_unused]] WServer *server)
{
    d->globalRemove();
}

wl_global *OutputUuidHeadManagerV1::global() const
{
    return d->globalHandle();
}

#include "outputuuidmanagerinterfacev1.moc"
