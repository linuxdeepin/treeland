// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

WAYLIB_SERVER_USE_NAMESPACE

// Configured (persisted) state of one output group.
struct OutputGroupConfig
{
    QString name;
    QString sourceUuid;
    QStringList mirrorUuids;
};

// Effective runtime state of one output group as seen by clients.
struct OutputGroupRuntime
{
    QString name;
    QString sourceUuid;
    QStringList mirrorUuids;
};

// Compositor side of the protocol: the group manager decides what the groups
// mean, this class only keeps the protocol state and forwards the effective
// configuration to the host which applies copy mode to the outputs.
class OutputGroupHost
{
public:
    virtual ~OutputGroupHost() = default;

    // Output is connected and enabled, so it may take part in copy mode.
    virtual bool isOutputAvailable(const QString &uuid) const = 0;
    // The initial output scan is over, so an auto-promoted source may become
    // the permanent configured source.
    virtual bool canReconfigureOutputs() const = 0;
    // Apply the effective group state to the outputs.
    virtual void applyOutputGroups() = 0;
};

class OutputGroupManagerInterfaceV1Private;
class OutputGroupManagerInterfaceV1
    : public QObject
    , public WServerInterface
{
    Q_OBJECT

public:
    explicit OutputGroupManagerInterfaceV1(QObject *parent = nullptr);
    ~OutputGroupManagerInterfaceV1() override;

    static constexpr int InterfaceVersion = 1;

    QByteArrayView interfaceName() const override;

    void setHost(OutputGroupHost *host);
    QList<OutputGroupRuntime> effectiveGroups() const;

    // Output availability changed (hotplug, enable, disable): recompute the
    // effective state and re-apply copy mode.
    void notifyOutputAvailabilityChanged();

    // Debug/menu-bar entry points, they behave like a client using the
    // protocol: one group whose source is sourceUuid. Returns the
    // compositor-assigned name of the created group, or an empty string when
    // no group was created.
    QString createGroup(const QString &sourceUuid, const QStringList &mirrorUuids);
    bool dissolveGroup(const QString &name);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private:
    friend class OutputGroupManagerInterfaceV1Private;
    std::unique_ptr<OutputGroupManagerInterfaceV1Private> d;
};
