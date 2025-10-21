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
    Backlight();
    virtual ~Backlight();
    static Backlight* createForOutput(WOutput* output);
    qreal brightness() const;
    qreal maxBrightness() const;
    qreal minBrightness() const;
    qreal setBrightness(qreal brightness);
private:
    virtual qreal handleSetBrightness(qreal brightness);
protected:
    qreal m_maxBrightness;
    qreal m_minBrightness;
    qreal m_brightness;
};
