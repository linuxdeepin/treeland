// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <DConfig>

#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusPendingReply>
#include <QObject>

#include <memory>

// DConfig owns this backend. Both it and the generated configuration's DConfig
// live on DConfig::globalThread(); callers connect configInitializeSucceed to subscribe.
class UserConfigBackend
    : public QObject
    , public DTK_CORE_NAMESPACE::DConfigBackend
{
    Q_OBJECT
public:
    UserConfigBackend(uint uid, const QString &name, const QString &subpath = {});
    ~UserConfigBackend() override;

    bool load(const QString &appId) override;
    bool isValid() const override;
    QStringList keyList() const override;
    QVariant value(const QString &key, const QVariant &fallback) const override;
    void setValue(const QString &key, const QVariant &value) override;
    void reset(const QString &key) override;
    bool isDefaultValue(const QString &key) const override;
    bool isReadOnly(const QString &key) const override;
    QString name() const override;

    void subscribe(DTK_CORE_NAMESPACE::DConfig *config);

private:
    static QVariant decodeValue(const QVariant &value);
    QDBusPendingReply<QDBusObjectPath> acquireManagerV2(const QString &appId) const;
    QDBusMessage call(const QString &method, const QVariantList &arguments) const;

    const uint m_uid;
    const QString m_name;
    const QString m_subpath;
    std::unique_ptr<QDBusInterface> m_manager;
};
