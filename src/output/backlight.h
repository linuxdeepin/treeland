// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

WAYLIB_SERVER_BEGIN_NAMESPACE
class WOutput;
WAYLIB_SERVER_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE

class Backlight
{
public:
    Backlight(const QString &name);
    ~Backlight();
    static Backlight* createForOutput(WOutput* output);
    qreal brightness() const;
    qreal setBrightness(qreal brightness);
private:
    qlonglong m_maxBrightness;
    qlonglong m_brightnessLevel;
    QString m_name;
};
