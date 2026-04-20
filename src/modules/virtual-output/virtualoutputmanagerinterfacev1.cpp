// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "virtualoutputmanagerinterfacev1.h"
#include "qwayland-server-treeland-virtual-output-manager-v1.h"

#include <wserver.h>

#include <qwdisplay.h>

static QList<VirtualOutputInterfaceV1 *> s_virtualOutputs;

static void wlarrayToStringList(const wl_array *wl_array, QStringList &stringList)
{
    char *dataStart = static_cast<char *>(wl_array->data);
    char *currentPos = dataStart;

    while (*currentPos != '\0') {
        QString str = QString::fromUtf8(currentPos);
        stringList << str;
        currentPos += str.toLocal8Bit().length() + 1;
    }
}

class VirtualOutputInterfaceV1Private : public QtWaylandServer::treeland_virtual_output_v1
{
public:
    VirtualOutputInterfaceV1Private(VirtualOutputInterfaceV1 *_q,
                                    const QString &_name,
                                    wl_array *_outputs,
                                    wl_resource *_resource);

    VirtualOutputInterfaceV1 *q;

    struct wl_array *screen_outputs = nullptr;
    QString name;
    QStringList outputList;

protected:
    void destroy_resource(Resource *resource) override;
    void destroy(Resource *resource) override;
};

VirtualOutputInterfaceV1Private::VirtualOutputInterfaceV1Private(VirtualOutputInterfaceV1 *_q,
                                                                 const QString &_name,
                                                                 wl_array *_outputs,
                                                                 wl_resource *resource)
    : q(_q)
    , name(_name)
{
    wlarrayToStringList(_outputs, outputList);
    init(resource);
}

void VirtualOutputInterfaceV1Private::destroy_resource([[maybe_unused]] Resource *resource)
{
    Q_EMIT q->beforeDestroy(q);
    delete q;
}

void VirtualOutputInterfaceV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

class VirtualOutputManagerInterfaceV1Private : public QtWaylandServer::treeland_virtual_output_manager_v1
{
public:
    VirtualOutputManagerInterfaceV1Private(VirtualOutputManagerInterfaceV1 *_q);
    wl_global *global() const;

    VirtualOutputManagerInterfaceV1 *q;
    QList<Resource *> m_resource;
protected:
    void destroy_global() override;
    void bind_resource(Resource *resource) override;
    void destroy_resource(Resource *resource) override;
    // TODO(YaoBing Xiao): treeland-virtual-output-manager-v1 is missing the 'destroy' request.
    // void destroy(Resource *resource) override;
    void create_virtual_output(Resource *resource, uint32_t id, const QString &name, wl_array *outputs) override;
    void get_virtual_output_list(Resource *resource) override;
    void get_virtual_output(Resource *resource, const QString &name, uint32_t id) override;
};

VirtualOutputManagerInterfaceV1Private::VirtualOutputManagerInterfaceV1Private(VirtualOutputManagerInterfaceV1 *_q)
    : q(_q)
{
}

wl_global *VirtualOutputManagerInterfaceV1Private::global() const
{
    return m_global;
}

void VirtualOutputManagerInterfaceV1Private::destroy_global()
{
    m_resource.clear();
    delete q;
}

void VirtualOutputManagerInterfaceV1Private::bind_resource(Resource *resource)
{
    m_resource.append(resource);
}

void VirtualOutputManagerInterfaceV1Private::destroy_resource(Resource *resource)
{
    m_resource.removeOne(resource);
}

// void VirtualOutputManagerInterfaceV1Private::destroy(Resource *resource)
// {
//     wl_resource_destroy(resource->handle);
// }

