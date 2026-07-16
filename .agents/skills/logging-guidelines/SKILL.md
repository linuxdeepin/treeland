---
name: logging-guidelines
description: Use this skill whenever a task adds, changes, reviews, or removes logging in treeland, waylib, or qwlroots code. Trigger on logging categories, Q_LOGGING_CATEGORY, Q_DECLARE_LOGGING_CATEGORY, qCDebug, qCInfo, qCWarning, qCCritical, log level selection, category naming, or requests to clean up log messages. For treeland code, prefer centralized categories in src/common/treelandlogging.h/.cpp. For waylib code, prefer centralized categories in waylib/src/server/wayliblogging.h/.cpp.
---

# Logging Guidelines

## Goal
Keep logging changes consistent with the centralized category system:

- **treeland**: centralized categories in `src/common/treelandlogging.h/.cpp`
- **waylib**: centralized categories in `waylib/src/server/wayliblogging.h/.cpp`

Use this skill for both implementation work and review work.

## Routing
Choose the logging path by module ownership, not by personal preference.

### Treeland code
Use centralized categories declared in:

- `src/common/treelandlogging.h`
- `src/common/treelandlogging.cpp`

Category variable naming: `lcTl` + PascalCase module name (e.g. `lcTlCore`, `lcTlInput`, `lcTlGestures`).

String ID naming: `treeland.<module>[.<submodule>]` (e.g. `treeland.core`, `treeland.input.gestures`).

The only exceptions are standalone executables that cannot include the centralized header (e.g. `treeland-xwayland`, `treeland-sd`, `treeland-shortcut`, `treeland-screensaver`). These may define local `Q_LOGGING_CATEGORY` in their own `.cpp` file.

### Waylib code
Use centralized categories declared in:

- `waylib/src/server/wayliblogging.h`
- `waylib/src/server/wayliblogging.cpp`

Category variable naming: `lcWl` + PascalCase module name (e.g. `lcWlSeat`, `lcWlOutput`, `lcWlCursor`).

String ID naming: `waylib.<module>[.<submodule>]` or `waylib.protocols.<name>` or `waylib.qtquick.<name>` or `waylib.utils.<name>` (e.g. `waylib.seat`, `waylib.input.pointer`, `waylib.protocols.inputmethod`).

## Waylib Category Hierarchy

```
waylib
├── Kernel
│   ├── waylib.socket               lcWlSocket
│   ├── waylib.seat                  lcWlSeat
│   ├── waylib.output                lcWlOutput
│   ├── waylib.output.drm            lcWlOutputDrm
│   ├── waylib.output.buffer         lcWlOutputBuffer
│   ├── waylib.input                 lcWlInput
│   ├── waylib.cursor                lcWlCursor
│   ├── waylib.input.pointer         lcWlPointer
│   ├── waylib.input.touch           lcWlTouch
│   ├── waylib.input.gesture         lcWlGesture
│   └── waylib.input.drag            lcWlDrag
├── QtQuick
│   ├── waylib.texture.provider      lcWlTextureProvider
│   ├── waylib.qtquick.surface       lcWlSurface
│   ├── waylib.qtquick.texture       lcWlQtQuickTexture
│   ├── waylib.renderer              lcWlRenderer
│   └── waylib.qtquick.bufferitem    lcWlBufferItem
├── Utils
│   ├── waylib.utils.imagecapture    lcWlImageCapture
│   ├── waylib.utils.cursorimage     lcWlCursorImage
│   ├── waylib.utils.bufferdumper    lcWlBufferDumper
│   └── waylib.utils.thread          lcWlThreadUtils
├── Protocols
│   ├── waylib.protocols.foreigntoplevel       lcWlForeignToplevel
│   ├── waylib.protocols.inputmethod           lcWlInputMethod
│   ├── waylib.protocols.extforeigntoplevel    lcWlExtForeignToplevel
│   └── waylib.protocols.textinput             lcWlTextInput
├── Layer shell
│   ├── waylib.layer.shell          lcWlLayerShell
│   └── waylib.layer.surface        lcWlLayerSurface
├── XDG
│   ├── waylib.xdg.output           lcWlXdgOutput
│   ├── waylib.xdg.decoration       lcWlXdgDecoration
│   └── waylib.xdg.dialog           lcWlXdgDialog
├── Output extras
│   ├── waylib.output.layer         lcWlOutputLayer
│   └── waylib.output.helper        lcWlOutputHelper
├── QtQuick extras
│   ├── waylib.qml.creator          lcWlQmlCreator
│   └── waylib.cursor.quick         lcWlQuickCursor
└── Platform & Rendering
    ├── waylib.platform              lcWlPlatform
    ├── waylib.render.helper         lcWlRenderHelper
    ├── waylib.render.buffer         lcWlRenderBuffer
    └── waylib.render.bufferrenderer lcWlBufferRenderer
```

