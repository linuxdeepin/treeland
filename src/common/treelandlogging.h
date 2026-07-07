// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef TREELAND_LOGGING_H
#define TREELAND_LOGGING_H

#include <QLoggingCategory>

// TreeLand Logging Categories
// Naming convention: lcTl + PascalCase module name
// String ID convention: treeland.<module>[.<submodule>]

// Core modules
Q_DECLARE_LOGGING_CATEGORY(lcTlCore)
Q_DECLARE_LOGGING_CATEGORY(lcTlServer)
Q_DECLARE_LOGGING_CATEGORY(lcTlCompositor)
Q_DECLARE_LOGGING_CATEGORY(lcTlShell)
Q_DECLARE_LOGGING_CATEGORY(lcTlQml)

// Input modules
Q_DECLARE_LOGGING_CATEGORY(lcTlInput)
Q_DECLARE_LOGGING_CATEGORY(lcTlInputManager)
Q_DECLARE_LOGGING_CATEGORY(lcTlGestures)
Q_DECLARE_LOGGING_CATEGORY(lcTlKeyboardNotify)

// Seat management
Q_DECLARE_LOGGING_CATEGORY(lcTlSeat)

// Output module
Q_DECLARE_LOGGING_CATEGORY(lcTlOutput)

// Window management
Q_DECLARE_LOGGING_CATEGORY(lcTlSurface)

// Workspace management
Q_DECLARE_LOGGING_CATEGORY(lcTlWorkspace)

// Protocol module
Q_DECLARE_LOGGING_CATEGORY(lcTlProtocol)

// Plugin system
Q_DECLARE_LOGGING_CATEGORY(lcTlPlugin)

// Configuration management
Q_DECLARE_LOGGING_CATEGORY(lcTlConfig)

// Wallpaper system
Q_DECLARE_LOGGING_CATEGORY(lcTlWallpaper)
Q_DECLARE_LOGGING_CATEGORY(lcTlWallpaperColor)

// Effects system
Q_DECLARE_LOGGING_CATEGORY(lcTlEffect)

// Capture system
Q_DECLARE_LOGGING_CATEGORY(lcTlCapture)

// DBus interface
Q_DECLARE_LOGGING_CATEGORY(lcTlDBus)

// Utility classes
Q_DECLARE_LOGGING_CATEGORY(lcTlUtils)
Q_DECLARE_LOGGING_CATEGORY(lcTlPropertyMonitor)

// Shortcut system
Q_DECLARE_LOGGING_CATEGORY(lcTlShortcut)

// Greeter module
Q_DECLARE_LOGGING_CATEGORY(lcTlGreeter)

// FPS display
Q_DECLARE_LOGGING_CATEGORY(lcTlFpsDisplay)

// xsettings
Q_DECLARE_LOGGING_CATEGORY(lcTlXsettings)

// Activation module
Q_DECLARE_LOGGING_CATEGORY(lcTlActivation)

// App ID resolver
Q_DECLARE_LOGGING_CATEGORY(lcTlAppIdResolver)

// Prelaunch splash
Q_DECLARE_LOGGING_CATEGORY(lcTlPrelaunchSplash)

// XWayland
Q_DECLARE_LOGGING_CATEGORY(lcTlXwayland)

// DDM integration
Q_DECLARE_LOGGING_CATEGORY(lcTlDdm)

// Popup focus management
Q_DECLARE_LOGGING_CATEGORY(lcTlPopupFocus)

#endif // TREELAND_LOGGING_H
