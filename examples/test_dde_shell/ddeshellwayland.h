// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "qwayland-treeland-dde-shell-unstable-v2.h"

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
    void setRole(QtWayland::treeland_dde_shell_surface_v2::role role);
    void setCursorPlacement(int32_t xOffset, int32_t yOffset);
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
    std::optional<QtWayland::treeland_dde_shell_surface_v2::role> m_role;
    std::optional<QPoint> m_cursorPlacement;
    uint32_t m_skipFlags = 0;
    bool m_acceptKeyboardFocus = true;

    std::unique_ptr<DDEShellSurface> m_shellSurface;
};
