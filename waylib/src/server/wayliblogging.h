// Copyright (C) 2023-2026 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef WAYLIB_LOGGING_H
#define WAYLIB_LOGGING_H

#include <QLoggingCategory>

// Waylib Logging Categories
// Naming convention: lcWl + PascalCase module name
// String ID convention: waylib.<module>[.<submodule>] or waylib.protocols.<name> or waylib.qtquick.<name> or waylib.utils.<name>

// Kernel - Core
Q_DECLARE_LOGGING_CATEGORY(lcWlSocket)
Q_DECLARE_LOGGING_CATEGORY(lcWlSeat)
Q_DECLARE_LOGGING_CATEGORY(lcWlOutput)
Q_DECLARE_LOGGING_CATEGORY(lcWlOutputDrm)
Q_DECLARE_LOGGING_CATEGORY(lcWlOutputBuffer)
Q_DECLARE_LOGGING_CATEGORY(lcWlInput)
Q_DECLARE_LOGGING_CATEGORY(lcWlCursor)

// Kernel - Input subcategories
Q_DECLARE_LOGGING_CATEGORY(lcWlPointer)
Q_DECLARE_LOGGING_CATEGORY(lcWlTouch)
Q_DECLARE_LOGGING_CATEGORY(lcWlGesture)
Q_DECLARE_LOGGING_CATEGORY(lcWlDrag)

// QtQuick
Q_DECLARE_LOGGING_CATEGORY(lcWlTextureProvider)
Q_DECLARE_LOGGING_CATEGORY(lcWlSurface)
Q_DECLARE_LOGGING_CATEGORY(lcWlQtQuickTexture)
Q_DECLARE_LOGGING_CATEGORY(lcWlRenderer)
Q_DECLARE_LOGGING_CATEGORY(lcWlBufferItem)

// Utils
Q_DECLARE_LOGGING_CATEGORY(lcWlImageCapture)
Q_DECLARE_LOGGING_CATEGORY(lcWlCursorImage)
Q_DECLARE_LOGGING_CATEGORY(lcWlBufferDumper)

// Thread utilities
Q_DECLARE_LOGGING_CATEGORY(lcWlThreadUtils)

// Protocols
Q_DECLARE_LOGGING_CATEGORY(lcWlForeignToplevel)
Q_DECLARE_LOGGING_CATEGORY(lcWlInputMethod)
Q_DECLARE_LOGGING_CATEGORY(lcWlExtForeignToplevel)
Q_DECLARE_LOGGING_CATEGORY(lcWlTextInput)

// Layer shell
Q_DECLARE_LOGGING_CATEGORY(lcWlLayerShell)
Q_DECLARE_LOGGING_CATEGORY(lcWlLayerSurface)

// XDG
Q_DECLARE_LOGGING_CATEGORY(lcWlXdgOutput)
Q_DECLARE_LOGGING_CATEGORY(lcWlXdgDecoration)
Q_DECLARE_LOGGING_CATEGORY(lcWlXdgDialog)

// Output
Q_DECLARE_LOGGING_CATEGORY(lcWlOutputLayer)
Q_DECLARE_LOGGING_CATEGORY(lcWlOutputHelper)

// QtQuick extras
Q_DECLARE_LOGGING_CATEGORY(lcWlQmlCreator)
Q_DECLARE_LOGGING_CATEGORY(lcWlQuickCursor)

// Platform & Rendering
Q_DECLARE_LOGGING_CATEGORY(lcWlPlatform)
Q_DECLARE_LOGGING_CATEGORY(lcWlRenderHelper)
Q_DECLARE_LOGGING_CATEGORY(lcWlRenderBuffer)
Q_DECLARE_LOGGING_CATEGORY(lcWlBufferRenderer)

#endif // WAYLIB_LOGGING_H
