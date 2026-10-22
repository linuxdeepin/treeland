// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "outputgroupmanagerinterfacev1.h"

#include "common/treelandlogging.h"
#include "dconfig/dconfigmanager.h"
#include "treelandconfig.hpp"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QScopeGuard>
#include <utility>
#include <QSet>

#include <qwayland-server-treeland-output-group-manager-unstable-v1.h>

WAYLIB_SERVER_USE_NAMESPACE

namespace {

QString serializeStringList(const QStringList &values)
{
    QJsonArray array;
    for (const auto &value : std::as_const(values)) {
        array.append(value);
    }
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

QStringList deserializeStringList(const QString &value)
{
    const auto document = QJsonDocument::fromJson(value.toUtf8());
    if (!document.isArray()) {
        return {};
    }

    QStringList values;
    const auto array = document.array();
    for (const auto &entry : std::as_const(array)) {
        if (entry.isString()) {
            values.append(entry.toString());
        }
    }
    return values;
}

// The state event carries the mirror uuids as one flat byte array: every uuid
// has a fixed length and never contains NUL, so no separators are needed.
QByteArray concatenateUuids(const QStringList &uuids)
{
    QByteArray result;
    for (const auto &uuid : std::as_const(uuids)) {
        result.append(uuid.toUtf8());
    }
    return result;
}

} // namespace

class OutputGroupInterfaceV1Private;

class OutputGroupManagerInterfaceV1Private
    : public QtWaylandServer::treeland_output_group_manager_v1
{
public:
    explicit OutputGroupManagerInterfaceV1Private(OutputGroupManagerInterfaceV1 *q);

    wl_global *globalHandle() const { return m_global; }

    struct PendingRequest
    {
        enum Type
        {
            SetSource,
            AddOutput,
            RemoveOutput,
        };
        Type type;
        QString uuid;
    };

    OutputGroupManagerInterfaceV1 *q;
    OutputGroupHost *host = nullptr;

    QList<OutputGroupConfig> configs;
    QHash<QString, OutputGroupRuntime> effective;
    QHash<QString, QList<OutputGroupInterfaceV1Private *>> handles;
    int nextGroupNumber = 1;
    bool configLoaded = false;
    bool recomputing = false;

    OutputGroupConfig *findConfig(const QString &name);
    const OutputGroupConfig *findConfig(const QString &name) const;
    QString nextGroupName();
    QString groupOwningOutput(const QString &uuid, const QString &exceptGroup = {}) const;

    void loadPersistedGroupsIfReady();
    void loadPersistedGroups();
    void restorePersistedGroup(const QString &name, TreelandConfig *groupConfig);
    void persistGroup(const OutputGroupConfig &config);
    void forgetPersistedGroup(const QString &name);
    void persistGroupRegistry();

    OutputGroupInterfaceV1Private *createHandle(Resource *managerResource,
                                               uint32_t id,
                                               const QString &name,
                                               bool valid);

    void sendInitialState(OutputGroupInterfaceV1Private *handle);
    void recomputeEffectiveState();
    void broadcastGroupAdded(const QString &name, wl_resource *skipClient);
    void broadcastGroupRemoved(const QString &name);
    void dissolveGroup(const QString &name);
    void commitGroup(OutputGroupInterfaceV1Private *handle);
    OutputGroupConfig *writableConfig(OutputGroupInterfaceV1Private *handle);

protected:
    void bind_resource(Resource *resource) override;
    void destroy(Resource *resource) override;
    void create_group(Resource *resource, uint32_t id, const QString &source) override;
    void get_group(Resource *resource, uint32_t id, const QString &name) override;
};

class OutputGroupInterfaceV1Private : public QtWaylandServer::treeland_output_group_v1
{
public:
    OutputGroupInterfaceV1Private(OutputGroupManagerInterfaceV1Private *_manager,
                                  wl_resource *resource,
                                  const QString &_name,
                                  bool _valid);

    void sendName();
    void sendState(const OutputGroupRuntime &state);
    void sendFailure(uint32_t code, const QString &uuid);
    void sendDone();
    void sendRemoved();
    void postAlreadyDissolved();

