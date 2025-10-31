// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "backlight.h"
#include "sysfsbacklight.h"

#include "qwbackend.h"

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

qreal Backlight::setBrightness(qreal brightness)
{
    Q_UNUSED(brightness);

    brightness = qBound(minBrightness(), brightness, maxBrightness());
    if (qFuzzyCompare(m_brightness, brightness)) {
        return m_brightness;
    }
    m_brightness = handleSetBrightness(brightness);
    return m_brightness;
}

qreal Backlight::handleSetBrightness(qreal brightness)
{
    Q_UNUSED(brightness);
    
    Q_UNREACHABLE();
}

Backlight* Backlight::createForOutput(WOutput* output)
{
    // query backlight driver through drm connector id
    auto backlightDirs = QDir("/sys/class/backlight").entryList(QDir::Dirs | QDir::NoDot);
    if (output->handle()->is_drm()) {
        uint connectorId = qw_drm_backend::connector_get_id(output->nativeHandle());
        for (const auto &dirname : backlightDirs) {
            auto idFile = QFile("/sys/class/backlight/" + dirname + "/device/connector_id");
            if (!idFile.exists() || !idFile.open(QIODevice::ReadOnly))
                continue;
            bool ok = false;
            uint id = idFile.readLine().toUInt(&ok);
            idFile.close();
            if (!ok)
                continue;
            if (connectorId == id)
                return new SysFSBacklight(dirname);
        }
    }
    // heuristic: map the only backlight driver to internal panel
    if (output->name() == "eDP-1" && backlightDirs.length() == 1) {
        return new SysFSBacklight(backlightDirs.first());
    }
    return new Backlight();
}
