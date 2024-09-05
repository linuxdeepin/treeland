// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include "datacontrolmanagerv1.h"

#include <QObject>
#include <QClipboard>

class Clipboard : public QObject
{
    Q_OBJECT
public:
    explicit Clipboard(QObject *parent = nullptr);
    ~Clipboard() = default;

    bool isValid();

Q_SIGNALS:
    void changed(QClipboard::Mode mode);

private:
    std::unique_ptr<DataControlDeviceV1ManagerV1> m_manager;
    std::unique_ptr<DataControlDeviceV1> m_device;
};
