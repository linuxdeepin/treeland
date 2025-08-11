// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef TREELAND_LOGGING_H
#define TREELAND_LOGGING_H

#include <QLoggingCategory>

// TreeLand Logging Categories
// Naming convention: treeland.module_name.submodule_name
// Use lowercase letters, separated by dots

// Core modules
Q_DECLARE_LOGGING_CATEGORY(treelandCore)
Q_DECLARE_LOGGING_CATEGORY(treelandServer)
Q_DECLARE_LOGGING_CATEGORY(treelandCompositor)
Q_DECLARE_LOGGING_CATEGORY(treelandShell)

// Input modules
Q_DECLARE_LOGGING_CATEGORY(treelandInput)
Q_DECLARE_LOGGING_CATEGORY(treelandGestures)

// Output module
Q_DECLARE_LOGGING_CATEGORY(treelandOutput)

// Window management
Q_DECLARE_LOGGING_CATEGORY(treelandSurface)

// Protocol module
Q_DECLARE_LOGGING_CATEGORY(treelandProtocol)

// Plugin system
Q_DECLARE_LOGGING_CATEGORY(treelandPlugin)

// Configuration management
Q_DECLARE_LOGGING_CATEGORY(treelandConfig)

// Workspace management
Q_DECLARE_LOGGING_CATEGORY(treelandWorkspace)

// Wallpaper system
Q_DECLARE_LOGGING_CATEGORY(treelandWallpaper)

// Effects system
Q_DECLARE_LOGGING_CATEGORY(treelandEffect)

// Capture system
Q_DECLARE_LOGGING_CATEGORY(treelandCapture)

// DBus interface
Q_DECLARE_LOGGING_CATEGORY(treelandDBus)

// Utility classes
Q_DECLARE_LOGGING_CATEGORY(treelandUtils)

// Shortcut system
Q_DECLARE_LOGGING_CATEGORY(treelandShortcut)

// QML engine
Q_DECLARE_LOGGING_CATEGORY(treelandQml)

// Greeter module
Q_DECLARE_LOGGING_CATEGORY(treelandGreeter)

#endif // TREELAND_LOGGING_H
