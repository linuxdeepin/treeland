// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "ddeshelsurfacewindow.h"

#include "ddeshellwayland.h"

#include <QLineEdit>

DDEShelSurfaceWindow::DDEShelSurfaceWindow(TestMode mode, QWidget *parent)
    : QWidget{ parent }
    , m_mode(mode)
{
    [[maybe_unused]] QLineEdit *l = new QLineEdit(this);
}

void DDEShelSurfaceWindow::showEvent([[maybe_unused]] QShowEvent *event)
{
    if (isVisible()) {
        apply();
    }
}

void DDEShelSurfaceWindow::apply()
{
    if (TestSetPosition == m_mode) {
        // 1 ----Convenient for the client to set the position of the surface
        DDEShellWayland::get(windowHandle())->setPosition(QPoint(100, 100));
        // ----------------------------------------------------------------
    }

    if (TestSetAutoPlace == m_mode) {
        // 2. Set the vertical alignment of the surface within the cursor width,
        // y offset is 30 relative to the cursor bottom.-------------------
        DDEShellWayland::get(windowHandle())->setAutoPlacement(30);

        // Setting this bit will indicate that the window prefers not to be
        // listed in a switcher/dock-preview/mutitask-view
        DDEShellWayland::get(windowHandle())->setSkipDockPreview(true);
        DDEShellWayland::get(windowHandle())->setSkipMutiTaskView(true);
        DDEShellWayland::get(windowHandle())->setSkipSwitcher(true);
        DDEShellWayland::get(windowHandle())->setAcceptKeyboardFocus(false);
        // ---------------------------------------------------------------
    }

    // Do not use setPosition and setAutoPlacement at the same time, there will
    // be conflicts !!!

    DDEShellWayland::get(windowHandle())
        ->setRole(QtWayland::treeland_dde_shell_surface_v1::role_overlay);
}
