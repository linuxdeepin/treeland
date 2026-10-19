// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "outputuuidmanagerinterfacev1.h"

#include "core/waylandserver.h"
#include "qwayland-server-treeland-output-uuid-unstable-v1.h"
#include "qwayland-server-treeland-output-uuid-head-unstable-v1.h"

#include <wbackend.h>
#include <woutput.h>
#include <wlr_all.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUuid>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
// Identity of a physical output device, following Output::getOutputId() for the
// primary (model + serial) branch so the uuid key stays consistent with the
// existing per-output DConfig keys. When the EDID serial is missing, fall back
// to the model alone instead of the connector name: the connector is a
// location, not a device identity, so a device replugged into a different
// connector would otherwise get a fresh uuid.
QString baseIdentity(wlr_output *output)
{
    const QString model = QString::fromUtf8(output->model ? output->model : "");
    const QString serial = QString::fromUtf8(output->serial ? output->serial : "");
    if (!model.isEmpty() && !serial.isEmpty()) {
        return model + serial;
    }
    if (!model.isEmpty()) {
        return model;
    }
    return QString::fromUtf8(output->name ? output->name : "");
}

// The protocol requires that no two currently connected outputs report the same
// uuid. Two distinct devices can share the same base identity (same model and
// serial, e.g. two panels of the same batch), so when the base identity is not
// unique among the connected outputs, disambiguate with the connector name.
// The connector suffix is stable across sessions, so the assignment is still
// persistent as long as the collision persists.
QString stableIdentityKey(wlr_output *output)
{
    const QString base = baseIdentity(output);

    int occurrences = 0;
    if (auto *server = WaylandServer::instance()) {
        if (auto *backend = server->backend()) {
            const auto outputs = backend->outputList();
            for (WOutput *w : outputs) {
                if (w && baseIdentity(w->handle()) == base) {
                    ++occurrences;
                }
            }
        }
    }

    const QString name = QString::fromUtf8(output->name ? output->name : "");
    if (occurrences > 1 && !name.isEmpty()) {
        return base + QLatin1Char('@') + name;
    }
    return base;
}
} // namespace

class OutputUuidStore
{
public:
    static OutputUuidStore *instance()
    {
        static OutputUuidStore store;
        return &store;
    }

    QString uuidFor(const QString &identity)
    {
        const auto it = m_uuids.constFind(identity);
        if (it != m_uuids.constEnd()) {
            return it.value();
        }

        const QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_uuids.insert(identity, uuid);
        save();
        return uuid;
    }

private:
    OutputUuidStore()
    {
        load();
    }

    QString filePath() const
    {
        return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QLatin1String("/output-uuid.json");
    }

    void load()
    {
        QFile file(filePath());
        if (!file.open(QIODevice::ReadOnly)) {
            return;
        }

        // A corrupt or non-object JSON document is treated as an empty mapping.
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        const QJsonObject object = doc.object();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            if (it.value().isString()) {
                m_uuids.insert(it.key(), it.value().toString());
            }
        }
    }

    void save() const
    {
        QJsonObject object;
        for (auto it = m_uuids.constBegin(); it != m_uuids.constEnd(); ++it) {
            object.insert(it.key(), it.value());
        }

        const QString path = filePath();
        QDir().mkpath(QFileInfo(path).absolutePath());

        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            file.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
        }
    }

    QHash<QString, QString> m_uuids;
};

class OutputUuidV1;

class OutputUuidV1Private
    : public QObject
    , public QtWaylandServer::treeland_output_uuid_v1
{
    Q_OBJECT
public:
    OutputUuidV1Private(OutputUuidV1 *q, wl_resource *resource, const QString &uuid)
        : QObject()
        , QtWaylandServer::treeland_output_uuid_v1(resource)
        , q(q)
        , m_uuid(uuid)
    {
        send_uuid(resource, m_uuid);
    }

protected:
    void destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void destroy_resource(Resource *resource) override;

private:
    OutputUuidV1 *q = nullptr;
    QString m_uuid;
};

class OutputUuidV1 : public QObject
{
    Q_OBJECT
public:
    ~OutputUuidV1() override = default;

private:
    explicit OutputUuidV1(wl_resource *resource, const QString &uuid)
        : QObject()
        , d(std::make_unique<OutputUuidV1Private>(this, resource, uuid))
    {
    }

    std::unique_ptr<OutputUuidV1Private> d;

    friend class OutputUuidManagerV1Private;
    friend class OutputUuidHeadManagerV1Private;
};

void OutputUuidV1Private::destroy_resource(Resource *resource)
{
    Q_UNUSED(resource);
    // The outer wrapper owns this object through its unique_ptr, so the
    // whole treeland_output_uuid_v1 object lifetime ends with the outer
    // QObject (same pattern as ColorControlV1).
    q->deleteLater();
}

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

        const QString identity = stableIdentityKey(wlr_o);
        const QString uuid = OutputUuidStore::instance()->uuidFor(identity);

        auto *objRes = wl_resource_create(resource->client(),
                                          QtWaylandServer::treeland_output_uuid_v1::interface(),
                                          1,
                                          id);
        if (!objRes) {
            wl_resource_post_no_memory(resource->handle);
            return;
        }

        new OutputUuidV1(objRes, uuid); // sends uuid during construction
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
        auto *headObj = static_cast<wlr_output_head_v1 *>(wl_resource_get_user_data(head));
        if (!headObj || !headObj->state.output) {
            wl_resource_post_error(resource->handle,
                                   WL_DISPLAY_ERROR_INVALID_OBJECT,
                                   "Invalid wlr output head resource");
            return;
        }

        const QString identity = stableIdentityKey(headObj->state.output);
        const QString uuid = OutputUuidStore::instance()->uuidFor(identity);

        auto *objRes = wl_resource_create(resource->client(),
                                          QtWaylandServer::treeland_output_uuid_v1::interface(),
                                          1,
                                          id);
        if (!objRes) {
            wl_resource_post_no_memory(resource->handle);
            return;
        }

        new OutputUuidV1(objRes, uuid); // sends uuid during construction
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
