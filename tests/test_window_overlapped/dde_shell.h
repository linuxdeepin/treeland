// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "qwayland-treeland-dde-shell-v1.h"

#include <qwaylandclientextension.h>

#include <qtmetamacros.h>

class DDEShell : public QWaylandClientExtensionTemplate<DDEShell>,
                 public QtWayland::treeland_dde_shell_manager_v1
{
    Q_OBJECT
public:
    explicit DDEShell();
};

class DDEShellWindowOverlapChecker
    : public QWaylandClientExtensionTemplate<DDEShellWindowOverlapChecker>,
      public QtWayland::treeland_window_overlap_checker
{
    Q_OBJECT
    Q_PROPERTY(bool overlapped READ overlapped NOTIFY overlappedChanged)
public:
    explicit DDEShellWindowOverlapChecker(struct ::treeland_window_overlap_checker *object);

    bool overlapped() const { return m_overlapped; }

Q_SIGNALS:
    void overlappedChanged();

protected:
    void treeland_window_overlap_checker_enter() override;
    void treeland_window_overlap_checker_leave() override;

private:
    bool m_overlapped;
};
