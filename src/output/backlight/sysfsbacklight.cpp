// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "sysfsbacklight.h"

SysFSBacklight::SysFSBacklight(const QString &name)
    : m_name(name)
{
    m_maxBrightness = 1.0;
    m_minBrightness = 1.0;

    QFile maxBrightnessFile(QString("/sys/class/backlight/%1/max_brightness").arg(name));
    Q_ASSERT(maxBrightnessFile.open(QIODevice::ReadOnly));

    bool ok = false;
    m_brightnessScale = maxBrightnessFile.readLine().trimmed().toDouble(&ok);
    maxBrightnessFile.close();
    Q_ASSERT(ok);
    Q_ASSERT(m_brightnessScale > 0.0);

    QFile brightnessFile(QString("/sys/class/backlight/%1/brightness").arg(name));
    Q_ASSERT(brightnessFile.open(QIODevice::ReadOnly));

    m_brightness = brightnessFile.readLine().trimmed().toDouble(&ok) / m_brightnessScale;
    brightnessFile.close();
    Q_ASSERT(ok);
}

SysFSBacklight::~SysFSBacklight()
{

}

qreal SysFSBacklight::handleSetBrightness(qreal brightness)
{
    QFile brightnessFile(QString("/sys/class/backlight/%1/brightness").arg(m_name));
    Q_ASSERT(brightnessFile.open(QIODevice::WriteOnly));

    int brightnessLevel = qRound(brightness * m_brightnessScale);
    QByteArray brightnessStr = QByteArray::number(brightnessLevel);
    qint64 written = brightnessFile.write(brightnessStr);
    brightnessFile.close();
    Q_ASSERT(written == brightnessStr.size());
    return brightnessLevel / m_brightnessScale;
}
