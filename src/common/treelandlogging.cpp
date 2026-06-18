// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "common/treelandlogging.h"

// TreeLand logging category definitions
// Naming convention: lcTl + PascalCase module name
// String ID convention: treeland.<module>[.<submodule>]

// Core modules
Q_LOGGING_CATEGORY(lcTlCore, "treeland.core", QtInfoMsg)
Q_LOGGING_CATEGORY(lcTlServer, "treeland.server")
Q_LOGGING_CATEGORY(lcTlCompositor, "treeland.compositor")
Q_LOGGING_CATEGORY(lcTlShell, "treeland.shell")
Q_LOGGING_CATEGORY(lcTlQml, "treeland.qml")

// Input modules
Q_LOGGING_CATEGORY(lcTlInput, "treeland.input")
Q_LOGGING_CATEGORY(lcTlInputManager, "treeland.input.manager")
Q_LOGGING_CATEGORY(lcTlGestures, "treeland.input.gestures")
Q_LOGGING_CATEGORY(lcTlKeyboardNotify, "treeland.input.keyboard.state.notify")

// Seat management
Q_LOGGING_CATEGORY(lcTlSeat, "treeland.seat")

// Output module
Q_LOGGING_CATEGORY(lcTlOutput, "treeland.output", QtInfoMsg)

// Window management
Q_LOGGING_CATEGORY(lcTlSurface, "treeland.surface")

// Workspace management
Q_LOGGING_CATEGORY(lcTlWorkspace, "treeland.workspace")

// Protocol module
Q_LOGGING_CATEGORY(lcTlProtocol, "treeland.protocol")

// Plugin system
Q_LOGGING_CATEGORY(lcTlPlugin, "treeland.plugin")

// Configuration management
Q_LOGGING_CATEGORY(lcTlConfig, "treeland.config")

// Wallpaper system
Q_LOGGING_CATEGORY(lcTlWallpaper, "treeland.wallpaper")
Q_LOGGING_CATEGORY(lcTlWallpaperColor, "treeland.wallpaper.color")

// Effects system
Q_LOGGING_CATEGORY(lcTlEffect, "treeland.effect")

// Capture system
Q_LOGGING_CATEGORY(lcTlCapture, "treeland.capture")

// DBus interface
Q_LOGGING_CATEGORY(lcTlDBus, "treeland.dbus")

// Utility classes
Q_LOGGING_CATEGORY(lcTlUtils, "treeland.utils")
Q_LOGGING_CATEGORY(lcTlPropertyMonitor, "treeland.property.monitor")

// Shortcut system
Q_LOGGING_CATEGORY(lcTlShortcut, "treeland.shortcut")

// Greeter module
Q_LOGGING_CATEGORY(lcTlGreeter, "treeland.greeter")

// FPS display
Q_LOGGING_CATEGORY(lcTlFpsDisplay, "treeland.fps")

// xsettings
Q_LOGGING_CATEGORY(lcTlXsettings, "treeland.xsettings")

// Activation module
Q_LOGGING_CATEGORY(lcTlActivation, "treeland.activation")

// App ID resolver
Q_LOGGING_CATEGORY(lcTlAppIdResolver, "treeland.appid.resolver", QtInfoMsg)

// Prelaunch splash
Q_LOGGING_CATEGORY(lcTlPrelaunchSplash, "treeland.prelaunch.splash", QtInfoMsg)

// XWayland
Q_LOGGING_CATEGORY(lcTlXwayland, "treeland.xwayland")

// DDM integration
Q_LOGGING_CATEGORY(lcTlDdm, "treeland.ddm")

// XdgDialog module
Q_LOGGING_CATEGORY(lcTlXdgDialog, "treeland.protocol.xdg-dialog")
