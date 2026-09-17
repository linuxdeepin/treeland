// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QPoint>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

class DDEShelSurfaceWindow : public QWidget
{
    Q_OBJECT
public:
    enum TestMode
    {
        TestSetPosition,
        TestSetCursorPlacement
    };

    explicit DDEShelSurfaceWindow(TestMode mode, QWidget *parent = nullptr);

    void setPositionHint(QPoint pos) { m_position = pos; }
    void setCursorOffset(QPoint offset) { m_cursorOffset = offset; }

protected:
    void showEvent(QShowEvent *event) override;

private:
    void apply();
    void updateStatusLabel();

private:
    TestMode m_mode;
    QPoint m_position{ 100, 100 };
    QPoint m_cursorOffset{ 0, 30 };

    QComboBox *m_roleCombo = nullptr;
    QCheckBox *m_skipSwitcher = nullptr;
    QCheckBox *m_skipDockPreview = nullptr;
    QCheckBox *m_skipMultitask = nullptr;
    QCheckBox *m_acceptFocus = nullptr;
    QLineEdit *m_focusProbe = nullptr;
    QPushButton *m_applyButton = nullptr;
    QLabel *m_statusLabel = nullptr;
};
