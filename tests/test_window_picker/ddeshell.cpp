// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "ddeshell.h"

void WindowPickerClient::pick()
{
    QtWayland::treeland_window_picker_v1::pick("Pick window pid");
}

WindowPickerClient::WindowPickerClient()
    : QWaylandClientExtensionTemplate<WindowPickerClient>(1)
    , QtWayland::treeland_window_picker_v1()
    , m_manager(new DDEShell)
{
    connect(
        m_manager,
        &DDEShell::activeChanged,
        this,
        [this]() {
            if (m_manager->isActive()) {
                auto object = m_manager->get_treeland_window_picker();
                Q_ASSERT(object);
                init(object);
                Q_EMIT readyChanged();
            }
        },
        Qt::QueuedConnection);
}

WindowPickerClient::~WindowPickerClient()
{
    m_manager->deleteLater();
}

void WindowPickerClient::treeland_window_picker_v1_window(int32_t pid)
{
    m_pid = pid;
    Q_EMIT pidChanged();
}
