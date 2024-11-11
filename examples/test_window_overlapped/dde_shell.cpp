// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "dde_shell.h"

DDEShell::DDEShell()
    : QWaylandClientExtensionTemplate<DDEShell>(1)
{
}

DDEShellWindowOverlapChecker::DDEShellWindowOverlapChecker(
    struct ::treeland_window_overlap_checker *object)
    : QWaylandClientExtensionTemplate<DDEShellWindowOverlapChecker>(1)
    , QtWayland::treeland_window_overlap_checker(object)
    , m_overlapped(false)
{
}

void DDEShellWindowOverlapChecker::treeland_window_overlap_checker_enter()
{
    m_overlapped = true;
    Q_EMIT overlappedChanged();
}

void DDEShellWindowOverlapChecker::treeland_window_overlap_checker_leave()
{
    m_overlapped = false;
    Q_EMIT overlappedChanged();
}
