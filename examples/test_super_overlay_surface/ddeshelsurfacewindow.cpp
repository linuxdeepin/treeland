// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "ddeshelsurfacewindow.h"

#include "ddeshellwayland.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

DDEShelSurfaceWindow::DDEShelSurfaceWindow(TestMode mode, QWidget *parent)
    : QWidget{ parent }
    , m_mode(mode)
{
    setWindowTitle(QStringLiteral("treeland_dde_shell_unstable_v2 示例"));
    resize(520, 460);

    auto *title = new QLabel(QStringLiteral("<h3>treeland_dde_shell_unstable_v2 示例</h3>"
                                            "测试 DDE Shell surface 的 role / skip 位域 / 键盘焦点"));
    title->setWordWrap(true);

    // role 选择
    m_roleCombo = new QComboBox;
    m_roleCombo->addItem(QStringLiteral("role_overlay（overlay 层，位于普通窗口之上）"), int(QtWayland::treeland_dde_shell_surface_v2::role_overlay));
    m_roleCombo->addItem(QStringLiteral("role_normal（普通窗口层）"), int(QtWayland::treeland_dde_shell_surface_v2::role_normal));

    // skip 位域
    auto *skipGroup = new QGroupBox(QStringLiteral("set_skip_flags 位域"));
    auto *skipLayout = new QVBoxLayout(skipGroup);
    m_skipSwitcher = new QCheckBox(QStringLiteral("skip_flag_switcher (0x1)"));
    m_skipDockPreview = new QCheckBox(QStringLiteral("skip_flag_dock_preview (0x2)"));
    m_skipMultitask = new QCheckBox(QStringLiteral("skip_flag_multitask_view (0x4)"));
    skipLayout->addWidget(m_skipSwitcher);
    skipLayout->addWidget(m_skipDockPreview);
    skipLayout->addWidget(m_skipMultitask);

    // 键盘焦点
    auto *focusGroup = new QGroupBox(QStringLiteral("set_accept_keyboard_focus"));
    auto *focusLayout = new QVBoxLayout(focusGroup);
    m_acceptFocus = new QCheckBox(QStringLiteral("接受键盘焦点"));
    m_acceptFocus->setChecked(true);
    m_focusProbe = new QLineEdit;
    m_focusProbe->setPlaceholderText(QStringLiteral("点击此处验证是否能获得键盘焦点"));
    focusLayout->addWidget(m_acceptFocus);
    focusLayout->addWidget(m_focusProbe);

    m_applyButton = new QPushButton(QStringLiteral("应用"));
    m_statusLabel = new QLabel;
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addWidget(m_roleCombo);
    layout->addWidget(skipGroup);
    layout->addWidget(focusGroup);
    layout->addWidget(m_applyButton);
    layout->addWidget(m_statusLabel);

    connect(m_roleCombo, &QComboBox::currentIndexChanged, this, [this] {
        DDEShellWayland::get(windowHandle())
            ->setRole(static_cast<QtWayland::treeland_dde_shell_surface_v2::role>(
                m_roleCombo->currentData().toInt()));
        updateStatusLabel();
    });
    connect(m_applyButton, &QPushButton::clicked, this, &DDEShelSurfaceWindow::apply);
    connect(m_acceptFocus, &QCheckBox::toggled, this, [this](bool on) {
        DDEShellWayland::get(windowHandle())->setAcceptKeyboardFocus(on);
        updateStatusLabel();
    });
    connect(m_skipSwitcher, &QCheckBox::toggled, this, [this](bool on) {
        DDEShellWayland::get(windowHandle())->setSkipSwitcher(on);
        updateStatusLabel();
    });
    connect(m_skipDockPreview, &QCheckBox::toggled, this, [this](bool on) {
        DDEShellWayland::get(windowHandle())->setSkipDockPreview(on);
        updateStatusLabel();
    });
    connect(m_skipMultitask, &QCheckBox::toggled, this, [this](bool on) {
        DDEShellWayland::get(windowHandle())->setSkipMutiTaskView(on);
        updateStatusLabel();
    });

    updateStatusLabel();
}

void DDEShelSurfaceWindow::showEvent([[maybe_unused]] QShowEvent *event)
{
    if (isVisible()) {
        apply();
    }
}

void DDEShelSurfaceWindow::updateStatusLabel()
{
    const QString modeText = (m_mode == TestSetPosition)
        ? QStringLiteral("set_position_hint (x=%1, y=%2)").arg(m_position.x()).arg(m_position.y())
        : QStringLiteral("set_cursor_placement_hint (x=%1, y=%2)").arg(m_cursorOffset.x()).arg(m_cursorOffset.y());

    const int skip = (m_skipSwitcher->isChecked() ? 0x1 : 0)
        | (m_skipDockPreview->isChecked() ? 0x2 : 0)
        | (m_skipMultitask->isChecked() ? 0x4 : 0);

    const QString roleText = m_roleCombo->currentData().toInt() == int(QtWayland::treeland_dde_shell_surface_v2::role_overlay)
        ? QStringLiteral("overlay")
        : QStringLiteral("normal");

    m_statusLabel->setText(
        QStringLiteral("当前配置：%1\n"
                       "role: %2\n"
                       "skip_flags: 0x%3\n"
                       "accept_keyboard_focus: %4")
            .arg(modeText)
            .arg(roleText)
            .arg(skip, 0, 16)
            .arg(m_acceptFocus->isChecked() ? QStringLiteral("true") : QStringLiteral("false")));
}

void DDEShelSurfaceWindow::apply()
{
    auto *dde = DDEShellWayland::get(windowHandle());

    dde->setRole(static_cast<QtWayland::treeland_dde_shell_surface_v2::role>(m_roleCombo->currentData().toInt()));

    if (m_mode == TestSetPosition) {
        // 1 ---- Convenient for the client to set the position of the surface
        dde->setPosition(m_position);
        // ----------------------------------------------------------------
    } else {
        // 2. Place the surface relative to the cursor.--------------------
        dde->setCursorPlacement(m_cursorOffset.x(), m_cursorOffset.y());
        // ---------------------------------------------------------------
    }

    // Setting these bits will indicate that the window prefers not to be
    // listed in a switcher/dock-preview/mutitask-view. The skip flags are
    // applied immediately via their toggled handlers; the apply button only
    // (re)sends role / position / cursor placement / keyboard focus.
    dde->setAcceptKeyboardFocus(m_acceptFocus->isChecked());

    updateStatusLabel();
}
