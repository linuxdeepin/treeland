// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "abstractsettings.h"

AbstractSettings::AbstractSettings(xcb_connection_t *connection, QObject *parent)
    : QObject(parent)
    , m_connection(connection)
{
}

AbstractSettings::~AbstractSettings()
{
}
