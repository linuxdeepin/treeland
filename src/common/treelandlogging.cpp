// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "common/treelandlogging.h"

#include "treelandconfig.hpp"

#include <QStringList>

namespace {

QString normalizeRules(const QString &rules)
{
    QString normalized = rules;
    normalized.replace(';', '\n');
    return normalized;
}

QString mergeRules(const QString &baseRules, const QString &extraRules)
{
    // QLoggingCategory::setFilterRules is order-dependent: later rules win.
    // Keep original order from environment and append dconfig overrides.
    const QStringList baseLines = normalizeRules(baseRules).split('\n', Qt::SkipEmptyParts);
    const QStringList extraLines = normalizeRules(extraRules).split('\n', Qt::SkipEmptyParts);

    QStringList mergedLines;
    mergedLines.reserve(baseLines.size() + extraLines.size());

    for (const QString &line : baseLines) {
        if (line.contains('=')) {
            mergedLines << line.trimmed();
        }
    }
    for (const QString &line : extraLines) {
        if (line.contains('=')) {
            mergedLines << line.trimmed();
        }
    }

    return mergedLines.join('\n');
}

QString baseLoggingRules()
{
    static const QString rules =
        normalizeRules(QString::fromLocal8Bit(qgetenv("QT_LOGGING_RULES")));
    return rules;
}

void applyLoggingRules(TreelandConfig *config)
{
    if (!config) {
        return;
    }

    const QString mergedRules = mergeRules(baseLoggingRules(), config->logRules());
    QLoggingCategory::setFilterRules(mergedRules);
    qCInfo(treelandConfig).noquote() << "Apply logging rules:" << mergedRules;
}

} // namespace

// TreeLand logging category definitions
// Naming convention: treeland.module_name.submodule_name

// Core modules
Q_LOGGING_CATEGORY(treelandCore, "treeland.core")
Q_LOGGING_CATEGORY(treelandServer, "treeland.server")
Q_LOGGING_CATEGORY(treelandCompositor, "treeland.compositor")
Q_LOGGING_CATEGORY(treelandShell, "treeland.shell")

// Input modules
Q_LOGGING_CATEGORY(treelandInput, "treeland.input")
Q_LOGGING_CATEGORY(treelandGestures, "treeland.gestures")

// Output module
Q_LOGGING_CATEGORY(treelandOutput, "treeland.output")

// Window management
Q_LOGGING_CATEGORY(treelandSurface, "treeland.surface")

// Protocol module
Q_LOGGING_CATEGORY(treelandProtocol, "treeland.protocol")

// Plugin system
Q_LOGGING_CATEGORY(treelandPlugin, "treeland.plugin")

// Configuration management
Q_LOGGING_CATEGORY(treelandConfig, "treeland.config")

// Workspace management
Q_LOGGING_CATEGORY(treelandWorkspace, "treeland.workspace")

// Wallpaper system
Q_LOGGING_CATEGORY(treelandWallpaper, "treeland.wallpaper")

// Effects system
Q_LOGGING_CATEGORY(treelandEffect, "treeland.effect")

// Capture system
Q_LOGGING_CATEGORY(treelandCapture, "treeland.capture")

// DBus interface
Q_LOGGING_CATEGORY(treelandDBus, "treeland.dbus")

// Utility classes
Q_LOGGING_CATEGORY(treelandUtils, "treeland.utils")

// Shortcut system
Q_LOGGING_CATEGORY(treelandShortcut, "treeland.shortcut")

// QML engine
Q_LOGGING_CATEGORY(treelandQml, "treeland.qml")

// Greeter module
Q_LOGGING_CATEGORY(treelandGreeter, "treeland.greeter")

// FPS display
Q_LOGGING_CATEGORY(treelandFpsDisplay, "treeland.fpsdisplay")

// xsettings
Q_LOGGING_CATEGORY(treelandXsettings, "treeland.xsettings")

void setupTreelandLogging(TreelandConfig *config)
{
    if (!config) {
        return;
    }

#if TREELANDCONFIG_DCONFIG_FILE_VERSION_MINOR > 0
    if (config->isInitializeSucceeded()) {
#else
    if (config->isInitializeSucceed()) {
#endif
        applyLoggingRules(config);
    } else {
        QObject::connect(
            config,
            &TreelandConfig::configInitializeSucceed,
            config,
            [config]() {
                applyLoggingRules(config);
            },
            Qt::SingleShotConnection);
    }

    QObject::connect(config, &TreelandConfig::logRulesChanged, config, [config]() {
        applyLoggingRules(config);
    });
}
