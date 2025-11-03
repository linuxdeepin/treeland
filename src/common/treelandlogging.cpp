// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "common/treelandlogging.h"

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
