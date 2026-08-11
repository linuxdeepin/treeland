// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "backlight.h"

#include "woutput.h"
#include "qwbackend.h"

#include "common/treelandlogging.h"

QW_USE_NAMESPACE

Backlight::Backlight(const QString &name, const QString &basePath)
    : m_maxBrightness(0)
    , m_brightnessLevel(0)
    , m_name(name)
    , m_basePath(basePath)
{
    QFile maxBrightnessFile(QString("%1/%2/max_brightness").arg(m_basePath, name));
    if (!maxBrightnessFile.open(QIODevice::ReadOnly)) {
        qCWarning(lcTlOutput) << "Backlight: Failed to open max_brightness for" << name;
        return;
    }

    bool ok = false;
    m_maxBrightness = maxBrightnessFile.readLine().trimmed().toLongLong(&ok);
    maxBrightnessFile.close();
    if (!ok || m_maxBrightness <= 0) {
        qCWarning(lcTlOutput) << "Backlight: Invalid max_brightness for" << name;
        m_maxBrightness = 0;
        return;
    }

    QFile brightnessFile(QString("%1/%2/brightness").arg(m_basePath, name));
    if (!brightnessFile.open(QIODevice::ReadOnly)) {
        qCWarning(lcTlOutput) << "Backlight: Failed to open brightness for" << name;
        m_maxBrightness = 0;
        return;
    }

    m_brightnessLevel = brightnessFile.readLine().trimmed().toLongLong(&ok);
    brightnessFile.close();
    if (!ok) {
        qCWarning(lcTlOutput) << "Backlight: Invalid brightness for" << name;
        m_maxBrightness = 0;
        m_brightnessLevel = 0;
        return;
    }
}

Backlight::~Backlight()
{

}

qreal Backlight::brightness() const
{
    if (m_maxBrightness <= 0)
        return 0.0;
    return m_brightnessLevel / static_cast<qreal>(m_maxBrightness);
}

qreal Backlight::setBrightness(qreal brightness)
{
    qlonglong brightnessLevel = qMin(qCeil(brightness * m_maxBrightness), m_maxBrightness);

    if (brightnessLevel == m_brightnessLevel) {
        return this->brightness();
    }

    QFile brightnessFile(QString("%1/%2/brightness").arg(m_basePath, m_name));
    if (!brightnessFile.open(QIODevice::WriteOnly)) {
        qCWarning(lcTlOutput) << "Output" << m_name << ": Failed to open backlight brightness file for writing.";
        return this->brightness();
    }

    QByteArray brightnessStr = QByteArray::number(brightnessLevel);
    qint64 written = brightnessFile.write(brightnessStr);
    brightnessFile.close();
    if (written != brightnessStr.size()) {
        qCWarning(lcTlOutput) << "Output" << m_name << ": Failed to write backlight brightness.";
        return this->brightness();
    }

    m_brightnessLevel = brightnessLevel;
    return this->brightness();    
}


Backlight* Backlight::createForOutput(WOutput* output)
{
    // query backlight driver through drm connector id
    if (output->handle()->is_drm()) {
        QDirIterator backlightIter("/sys/class/backlight", QDir::Dirs | QDir::NoDot);
        uint connectorId = qw_drm_backend::connector_get_id(output->nativeHandle());
        QString dirname;
        uint backlightCount = 0;
        while (backlightIter.hasNext()) {
            backlightCount++;
            backlightIter.next();
            dirname = backlightIter.fileName();
            auto idFile = QFile("/sys/class/backlight/" + dirname + "/device/connector_id");
            if (!idFile.exists() || !idFile.open(QIODevice::ReadOnly))
                continue;
            bool ok = false;
            uint id = idFile.readLine().toUInt(&ok);
            idFile.close();
            if (!ok)
                continue;
            if (connectorId == id)
                return new Backlight(dirname);
        }

        // heuristic: map the only backlight driver to internal panel
        if (output->name() == "eDP-1" && backlightCount == 1) {
            return new Backlight(dirname);
        }
    }
    return nullptr;
}