void VirtualOutputManagerInterfaceV1Private::create_virtual_output(Resource *resource,
                                                                   uint32_t id,
                                                                   const QString &name, wl_array *outputs)
{
    if (!outputs) {
        wl_resource_post_error(resource->handle, 0, "outputs array is NULL!");
        return;
    }

    wl_resource *outputResource = wl_resource_create(resource->client(),
                                                     &treeland_virtual_output_v1_interface,
                                                     resource->version(),
                                                     id);
    if (!outputResource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto virtualOutput = new VirtualOutputInterfaceV1(name, outputs, outputResource);
    s_virtualOutputs.append(virtualOutput);
    QObject::connect(virtualOutput, &VirtualOutputInterfaceV1::beforeDestroy,
                     q, &VirtualOutputManagerInterfaceV1::destroyVirtualOutput);
    QObject::connect(virtualOutput, &QObject::destroyed, [virtualOutput, this]() {
        s_virtualOutputs.removeOne(virtualOutput);
    });

    if (name.isEmpty()) {
        virtualOutput->sendError(TREELAND_VIRTUAL_OUTPUT_V1_ERROR_INVALID_GROUP_NAME,
                                 "Group name is empty!");
        return;
    }

    if (outputs->size < 2) {
        virtualOutput->sendError(TREELAND_VIRTUAL_OUTPUT_V1_ERROR_INVALID_SCREEN_NUMBER,
                                 "The number of screens applying for copy mode is less than 2!");
        return;
    }

    Q_EMIT q->requestCreateVirtualOutput(virtualOutput);
}

void VirtualOutputManagerInterfaceV1Private::get_virtual_output_list(Resource *resource)
{
    QByteArray arr;
    for (auto interface : std::as_const(s_virtualOutputs)) {
        arr.append(interface->d->name.toLatin1());
    }

    send_virtual_output_list(resource->handle, arr);
}

void VirtualOutputManagerInterfaceV1Private::get_virtual_output([[maybe_unused]] Resource *resource,
                                                                const QString &name,
                                                                [[maybe_unused]] uint32_t id)
{
    for (auto interface : std::as_const(s_virtualOutputs)) {
        if (interface->d->name == name) {
            QByteArray arr;
            for (auto str : interface->outputList()) {
                arr.append(str.toLatin1());
            }
            interface->sendOutputs(name, arr);
        }
    }
}

VirtualOutputManagerInterfaceV1::VirtualOutputManagerInterfaceV1(QObject *parent)
    : QObject(parent)
    , d(new VirtualOutputManagerInterfaceV1Private(this))
{
}

VirtualOutputManagerInterfaceV1::~VirtualOutputManagerInterfaceV1() = default;


void VirtualOutputManagerInterfaceV1::create(WServer *server)
{
    d->init(server->handle()->handle(), InterfaceVersion);
}

void VirtualOutputManagerInterfaceV1::destroy([[maybe_unused]] WServer *server) {
    d->globalRemove();
}

wl_global *VirtualOutputManagerInterfaceV1::global() const
{
    return d->global();
}

QByteArrayView VirtualOutputManagerInterfaceV1::interfaceName() const
{
    return d->interfaceName();
}

VirtualOutputInterfaceV1::~VirtualOutputInterfaceV1() = default;

VirtualOutputInterfaceV1::VirtualOutputInterfaceV1(const QString &name, wl_array *outputs, wl_resource *resource)
    : d(new VirtualOutputInterfaceV1Private(this, name, outputs, resource))
{
}

wl_resource *VirtualOutputInterfaceV1::resource() const
{
    return d->resource()->handle;
}

QStringList VirtualOutputInterfaceV1::outputList() const
{
    return d->outputList;
}

void VirtualOutputInterfaceV1::sendOutputs(const QString &name, const QByteArray &outputs)
{
    d->send_outputs(name, outputs);
}

void VirtualOutputInterfaceV1::sendError(uint32_t code, const QString &message)
{
    d->send_error(code, message);
}

VirtualOutputInterfaceV1 *VirtualOutputInterfaceV1::get(wl_resource *resource)
{
    for (auto interface : s_virtualOutputs) {
        if (interface->resource() == resource) {
            return interface;
        }
    }

    return nullptr;
}
