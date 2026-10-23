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

// Membership state of one connected output, persisted in the output's own
// configuration: the group it belongs to, when it joined the group and when
// it was last explicitly designated as the source (0 = never). The effective
// source is resolved from these records at runtime, never stored.
struct OutputGroupMemberInfo
{
    QString uuid;
    QString groupName;
    qlonglong joinTime = 0;
    qlonglong lastSetSourceTime = 0;
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
    // Membership of every connected output whose configuration is readable.
    virtual QList<OutputGroupMemberInfo> outputGroupMembers() const = 0;
    // Persist the membership fields of one connected output.
    virtual void writeOutputGroupMembership(const QString &uuid,
                                            const QString &groupName,
                                            qlonglong joinTime,
                                            qlonglong lastSetSourceTime) = 0;
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