    OutputGroupManagerInterfaceV1Private *manager;
    QString name;
    bool valid = true;
    bool dissolved = false;
    bool nameSent = false;
    bool doneSent = false;
    bool hasState = false;
    QString lastSource;
    QStringList lastMirrors;
    QList<OutputGroupManagerInterfaceV1Private::PendingRequest> pending;

protected:
    void destroy(Resource *resource) override;
    void destroy_resource(Resource *resource) override;
    void set_source(Resource *resource, const QString &uuid) override;
    void add_output(Resource *resource, const QString &uuid) override;
    void remove_output(Resource *resource, const QString &uuid) override;
    void commit(Resource *resource) override;
    void dissolve(Resource *resource) override;
};

OutputGroupInterfaceV1Private::OutputGroupInterfaceV1Private(OutputGroupManagerInterfaceV1Private *_manager,
                                                             wl_resource *resource,
                                                             const QString &_name,
                                                             bool _valid)
    : QtWaylandServer::treeland_output_group_v1(resource)
    , manager(_manager)
    , name(_name)
    , valid(_valid)
{
}

void OutputGroupInterfaceV1Private::sendName()
{
    if (!valid || nameSent || dissolved) {
        return;
    }
    nameSent = true;
    send_name(resource()->handle, name);
}

void OutputGroupInterfaceV1Private::sendState(const OutputGroupRuntime &state)
{
    if (!valid || dissolved) {
        return;
    }
    if (hasState && lastSource == state.sourceUuid && lastMirrors == state.mirrorUuids) {
        return;
    }
    hasState = true;
    lastSource = state.sourceUuid;
    lastMirrors = state.mirrorUuids;

    send_state(resource()->handle,
               state.sourceUuid,
               static_cast<uint32_t>(state.mirrorUuids.size()),
               concatenateUuids(state.mirrorUuids));
}

void OutputGroupInterfaceV1Private::sendFailure(uint32_t code, const QString &uuid)
{
    if (dissolved) {
        return;
    }
    send_operation_failed(resource()->handle, code, uuid);
}

void OutputGroupInterfaceV1Private::sendDone()
{
    if (dissolved || doneSent) {
        return;
    }
    doneSent = true;
    send_done(resource()->handle);
}

void OutputGroupInterfaceV1Private::sendRemoved()
{
    if (dissolved) {
        return;
    }
    dissolved = true;
    pending.clear();
    send_removed(resource()->handle);
}

void OutputGroupInterfaceV1Private::postAlreadyDissolved()
{
    wl_resource_post_error(resource()->handle,
                           QtWaylandServer::treeland_output_group_v1::error_already_dissolved,
                           "The output group has been dissolved");
}

void OutputGroupInterfaceV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void OutputGroupInterfaceV1Private::destroy_resource([[maybe_unused]] Resource *resource)
{
    if (auto it = manager->handles.find(name); it != manager->handles.end()) {
        it->removeOne(this);
        if (it->isEmpty()) {
            manager->handles.erase(it);
        }
    }
    delete this;
}

void OutputGroupInterfaceV1Private::set_source([[maybe_unused]] Resource *resource, const QString &uuid)
{
    if (dissolved) {
        postAlreadyDissolved();
        return;
    }
    if (!valid) {
        sendFailure(QtWaylandServer::treeland_output_group_v1::failure_invalid_name, QString());
        return;
    }
    pending.append({ OutputGroupManagerInterfaceV1Private::PendingRequest::SetSource, uuid });
}

void OutputGroupInterfaceV1Private::add_output([[maybe_unused]] Resource *resource, const QString &uuid)
{
    if (dissolved) {
        postAlreadyDissolved();
        return;
    }
    if (!valid) {
        sendFailure(QtWaylandServer::treeland_output_group_v1::failure_invalid_name, QString());
        return;
    }
    pending.append({ OutputGroupManagerInterfaceV1Private::PendingRequest::AddOutput, uuid });
}

void OutputGroupInterfaceV1Private::remove_output([[maybe_unused]] Resource *resource, const QString &uuid)
{
    if (dissolved) {
        postAlreadyDissolved();
        return;
    }
    if (!valid) {
        sendFailure(QtWaylandServer::treeland_output_group_v1::failure_invalid_name, QString());
        return;
    }
    pending.append({ OutputGroupManagerInterfaceV1Private::PendingRequest::RemoveOutput, uuid });
}

void OutputGroupInterfaceV1Private::commit([[maybe_unused]] Resource *resource)
{
    if (dissolved) {
        postAlreadyDissolved();
        return;
    }
    if (!valid) {
        sendFailure(QtWaylandServer::treeland_output_group_v1::failure_invalid_name, QString());
        return;
    }
    manager->commitGroup(this);
}

void OutputGroupInterfaceV1Private::dissolve([[maybe_unused]] Resource *resource)
{
    if (dissolved) {
        postAlreadyDissolved();
        return;
    }
    if (!valid) {
        sendFailure(QtWaylandServer::treeland_output_group_v1::failure_invalid_name, QString());
        return;
    }
    manager->dissolveGroup(name);
}

OutputGroupManagerInterfaceV1Private::OutputGroupManagerInterfaceV1Private(
    OutputGroupManagerInterfaceV1 *_q)
    : QtWaylandServer::treeland_output_group_manager_v1()
    , q(_q)
{
}

OutputGroupConfig *OutputGroupManagerInterfaceV1Private::findConfig(const QString &name)
{
    for (auto &config : configs) {
        if (config.name == name) {
            return &config;
        }
    }
    return nullptr;
}

const OutputGroupConfig *OutputGroupManagerInterfaceV1Private::findConfig(const QString &name) const
{
    for (const auto &config : configs) {
        if (config.name == name) {
            return &config;
        }
    }
    return nullptr;
}

QString OutputGroupManagerInterfaceV1Private::nextGroupName()
{
    auto taken = [this](const QString &name) {
        return findConfig(name) || handles.contains(name);
    };

    QString name;
    do {
        name = QStringLiteral("output-group-%1").arg(nextGroupNumber++);
    } while (taken(name));
    return name;
}

QString OutputGroupManagerInterfaceV1Private::groupOwningOutput(const QString &uuid,
                                                               const QString &exceptGroup) const
{
    if (uuid.isEmpty()) {
        return {};
    }

    for (const auto &config : configs) {
        if (config.name == exceptGroup) {
            continue;
        }
        if (config.sourceUuid == uuid || config.mirrorUuids.contains(uuid)) {
            return config.name;
        }
    }
    return {};
}

void OutputGroupManagerInterfaceV1Private::loadPersistedGroupsIfReady()
{
    if (configLoaded) {
        return;
    }

    auto *manager = DConfigManager::instance();
    if (!manager || !manager->globalConfig()) {
        return;
    }

    auto *global = manager->globalConfig();
    if (global->isInitializeSucceeded()) {
        loadPersistedGroups();
        return;
    }
    if (global->isInitializeFailed()) {
        configLoaded = true;
        return;
    }

    QObject::connect(
        global,
        &TreelandConfig::configInitializeSucceed,
        q,
        [this](auto *) { loadPersistedGroups(); },
        Qt::SingleShotConnection);
    QObject::connect(
        global,
        &TreelandConfig::configInitializeFailed,
        q,
        [this]() { configLoaded = true; },
        Qt::SingleShotConnection);
}

void OutputGroupManagerInterfaceV1Private::restorePersistedGroup(const QString &name,
                                                                 TreelandConfig *groupConfig)
{
    if (!groupConfig || findConfig(name)) {
        return;
    }

    OutputGroupConfig config;
    config.name = name;
    config.sourceUuid = groupConfig->outputGroupSource();
    config.mirrorUuids = deserializeStringList(groupConfig->outputGroupMirrors());
    configs.append(config);
    qCInfo(lcTlOutput) << "Restored output group" << name
                       << "source:" << config.sourceUuid
                       << "mirrors:" << config.mirrorUuids;
    // Clients that bound before the configuration was readable already got
    // their groups_done dump; announce the restored groups to them as well
    // instead of leaving them permanently invisible.
    broadcastGroupAdded(config.name, nullptr);
    recomputeEffectiveState();
}

void OutputGroupManagerInterfaceV1Private::loadPersistedGroups()
{
    if (configLoaded) {
        return;
    }
    configLoaded = true;

    auto *manager = DConfigManager::instance();
    if (!manager || !manager->globalConfig()) {
        return;
    }

    const auto names = deserializeStringList(manager->globalConfig()->outputGroups());
    for (const auto &name : names) {
        if (name.isEmpty()) {
            continue;
        }
        if (findConfig(name)) {
            // A group was created before the persisted configuration became
            // readable and took the same name; the in-memory one wins.
            qCWarning(lcTlOutput) << "Not restoring persisted output group" << name
                                  << "because a group with that name already exists";
            continue;
        }

        auto *groupConfig = manager->outputGroupConfig(name);
        if (!groupConfig) {
            continue;
        }

        if (groupConfig->isInitializeSucceeded()) {
            restorePersistedGroup(name, groupConfig);
            continue;
        }

        // The per-group config is created on demand and DConfig initializes
        // asynchronously, so its values are only readable once it is ready.
        QObject::connect(
            groupConfig,
            &TreelandConfig::configInitializeSucceed,
            q,
            [this, name](auto *) {
                restorePersistedGroup(name, DConfigManager::instance()->outputGroupConfig(name));
            },
            Qt::SingleShotConnection);
        QObject::connect(
            groupConfig,
            &TreelandConfig::configInitializeFailed,
            q,
            [name] {
                qCWarning(lcTlOutput) << "Failed to read persisted output group" << name;
            },
            Qt::SingleShotConnection);
    }

    recomputeEffectiveState();
}

void OutputGroupManagerInterfaceV1Private::persistGroup(const OutputGroupConfig &config)
{
    auto *manager = DConfigManager::instance();
    if (!manager) {
        return;
    }
    auto *groupConfig = manager->outputGroupConfig(config.name);
    if (!groupConfig) {
        return;
    }
    groupConfig->setOutputGroupSource(config.sourceUuid);
    groupConfig->setOutputGroupMirrors(serializeStringList(config.mirrorUuids));
}

void OutputGroupManagerInterfaceV1Private::forgetPersistedGroup(const QString &name)
{
    auto *manager = DConfigManager::instance();
    if (!manager) {
        return;
    }
    if (auto *groupConfig = manager->outputGroupConfig(name)) {
        groupConfig->setOutputGroupSource(QString());
        groupConfig->setOutputGroupMirrors(QStringLiteral("[]"));
    }
}

void OutputGroupManagerInterfaceV1Private::persistGroupRegistry()
{
    auto *manager = DConfigManager::instance();
    if (!manager || !manager->globalConfig()) {
        return;
    }

    QStringList names;
    names.reserve(configs.size());
    for (const auto &config : std::as_const(configs)) {
        names.append(config.name);
    }
    manager->globalConfig()->setOutputGroups(serializeStringList(names));
}

OutputGroupInterfaceV1Private *OutputGroupManagerInterfaceV1Private::createHandle(
    Resource *managerResource, uint32_t id, const QString &name, bool valid)
{
    wl_resource *outputResource = wl_resource_create(managerResource->client(),
                                                     &treeland_output_group_v1_interface,
                                                     managerResource->version(),
                                                     id);
    if (!outputResource) {
        wl_client_post_no_memory(managerResource->client());
        return nullptr;
    }

    auto *handle = new OutputGroupInterfaceV1Private(this, outputResource, name, valid);
    if (valid) {
        handles[name].append(handle);
        handle->sendName();
    }
    return handle;
}

void OutputGroupManagerInterfaceV1Private::sendInitialState(OutputGroupInterfaceV1Private *handle)
{
    if (!handle || !handle->valid) {
        return;
    }
    if (!effective.contains(handle->name)) {
        OutputGroupRuntime runtime;
        runtime.name = handle->name;
        effective.insert(handle->name, runtime);
    }
    handle->sendState(effective.value(handle->name));
}

void OutputGroupManagerInterfaceV1Private::recomputeEffectiveState()
{
    if (recomputing || !host) {
        return;
    }

    QScopeGuard resetGuard([this] { recomputing = false; });
    recomputing = true;

    const bool canPromote = host->canReconfigureOutputs();
    QStringList promotedGroups;

    for (auto &config : configs) {
        OutputGroupRuntime runtime;
        runtime.name = config.name;

        const auto isAvailable = [this](const QString &uuid) {
            return !uuid.isEmpty() && host->isOutputAvailable(uuid);
        };

        QString source = isAvailable(config.sourceUuid) ? config.sourceUuid : QString();
        QStringList mirrors;
        for (const auto &uuid : std::as_const(config.mirrorUuids)) {
            if (uuid == source || !isAvailable(uuid)) {
                continue;
            }
            mirrors.append(uuid);
        }

        if (source.isEmpty() && !mirrors.isEmpty()) {
            // Promote the first available configured mirror. Once the output
            // set is stable this is permanent: the promoted output becomes the
            // configured source so the group keeps working after a restart.
            const QString promoted = mirrors.takeFirst();
            const QString previousSource = config.sourceUuid;
            source = promoted;
            if (canPromote) {
                // Permanent promotion: the promoted output becomes the
                // configured source and the unavailable source takes its place
                // in the configured mirror list, so copy mode resumes with the
                // same members once it comes back.
                config.sourceUuid = promoted;
                config.mirrorUuids.removeAll(promoted);
                if (!previousSource.isEmpty() && previousSource != promoted
                    && !config.mirrorUuids.contains(previousSource)) {
                    config.mirrorUuids.append(previousSource);
                }
                promotedGroups.append(config.name);
            }
        }

        runtime.sourceUuid = source;
        runtime.mirrorUuids = mirrors;

        auto it = effective.find(config.name);
        if (it == effective.end()) {
            effective.insert(config.name, runtime);
        } else if (it->sourceUuid != runtime.sourceUuid
                   || it->mirrorUuids != runtime.mirrorUuids) {
            *it = runtime;
        }
    }

    if (!promotedGroups.isEmpty()) {
        for (const auto &name : std::as_const(promotedGroups)) {
            if (const auto *config = findConfig(name)) {
                persistGroup(*config);
            }
        }
    }

    // Report the effective state to every bound group object.
    for (const auto &config : std::as_const(configs)) {
        if (auto it = handles.find(config.name); it != handles.end()) {
            for (auto *handle : std::as_const(*it)) {
                handle->sendState(effective.value(config.name));
            }
        }
    }

    // Always let the host re-apply: a caller may have torn copy mode down
    // while applying an output configuration without changing the effective
    // state, and the reported state must stay consistent with what is
    // rendered. applyOutputGroups() is idempotent.
    host->applyOutputGroups();
}

void OutputGroupManagerInterfaceV1Private::broadcastGroupAdded(const QString &name,
                                                               wl_resource *skipClient)
{
    for (auto *resource : resourceMap()) {
        if (skipClient && resource->handle == skipClient) {
            continue;
        }
        send_group_added(resource->handle, name);
    }
}

void OutputGroupManagerInterfaceV1Private::broadcastGroupRemoved(const QString &name)
{
    for (auto *resource : resourceMap()) {
        send_group_removed(resource->handle, name);
    }
}

void OutputGroupManagerInterfaceV1Private::dissolveGroup(const QString &name)
{
    for (int i = 0; i < configs.size(); ++i) {
        if (configs.at(i).name == name) {
            configs.removeAt(i);
            break;
        }
    }

    if (auto it = handles.find(name); it != handles.end()) {
        const auto groupHandles = *it;
        handles.erase(it);
        for (auto *handle : groupHandles) {
            handle->sendRemoved();
        }
    }

    effective.remove(name);
    forgetPersistedGroup(name);
    persistGroupRegistry();
    broadcastGroupRemoved(name);
    recomputeEffectiveState();
}

OutputGroupConfig *OutputGroupManagerInterfaceV1Private::writableConfig(
    OutputGroupInterfaceV1Private *handle)
{
    if (!handle || !handle->valid || handle->dissolved) {
        return nullptr;
    }
    return findConfig(handle->name);
}

void OutputGroupManagerInterfaceV1Private::commitGroup(OutputGroupInterfaceV1Private *handle)
{
    auto *config = writableConfig(handle);
    if (!config) {
        handle->sendFailure(QtWaylandServer::treeland_output_group_v1::failure_invalid_name, QString());
        return;
    }
    if (!host) {
        handle->pending.clear();
        return;
    }

    const auto pending = std::exchange(handle->pending, {});
    bool changed = false;

    // A later set_source replaces an earlier uncommitted one: only the most
    // recent source designation in the queue is applied, the others are
    // dropped silently (no operation_failed for them either).
    int lastSetSource = -1;
    for (int i = 0; i < pending.size(); ++i) {
        if (pending.at(i).type == PendingRequest::SetSource) {
            lastSetSource = i;
        }
    }

    for (int i = 0; i < pending.size(); ++i) {
        const auto &request = pending.at(i);
        if (request.type == PendingRequest::SetSource && i != lastSetSource) {
            continue;
        }

        switch (request.type) {
        case PendingRequest::SetSource: {
            if (request.uuid.isEmpty() || !host->isOutputAvailable(request.uuid)) {
                handle->sendFailure(QtWaylandServer::treeland_output_group_v1::failure_invalid_output,
                                    request.uuid);
                break;
            }
            if (config->sourceUuid == request.uuid) {
                break;
            }
            if (effective.value(config->name).sourceUuid == request.uuid) {
                // The output is already the effective source, for example after
                // an auto-promotion: pin it as the configured source.
                const QString previousSource = config->sourceUuid;
                config->sourceUuid = request.uuid;
                config->mirrorUuids.removeAll(request.uuid);
                if (!previousSource.isEmpty() && previousSource != request.uuid
                    && !config->mirrorUuids.contains(previousSource)) {
                    config->mirrorUuids.append(previousSource);
                }
                changed = true;
                break;
            }
            const QString owner = groupOwningOutput(request.uuid, config->name);
            if (!owner.isEmpty()) {
                handle->sendFailure(QtWaylandServer::treeland_output_group_v1::failure_output_in_use,
                                    request.uuid);
                break;
            }
            if (config->mirrorUuids.contains(request.uuid)) {
                handle->sendFailure(QtWaylandServer::treeland_output_group_v1::failure_duplicate_output,
                                    request.uuid);
                break;
            }

            const QString previousSource = config->sourceUuid;
            config->sourceUuid = request.uuid;
            if (!previousSource.isEmpty()) {
                config->mirrorUuids.append(previousSource);
            }
            changed = true;
            break;
        }
        case PendingRequest::AddOutput: {
            if (request.uuid.isEmpty() || !host->isOutputAvailable(request.uuid)) {
                handle->sendFailure(QtWaylandServer::treeland_output_group_v1::failure_invalid_output,
                                    request.uuid);
                break;
            }
            if (config->sourceUuid == request.uuid
                || config->mirrorUuids.contains(request.uuid)) {
                handle->sendFailure(QtWaylandServer::treeland_output_group_v1::failure_duplicate_output,
                                    request.uuid);
                break;
            }
            const QString owner = groupOwningOutput(request.uuid, config->name);
            if (!owner.isEmpty()) {
                handle->sendFailure(QtWaylandServer::treeland_output_group_v1::failure_output_in_use,
                                    request.uuid);
                break;
            }
            config->mirrorUuids.append(request.uuid);
            changed = true;
            break;
        }
        case PendingRequest::RemoveOutput: {
            if (config->sourceUuid == request.uuid) {
                config->sourceUuid.clear();
                changed = true;
            } else if (config->mirrorUuids.removeOne(request.uuid)) {
                changed = true;
            } else {
                handle->sendFailure(QtWaylandServer::treeland_output_group_v1::failure_not_in_group,
                                    request.uuid);
            }
            break;
        }
        }
    }

    if (changed) {
        persistGroup(*config);
    }
    recomputeEffectiveState();
}

void OutputGroupManagerInterfaceV1Private::bind_resource(Resource *resource)
{
    loadPersistedGroupsIfReady();

    for (const auto &config : std::as_const(configs)) {
        send_group_added(resource->handle, config.name);
    }
    send_groups_done(resource->handle);
}

void OutputGroupManagerInterfaceV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void OutputGroupManagerInterfaceV1Private::create_group(Resource *resource,
                                                        uint32_t id,
                                                        const QString &source)
{
    OutputGroupConfig config;
    config.name = nextGroupName();

    QString failedUuid;
    uint32_t failure = 0;
    if (!source.isEmpty()) {
        if (!host || !host->isOutputAvailable(source)) {
            failedUuid = source;
            failure = QtWaylandServer::treeland_output_group_v1::failure_invalid_output;
        } else if (!groupOwningOutput(source).isEmpty()) {
            failedUuid = source;
            failure = QtWaylandServer::treeland_output_group_v1::failure_output_in_use;
        } else {
            config.sourceUuid = source;
        }
    }

    configs.append(config);
    persistGroup(config);
    persistGroupRegistry();
    // Compute the effective state before the new object exists, so its initial
    // batch carries the state the create request produced.
    recomputeEffectiveState();

    auto *handle = createHandle(resource, id, config.name, true);
    if (handle) {
        sendInitialState(handle);
        if (failure != 0) {
            handle->sendFailure(failure, failedUuid);
        }
        handle->sendDone();
    }

    broadcastGroupAdded(config.name, resource->handle);
}

void OutputGroupManagerInterfaceV1Private::get_group(Resource *resource,
                                                     uint32_t id,
                                                     const QString &name)
{
    if (name.isEmpty() || !findConfig(name)) {
        auto *handle = createHandle(resource, id, name, false);
        if (handle) {
            handle->sendFailure(QtWaylandServer::treeland_output_group_v1::failure_invalid_name, QString());
            handle->sendDone();
        }
        return;
    }

    recomputeEffectiveState();

    auto *handle = createHandle(resource, id, name, true);
    if (handle) {
        sendInitialState(handle);
        handle->sendDone();
    }
}

OutputGroupManagerInterfaceV1::OutputGroupManagerInterfaceV1(QObject *parent)
    : QObject(parent)
    , d(new OutputGroupManagerInterfaceV1Private(this))
{
}

OutputGroupManagerInterfaceV1::~OutputGroupManagerInterfaceV1() = default;

void OutputGroupManagerInterfaceV1::create(WServer *server)
{
    d->init(server->handle(), InterfaceVersion);
    d->loadPersistedGroupsIfReady();
}

void OutputGroupManagerInterfaceV1::destroy([[maybe_unused]] WServer *server)
{
    d->globalRemove();
}

wl_global *OutputGroupManagerInterfaceV1::global() const
{
    return d->globalHandle();
}

QByteArrayView OutputGroupManagerInterfaceV1::interfaceName() const
{
    return QtWaylandServer::treeland_output_group_manager_v1::interfaceName();
}

void OutputGroupManagerInterfaceV1::setHost(OutputGroupHost *host)
{
    d->host = host;
    d->loadPersistedGroupsIfReady();
    d->recomputeEffectiveState();
}

QList<OutputGroupRuntime> OutputGroupManagerInterfaceV1::effectiveGroups() const
{
    QList<OutputGroupRuntime> result;
    result.reserve(d->configs.size());
    for (const auto &config : std::as_const(d->configs)) {
        result.append(d->effective.value(config.name));
    }
    return result;
}

void OutputGroupManagerInterfaceV1::notifyOutputAvailabilityChanged()
{
    d->recomputeEffectiveState();
}

QString OutputGroupManagerInterfaceV1::createGroup(const QString &sourceUuid,
                                                   const QStringList &mirrorUuids)
{
    if (!d->host) {
        return {};
    }

    // This entry point behaves like a client using the protocol, so it applies
    // the same rules: members must be connected, enabled and claimed by no
    // other group.
    if (sourceUuid.isEmpty() || !d->host->isOutputAvailable(sourceUuid)
        || !d->groupOwningOutput(sourceUuid).isEmpty()) {
        return {};
    }

    QStringList acceptedMirrors;
    for (const auto &uuid : mirrorUuids) {
        if (d->host->isOutputAvailable(uuid) && d->groupOwningOutput(uuid).isEmpty()
            && uuid != sourceUuid && !acceptedMirrors.contains(uuid)) {
            acceptedMirrors.append(uuid);
        }
    }

    OutputGroupConfig config;
    config.name = d->nextGroupName();
    config.sourceUuid = sourceUuid;
    config.mirrorUuids = acceptedMirrors;

    d->configs.append(config);
    d->persistGroup(config);
    d->persistGroupRegistry();
    d->broadcastGroupAdded(config.name, nullptr);
    d->recomputeEffectiveState();
    return config.name;
}

bool OutputGroupManagerInterfaceV1::dissolveGroup(const QString &name)
{
    if (!d->findConfig(name)) {
        return false;
    }
    d->dissolveGroup(name);
    return true;
}
