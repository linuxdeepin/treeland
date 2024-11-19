// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QQuickItem>

class DDEShellAttached : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
public:
    DDEShellAttached(QQuickItem *target, QObject *parent = nullptr);

protected:
    QQuickItem *m_target;
};

class WindowOverlapChecker : public DDEShellAttached
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(bool overlapped READ overlapped WRITE setOverlapped NOTIFY overlappedChanged)

public:
    WindowOverlapChecker(QQuickItem *target, QObject *parent = nullptr);
    ~WindowOverlapChecker();

    inline bool overlapped() const
    {
        return m_overlapped;
    }

Q_SIGNALS:
    void overlappedChanged();

private:
    void setOverlapped(bool overlapped);

    bool m_overlapped{ false };
    QRect m_lastRect;

    inline static QRegion region;
};

class DDEShellHelper : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(DDEShell)
    QML_UNCREATABLE("Only use for the attached.")
    QML_ATTACHED(DDEShellAttached)

public:
    using QObject::QObject;
    ~DDEShellHelper() override = default;

    static DDEShellAttached *qmlAttachedProperties(QObject *target);
};
