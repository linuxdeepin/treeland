// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "backlight.h"

class SysFSBacklight : virtual public Backlight
{
public:
    SysFSBacklight(const QString &name);
    ~SysFSBacklight();
private:
    qreal handleSetBrightness(qreal brightness) override;
    double m_brightnessScale;
    QString m_name;
};
