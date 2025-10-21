// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "backlight.h"

#include <woutput.h>

Backlight::Backlight()
    : m_maxBrightness(1.0)
    , m_minBrightness(1.0)
    , m_brightness(1.0)
{

}

Backlight::~Backlight()
{

}

qreal Backlight::brightness() const
{
    return m_brightness;
}

qreal Backlight::maxBrightness() const
{
    return m_maxBrightness;
}

qreal Backlight::minBrightness() const
{
    return m_minBrightness;
}

qreal Backlight::setBrightness([[maybe_unused]]qreal brightness)
{
    return 1.0;
}


Backlight* Backlight::createForOutput([[maybe_unused]]WOutput *output) {
    return new Backlight();
}
