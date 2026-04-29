// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "utils/propertymonitor.h"

#include <QSignalSpy>
#include <QTest>

class MonitorTarget : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)

public:
    int value() const { return m_value; }

    void setValue(int value)
    {
        if (m_value == value)
            return;
        m_value = value;
        Q_EMIT valueChanged();
    }

    QString title() const { return m_title; }

    void setTitle(const QString &title)
    {
        if (m_title == title)
            return;
        m_title = title;
        Q_EMIT titleChanged();
    }

    int valueReceiverCount() const
    {
        return receivers(SIGNAL(valueChanged()));
    }

    int titleReceiverCount() const
    {
        return receivers(SIGNAL(titleChanged()));
    }

Q_SIGNALS:
    void valueChanged();
    void titleChanged();

private:
    int m_value = 0;
    QString m_title;
};

class PropertyMonitorTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void emitsChangeSignalsWhenConfigurationChanges()
    {
        PropertyMonitor monitor;
        MonitorTarget target;
        QSignalSpy targetChangedSpy(&monitor, &PropertyMonitor::targetChanged);
        QSignalSpy propertiesChangedSpy(&monitor, &PropertyMonitor::propertiesChanged);

        monitor.setTarget(&target);
        monitor.setProperties(QStringLiteral("value,title"));

        QCOMPARE(monitor.target(), &target);
        QCOMPARE(monitor.properties(), QStringLiteral("value,title"));
        QCOMPARE(targetChangedSpy.count(), 1);
        QCOMPARE(propertiesChangedSpy.count(), 1);

        monitor.setTarget(&target);
        monitor.setProperties(QStringLiteral("value,title"));

        QCOMPARE(targetChangedSpy.count(), 1);
        QCOMPARE(propertiesChangedSpy.count(), 1);
    }

    void connectsOnlyNamedNotifyPropertiesOnCurrentTarget()
    {
        PropertyMonitor monitor;
        MonitorTarget firstTarget;
        MonitorTarget secondTarget;

        monitor.setTarget(&firstTarget);
        monitor.setProperties(QStringLiteral("value, missing"));

        QVERIFY(firstTarget.valueReceiverCount() > 0);
        QCOMPARE(firstTarget.titleReceiverCount(), 0);

        monitor.setTarget(&secondTarget);

        QCOMPARE(firstTarget.valueReceiverCount(), 0);
        QVERIFY(secondTarget.valueReceiverCount() > 0);
        QCOMPARE(secondTarget.titleReceiverCount(), 0);
    }

    void reconnectsWhenPropertyListChanges()
    {
        PropertyMonitor monitor;
        MonitorTarget target;

        monitor.setTarget(&target);
        monitor.setProperties(QStringLiteral("value"));

        QVERIFY(target.valueReceiverCount() > 0);
        QCOMPARE(target.titleReceiverCount(), 0);

        monitor.setProperties(QStringLiteral("title"));

        QCOMPARE(target.valueReceiverCount(), 0);
        QVERIFY(target.titleReceiverCount() > 0);
    }
};

QTEST_MAIN(PropertyMonitorTest)
#include "main.moc"
