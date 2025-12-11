// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include <QObject>
#include <QSize>
#include <QHash>

// Simple window size persistence: records normalGeometry size at last normal close (destruction) based on appId  
class WindowSizeStore : public QObject {
public:
    explicit WindowSizeStore(QObject *parent = nullptr)
        : QObject(parent)
        , m_settings("deepin", "treeland-window-size") {}

    QSize lastSizeFor(const QString &appId) const {
        if (appId.isEmpty()) return {};
        qInfo() << "WindowSizeStore: last size for" << appId << "is"
                << m_settings.value(appId + "/normalSize").toSize();
        return m_settings.value(appId + "/normalSize").toSize();
    }

    void saveSize(const QString &appId, const QSize &size) {
        qInfo() << "WindowSizeStore: save size for" << appId << "as" << size;
        if (appId.isEmpty() || !size.isValid()) return;
        m_settings.setValue(appId + "/normalSize", size);
        m_settings.sync();
    }
private:
    // TODO(rewine): use dconfig
    mutable QSettings m_settings;
};
