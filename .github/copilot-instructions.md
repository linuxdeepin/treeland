# GitHub Copilot Repository Instructions for Treeland

## Big Picture
- `treeland` is a Wayland compositor built on `wlroots` + Qt6/QtQuick (Scene Graph rendering).
- `qwlroots/` (independent module) provides Qt-style C++ bindings for `wlroots`.
- `waylib/` (independent module) builds on `wlroots`/`qwlroots` to provide a QtQuick-friendly compositor framework (outputs <-> `QQuickWindow`, surfaces <-> `QQuickItem`).
- Treeland-specific policies live in `src/` (workspace/seat/input/output, effects/plugins, XWayland integration).

## Where to Look
- `src/core/`: lifecycle + QML engine glue (e.g., `src/core/qmlengine.*`, `src/core/treeland.*`).
- `src/surface/`: surface/window wrappers, state/visibility/geometry (e.g., `src/surface/surfacewrapper.cpp`).
- `src/workspace/`: workspace model and switching.
- `src/plugins/` + `src/modules/`: integration points and feature composition.
- `src/common/treelandlogging.*`: centralized treeland logging categories.

## Build & Test (CMake presets)
```bash
cmake --preset default
cmake --build --preset default
ctest --test-dir build --output-on-failure
```
- Presets in `CMakePresets.json`: `default`, `clang`, `ci`, `deb`.
- `WITH_SUBMODULE_WAYLIB=ON|OFF` selects submodule vs system waylib (default preference: `ON`; see `README.md`).

## Project Rules (must-follow)
- Use C++20; Qt6 only (do not introduce Qt5 APIs/modules).
- Keep diffs minimal: touch only task-related files/symbols; avoid broad refactors/renames and avoid reformatting unrelated code.
- New files must include an SPDX line consistent with this repo (typically `SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only`).
- Logging changes must follow `docs/logging-guidelines.md` (treeland: centralized categories; waylib: per-file `Q_LOGGING_CATEGORY` with `waylib.*` names).
- For English reviews, append a Chinese translation after the English content.
- Terminal commands in fenced `bash` blocks; paths/symbols in backticks.
