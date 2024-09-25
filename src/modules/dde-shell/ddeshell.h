// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>
#include <wsurfaceitem.h>

#include <QObject>
#include <QQmlEngine>

WAYLIB_SERVER_USE_NAMESPACE

class treeland_dde_shell_manager_v1;
class treeland_window_overlap_checker;
class DDEShellV1;

class DDEShellAttached : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
public:
    DDEShellAttached(WSurfaceItem *target, QObject *parent = nullptr);

protected:
    WSurfaceItem *m_target;
};

class WindowOverlapChecker : public DDEShellAttached
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(bool overlapped READ overlapped WRITE setOverlapped NOTIFY overlappedChanged)

public:
    WindowOverlapChecker(WSurfaceItem *target, QObject *parent = nullptr);

    inline bool overlapped() const { return m_overlapped; }

Q_SIGNALS:
    void overlappedChanged();

private:
    void setOverlapped(bool overlapped);

    bool m_overlapped{ false };
};

class DDEShell : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(DDEShell)
    QML_UNCREATABLE("Only use for the attached.")
    QML_ATTACHED(DDEShellAttached)

public:
    using QObject::QObject;
    ~DDEShell() override = default;

    static DDEShellAttached *qmlAttachedProperties(QObject *target);
};

class DDEShellV1
    : public QObject
    , public WServerInterface
{
    Q_OBJECT
public:
    explicit DDEShellV1(QObject *parent = nullptr);
    ~DDEShellV1() override = default;

    void checkRegionalConflict(WSurfaceItem *target);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
    QByteArrayView interfaceName() const override;

private:
    treeland_dde_shell_manager_v1 *m_manager;
    QMap<treeland_window_overlap_checker *, QRect> m_conflictList;
};
