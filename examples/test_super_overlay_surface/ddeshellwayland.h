// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "qwayland-treeland-dde-shell-v1.h"

#include <QObject>
#include <QWindow>

class DDEShellSurface;

class DDEShellWayland : public QObject
{
    Q_OBJECT
public:
    static DDEShellWayland *get(QWindow *window);
    ~DDEShellWayland();

    void setPosition(const QPoint &position);
    void setRole(QtWayland::treeland_dde_shell_surface_v1::role role);
    void setAutoPlacement(int32_t yOffset);
    void setSkipSwitcher(uint32_t skip);
    void setSkipDockPreview(uint32_t skip);
    void setSkipMutiTaskView(uint32_t skip);
    void setAcceptKeyboardFocus(uint32_t accept);
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    DDEShellWayland(QWindow *window);
    void platformSurfaceCreated(QWindow *window);
    void surfaceCreated();
    void surfaceDestroyed();

    QWindow *m_window = nullptr;
    std::optional<QPoint> m_position;
    QtWayland::treeland_dde_shell_surface_v1::role m_role =
        QtWayland::treeland_dde_shell_surface_v1::role_overlay;
    std::optional<int32_t> m_yOffset;
    std::optional<bool> m_skipSwitcher;
    std::optional<bool> m_skipDockPreview;
    std::optional<bool> m_skipMutiTaskView;
    bool m_acceptKeyboardFocus = true;

    std::unique_ptr<DDEShellSurface> m_shellSurface;
};
