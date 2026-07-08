// Copyright (C) 2023-2026 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wayliblogging.h"

// Waylib logging category definitions
// Naming convention: lcWl + PascalCase module name
// String ID convention: waylib.<module>[.<submodule>] or waylib.protocols.<name> or waylib.qtquick.<name> or waylib.utils.<name>

// Kernel - Core
Q_LOGGING_CATEGORY(lcWlSocket, "waylib.socket", QtInfoMsg)             // Wayland socket lifecycle
Q_LOGGING_CATEGORY(lcWlSeat, "waylib.seat", QtInfoMsg)                 // Seat management (keyboard focus, capability)
Q_LOGGING_CATEGORY(lcWlOutput, "waylib.output", QtInfoMsg)             // Output lifecycle and configuration
Q_LOGGING_CATEGORY(lcWlOutputDrm, "waylib.output.drm", QtInfoMsg)     // DRM/KMS format negotiation and hardware rendering
Q_LOGGING_CATEGORY(lcWlOutputBuffer, "waylib.output.buffer", QtDebugMsg) // Output buffer commit and swap
Q_LOGGING_CATEGORY(lcWlInput, "waylib.input", QtInfoMsg)               // Generic input device events
Q_LOGGING_CATEGORY(lcWlCursor, "waylib.cursor", QtInfoMsg)             // Cursor image and theme management

// Kernel - Input subcategories
Q_LOGGING_CATEGORY(lcWlPointer, "waylib.input.pointer", QtInfoMsg)     // Pointer/mouse button and motion events
Q_LOGGING_CATEGORY(lcWlTouch, "waylib.input.touch", QtInfoMsg)         // Touch down/move/up/cancel events
Q_LOGGING_CATEGORY(lcWlGesture, "waylib.input.gesture", QtInfoMsg)     // Touchpad gesture begin/update/end and hold
Q_LOGGING_CATEGORY(lcWlDrag, "waylib.input.drag", QtInfoMsg)           // DnD start/motion/drop operations

// QtQuick
Q_LOGGING_CATEGORY(lcWlTextureProvider, "waylib.texture.provider")     // QSGTexture provider bridging
Q_LOGGING_CATEGORY(lcWlSurface, "waylib.qtquick.surface", QtInfoMsg)   // QtQuick surface item lifecycle and rendering
// More verbose in debug builds for texture inspection
#ifdef QT_DEBUG
Q_LOGGING_CATEGORY(lcWlQtQuickTexture, "waylib.qtquick.texture", QtDebugMsg)
#else
Q_LOGGING_CATEGORY(lcWlQtQuickTexture, "waylib.qtquick.texture", QtInfoMsg)
#endif
// More verbose in debug builds for render diagnostics
#ifdef QT_DEBUG
Q_LOGGING_CATEGORY(lcWlRenderer, "waylib.renderer", QtDebugMsg)
#else
Q_LOGGING_CATEGORY(lcWlRenderer, "waylib.renderer", QtWarningMsg)
#endif
Q_LOGGING_CATEGORY(lcWlBufferItem, "waylib.qtquick.bufferitem", QtInfoMsg) // Buffer item commit and damage

// Utils
Q_LOGGING_CATEGORY(lcWlImageCapture, "waylib.utils.imagecapture")      // Ext-image-capture-source frame capture
Q_LOGGING_CATEGORY(lcWlCursorImage, "waylib.utils.cursorimage", QtWarningMsg) // Cursor image scaling and conversion
Q_LOGGING_CATEGORY(lcWlBufferDumper, "waylib.utils.bufferdumper", QtWarningMsg) // Buffer dump for debugging
Q_LOGGING_CATEGORY(lcWlThreadUtils, "waylib.utils.thread", QtWarningMsg) // Thread utility async call and deadlock warning

// Protocols
Q_LOGGING_CATEGORY(lcWlForeignToplevel, "waylib.protocols.foreigntoplevel", QtWarningMsg) // wlr-foreign-toplevel-v1
Q_LOGGING_CATEGORY(lcWlInputMethod, "waylib.protocols.inputmethod", QtInfoMsg) // Input method v2 and virtual keyboard
Q_LOGGING_CATEGORY(lcWlExtForeignToplevel, "waylib.protocols.extforeigntoplevel", QtWarningMsg) // ext-foreign-toplevel-list-v1
Q_LOGGING_CATEGORY(lcWlTextInput, "waylib.protocols.textinput", QtInfoMsg) // Text input v1/v2/v3 protocol
Q_LOGGING_CATEGORY(lcWlLinuxDmabuf, "waylib.protocols.linuxdmabuf", QtInfoMsg) // Renderer-backed linux-dmabuf global
Q_LOGGING_CATEGORY(lcWlPresentation, "waylib.protocols.presentation", QtInfoMsg) // Presentation-time feedback

// Layer shell
Q_LOGGING_CATEGORY(lcWlLayerShell, "waylib.layer.shell", QtWarningMsg) // wlr-layer-shell surface requests
Q_LOGGING_CATEGORY(lcWlLayerSurface, "waylib.layer.surface", QtWarningMsg) // Layer surface commit/geometry

// XDG
Q_LOGGING_CATEGORY(lcWlXdgOutput, "waylib.xdg.output", QtWarningMsg)  // XDG output manager
Q_LOGGING_CATEGORY(lcWlXdgDecoration, "waylib.xdg.decoration", QtWarningMsg) // XDG decoration mode
Q_LOGGING_CATEGORY(lcWlXdgDialog, "waylib.xdg.dialog", QtWarningMsg) // xdg-dialog-v1 modal state

// Output
Q_LOGGING_CATEGORY(lcWlOutputLayer, "waylib.output.layer", QtWarningMsg)   // QtQuick output layer
Q_LOGGING_CATEGORY(lcWlOutputHelper, "waylib.output.helper", QtWarningMsg) // Output helper state/commit

// QtQuick extras
Q_LOGGING_CATEGORY(lcWlQmlCreator, "waylib.qml.creator", QtWarningMsg)     // QML component creation
Q_LOGGING_CATEGORY(lcWlQuickCursor, "waylib.cursor.quick", QtWarningMsg)   // QtQuick cursor texture provider

// Platform & Rendering
Q_LOGGING_CATEGORY(lcWlPlatform, "waylib.platform", QtWarningMsg)          // QPA integration
Q_LOGGING_CATEGORY(lcWlRenderHelper, "waylib.render.helper", QtWarningMsg) // Render target creation and buffer import
Q_LOGGING_CATEGORY(lcWlRenderBuffer, "waylib.render.buffer", QtWarningMsg) // Render buffer node DMA-BUF
Q_LOGGING_CATEGORY(lcWlBufferRenderer, "waylib.render.bufferrenderer", QtWarningMsg) // Buffer renderer texture provider
