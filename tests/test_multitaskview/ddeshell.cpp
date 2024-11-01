// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "ddeshell.h"

void Multitaskview::toggle()
{
    QtWayland::treeland_multitaskview_v1::toggle();
}

Multitaskview::Multitaskview()
    : QWaylandClientExtensionTemplate<Multitaskview>(1)
    , QtWayland::treeland_multitaskview_v1()
    , m_manager(new DDEShell)
{
    connect(
        m_manager,
        &DDEShell::activeChanged,
        this,
        [this]() {
            if (m_manager->isActive()) {
                auto object = m_manager->get_treeland_multitaskview();
                Q_ASSERT(object);
                init(object);
                Q_EMIT readyChanged();
            }
        },
        Qt::QueuedConnection);
}

Multitaskview::~Multitaskview()
{
    m_manager->deleteLater();
}
