// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "propertymonitor.h"
#include "common/treelandlogging.h"

#include <QEvent>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(qLcMonitor, "treeland.property.monitor")

PropertyMonitor::PropertyMonitor(QObject *parent)
    : QObject(parent)
    , m_target(parent)
{
}

QObject *PropertyMonitor::target() const
{
    return m_target;
}

void PropertyMonitor::setTarget(QObject *newTarget)
{
    if (m_target == newTarget)
        return;
    if (m_target)
        disconnect(m_target, nullptr, this, SLOT(handlePropertyChanged()));
    m_target = newTarget;
    connectToTarget();
    Q_EMIT targetChanged();
}

QString PropertyMonitor::properties() const
{
    return m_properties;
}

void PropertyMonitor::setProperties(const QString &newProperties)
{
    if (m_properties == newProperties)
        return;
    m_properties = newProperties;
    connectToTarget();
    Q_EMIT propertiesChanged();
}

void PropertyMonitor::handlePropertyChanged()
{
    auto index = senderSignalIndex();
    for (const auto &mProp : std::as_const(m_metaProps)) {
        if (mProp.hasNotifySignal() && mProp.notifySignalIndex() == index) {
            qCDebug(qLcMonitor) << m_target << mProp.name() << mProp.read(m_target);
            break;
        }
    }
}

void PropertyMonitor::connectToTarget()
{
    static const auto slotIndex = metaObject()->indexOfSlot("handlePropertyChanged()");
    static auto slot = metaObject()->method(slotIndex);
    if (!m_target || m_properties.isEmpty())
        return;
    disconnect(m_target, nullptr, this, SLOT(handlePropertyChanged()));
    m_metaProps.clear();
    auto propertyList = m_properties.split(",");
    for (const auto &property : std::as_const(propertyList)) {
        auto index =
            m_target->metaObject()->indexOfProperty(property.trimmed().toStdString().c_str());
        if (index != -1) {
            auto mProp = m_target->metaObject()->property(index);
            m_metaProps.append(mProp);
            if (mProp.hasNotifySignal()) {
                connect(m_target, mProp.notifySignal(), this, slot);
            }
        }
    }
}
