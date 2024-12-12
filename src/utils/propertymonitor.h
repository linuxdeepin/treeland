// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include <QObject>
#include <QQmlEngine>

class PropertyMonitor : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QObject *target READ target WRITE setTarget NOTIFY targetChanged FINAL)
    Q_PROPERTY(QString properties READ properties WRITE setProperties NOTIFY propertiesChanged REQUIRED FINAL)

public:
    explicit PropertyMonitor(QObject *parent = nullptr);

    QObject *target() const;
    void setTarget(QObject *newTarget);

    QString properties() const;
    void setProperties(const QString &newProperties);

public Q_SLOTS:
    void handlePropertyChanged();

Q_SIGNALS:
    void targetChanged();
    void propertiesChanged();

private:
    void connectToTarget();

    QObject *m_target = nullptr;
    QString m_properties;
    QList<QMetaProperty> m_metaProps;
};
