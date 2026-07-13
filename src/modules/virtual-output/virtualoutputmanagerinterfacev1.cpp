// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "virtualoutputmanagerinterfacev1.h"
#include "qwayland-server-treeland-virtual-output-manager-v1.h"

#include <wserver.h>

#include <qwdisplay.h>

#include <QHash>
#include <QPointer>
#include <QScopeGuard>

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

static bool stringListToWlArray(const QStringList &list, wl_array *array)
{
    for (const QString &str : list) {
        QByteArray data = str.toUtf8();
        char *target = static_cast<char *>(wl_array_add(array, data.size() + 1));
        if (!target) [[unlikely]]
            return false;
        memcpy(target, data.constData(), static_cast<size_t>(data.size()));
        target[data.size()] = '\0';
    }
    return true;
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
    : QtWaylandServer::treeland_virtual_output_v1(resource)
    , q(_q)
    , name(_name)
{
    wlarrayToStringList(_outputs, outputList);
}

void VirtualOutputInterfaceV1Private::destroy_resource([[maybe_unused]] Resource *resource)
{
    delete q;
}

void VirtualOutputInterfaceV1Private::destroy(Resource *resource)
{
    Q_EMIT q->beforeDestroy(q);
    wl_resource_destroy(resource->handle);
}

class VirtualOutputManagerInterfaceV1Private : public QtWaylandServer::treeland_virtual_output_manager_v1
{
public:
    VirtualOutputManagerInterfaceV1Private(VirtualOutputManagerInterfaceV1 *_q);
    wl_global *global() const;

    struct VirtualOutputConfig {
        QStringList outputs;
        QPointer<VirtualOutputInterfaceV1> virtualOutput;
    };
    QHash<QString, VirtualOutputConfig> m_configs;

    VirtualOutputManagerInterfaceV1 *q;

    VirtualOutputInterfaceV1 *createVirtualOutputInternal(Resource *resource,
                                                          uint32_t id,
                                                          const QString &name,
                                                          wl_array *outputs);

    void attachDestroyCleanup(const QString &name,
                              VirtualOutputInterfaceV1 *virtualOutput);
    void storeVirtualOutput(const QString &name,
                            const QStringList &outputs,
                            VirtualOutputInterfaceV1 *virtualOutput = nullptr);

protected:
    // TODO(YaoBing Xiao): treeland-virtual-output-manager-v1 is missing the 'destroy' request.
    // void destroy(Resource *resource) override;
    void create_virtual_output(Resource *resource, uint32_t id, const QString &name, wl_array *outputs) override;
    void get_virtual_output_list(Resource *resource) override;
    void get_virtual_output(Resource *resource, const QString &name, uint32_t id) override;
};

VirtualOutputManagerInterfaceV1Private::VirtualOutputManagerInterfaceV1Private(VirtualOutputManagerInterfaceV1 *_q)
    : QtWaylandServer::treeland_virtual_output_manager_v1()
    , q(_q)
{
}

wl_global *VirtualOutputManagerInterfaceV1Private::global() const
{
    return m_global;
}

VirtualOutputInterfaceV1 *VirtualOutputManagerInterfaceV1Private::createVirtualOutputInternal(Resource *resource,
                                                                                               uint32_t id,
                                                                                               const QString &name,
                                                                                               wl_array *outputs)
{
    wl_resource *outputResource = wl_resource_create(resource->client(),
                                                     &treeland_virtual_output_v1_interface,
                                                     resource->version(),
                                                     id);
    if (!outputResource) {
        wl_client_post_no_memory(resource->client());
        return nullptr;
    }

    auto virtualOutput = new VirtualOutputInterfaceV1(name, outputs, outputResource);
    attachDestroyCleanup(name, virtualOutput);
    return virtualOutput;
}

void VirtualOutputManagerInterfaceV1Private::attachDestroyCleanup(
    const QString &name,
    VirtualOutputInterfaceV1 *virtualOutput)
{
    QObject::connect(virtualOutput, &VirtualOutputInterfaceV1::beforeDestroy,
                     q, &VirtualOutputManagerInterfaceV1::destroyVirtualOutput);

    QObject::connect(virtualOutput, &VirtualOutputInterfaceV1::beforeDestroy,
                     q, [this, name]() {
        m_configs.remove(name);
    });
}

void VirtualOutputManagerInterfaceV1Private::storeVirtualOutput(const QString &name,
                                                                const QStringList &outputs,
                                                                VirtualOutputInterfaceV1 *virtualOutput)
{
    if (name.isEmpty() || outputs.size() < 2) {
        return;
    }

    m_configs.insert(name, { outputs, virtualOutput });
    if (virtualOutput) {
        const QByteArray arrSend = outputs.join('\0').toLatin1();
        virtualOutput->sendOutputs(name, arrSend);
    }
}

// void VirtualOutputManagerInterfaceV1Private::destroy(Resource *resource)
// {
//     wl_resource_destroy(resource->handle);
// }

void VirtualOutputManagerInterfaceV1Private::create_virtual_output(Resource *resource,
                                                                   uint32_t id,
                                                                   const QString &name,
                                                                   wl_array *outputs)
{
    if (!outputs) {
        wl_resource_post_error(resource->handle, 0, "outputs array is NULL!");
        return;
    }

    auto virtualOutput = createVirtualOutputInternal(resource, id, name, outputs);
    if (!virtualOutput)
        return;

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

    // Store persistent config (survives client disconnection)
    QStringList outputList;
    wlarrayToStringList(outputs, outputList);

    // Enforce unique names — name is the key for config lookup/removal
    if (m_configs.contains(name)) {
        virtualOutput->sendError(TREELAND_VIRTUAL_OUTPUT_V1_ERROR_INVALID_GROUP_NAME,
                                 "A virtual output with this name already exists!");
        return;
    }

    storeVirtualOutput(name, outputList, virtualOutput);

    Q_EMIT q->requestCreateVirtualOutput(virtualOutput);
}

void VirtualOutputManagerInterfaceV1Private::get_virtual_output_list(Resource *resource)
{
    const QByteArray arr = m_configs.keys().join('\0').toLatin1();
    send_virtual_output_list(resource->handle, arr);
}

void VirtualOutputManagerInterfaceV1Private::get_virtual_output(Resource *resource,
                                                                 const QString &name,
                                                                 uint32_t id)
{
    auto it = m_configs.find(name);
    if (it == m_configs.end()) {
        wl_resource_post_error(resource->handle, 0,
            "Virtual output '%s' not found!", name.toUtf8().constData());
        return;
    }

    auto &config = it.value();
    wl_array arr;
    wl_array_init(&arr);
    auto cleanup = qScopeGuard([&arr] { wl_array_release(&arr); });

    if (!stringListToWlArray(config.outputs, &arr)) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto virtualOutput = createVirtualOutputInternal(resource, id, name, &arr);
    if (!virtualOutput)
        return;

    // Send the outputs event to the requesting client
    const QByteArray arrSend = config.outputs.join('\0').toLatin1();
    virtualOutput->sendOutputs(name, arrSend);
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

void VirtualOutputManagerInterfaceV1::restoreVirtualOutput(const QString &name,
                                                           const QStringList &outputs)
{
    d->storeVirtualOutput(name, outputs);
}

void VirtualOutputManagerInterfaceV1::updateVirtualOutput(const QString &name,
                                                          const QStringList &outputs)
{
    auto it = d->m_configs.find(name);
    if (it == d->m_configs.end()) {
        d->storeVirtualOutput(name, outputs);
        return;
    }

    it->outputs = outputs;
    if (it->virtualOutput) {
        const QByteArray arrSend = outputs.join('\0').toLatin1();
        it->virtualOutput->sendOutputs(name, arrSend);
    }
}

void VirtualOutputManagerInterfaceV1::removeVirtualOutput(const QString &name)
{
    d->m_configs.remove(name);
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

QString VirtualOutputInterfaceV1::name() const
{
    return d->name;
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