## Mandatory Rules

1. **No uncategorized logging** — Never use bare `qDebug()`, `qWarning()`, `qCritical()`, or `qInfo()`. Always use the categorized variants: `qCDebug(cat)`, `qCInfo(cat)`, `qCWarning(cat)`, `qCCritical(cat)`.

2. **No local Q_LOGGING_CATEGORY in library code** — All categories for treeland and waylib library code must be declared in the centralized `*logging.h/.cpp`. Only standalone executables (e.g. `treeland-xwayland`) may define local categories in their own `.cpp` file.

3. **No logging header includes in .h files** — Only `treelandlogging.h` / `wayliblogging.h` themselves may declare categories. Other `.h` files must not `#include` logging headers or use `qC*()` macros. Move log calls to `.cpp` instead.

4. **Every category must have a comment** — Each `Q_LOGGING_CATEGORY` definition must be documented. Two conventions are in use:
   - **Treeland** (`treelandlogging.cpp`): uses section-level `//` comments (e.g., `// Core modules`, `// Input modules`) to annotate groups of related categories.
   - **Waylib** (`wayliblogging.cpp`): uses per-category `//` inline comments on each definition, e.g.:
     ```cpp
     Q_LOGGING_CATEGORY(lcWlSeat, "waylib.seat", QtInfoMsg) // Seat management (keyboard focus, capability)
     ```
   When adding a new category to either file, follow the existing convention of that file.

## Log Levels
Choose the lowest level that still matches the operational importance.

- `qCDebug`: detailed debugging and troubleshooting
- `qCInfo`: normal operational information
- `qCWarning`: unexpected but recoverable situations
- `qCCritical`: serious failures requiring immediate attention

Do not upgrade routine noise to warning or critical just to make it visible.

## Message Quality
Write messages that are specific and filterable.

Prefer:

```cpp
qCDebug(lcWlCursor) << "Cursor moved from" << oldPos << "to" << newPos;
```

Avoid:

```cpp
qCDebug(lcWlCursor) << "Cursor moved";
```

When useful, include:

- the object or subsystem involved
- before/after state
- identifiers or names
- the impact of an error

## Error Logging
When logging failures, include enough context to debug the issue without reading surrounding code.

Prefer:

```cpp
qCWarning(lcWlCursor) << "Failed to attach device" << device->name()
                      << "- not a pointing device";
```

Avoid vague failure messages with no object, cause, or impact.

## Sensitive Data
Do not log secrets, passwords, tokens, or other sensitive user data.

## Adding a New Category

Follow these steps when you need to add a new logging category:

1. **Choose the naming** — Pick a variable name (`lcTl*` or `lcWl*` + PascalCase) and a string ID under the appropriate hierarchy.
2. **Add declaration** — Add `Q_DECLARE_LOGGING_CATEGORY(lcXxx)` to the centralized `.h` file (`treelandlogging.h` or `wayliblogging.h`) in the correct section.
3. **Add definition** — Add `Q_LOGGING_CATEGORY(lcXxx, "domain.path", level)` to the centralized `.cpp` file with a `//` comment explaining its scope.
4. **Include the header** — In the `.cpp` file where you use the category, add `#include "treelandlogging.h"` or `#include "wayliblogging.h"`.
5. **Migrate calls** — Replace bare `qDebug()`/`qWarning()` etc. with `qCDebug(lcXxx)`/`qCWarning(lcXxx)` etc.
6. **Verify** — Run `rg 'qWarning\(\)|qDebug\(\)|qCritical\(\)|qInfo\(\)' src/ waylib/src/` to confirm no uncategorized calls remain.

## Review Checklist
When reviewing a logging change, verify:

1. the category follows the correct `lcTl*`/`lcWl*` naming convention
2. the category is declared in the centralized header (not locally in library code)
3. no `.h` file (other than the logging header itself) includes a logging header or uses `qC*()`
4. each `Q_LOGGING_CATEGORY` definition has a `//` comment explaining its scope
5. the chosen log level matches severity
6. the message contains enough context
7. the change does not leak sensitive information
8. the change does not introduce bare `qDebug()`/`qWarning()`/`qCritical()`/`qInfo()` calls
9. the change does not introduce unrelated category churn

## Repository References
- `src/common/treelandlogging.h`
- `src/common/treelandlogging.cpp`
- `waylib/src/server/wayliblogging.h`
- `waylib/src/server/wayliblogging.cpp`
