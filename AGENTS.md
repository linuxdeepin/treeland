# Treeland Agent Notes

## Big Picture
- `treeland` is a Wayland compositor built on `wlroots` + Qt6/QtQuick (Scene Graph rendering).
- `qwlroots/` provides Qt-style C++ bindings for `wlroots`.
- `waylib/` builds on `wlroots`/`qwlroots` to provide a QtQuick-friendly compositor framework (outputs <-> `QQuickWindow`, surfaces <-> `QQuickItem`).
- Treeland-specific policies live in `src/` (workspace/seat/input/output, effects/plugins, XWayland integration).

## Where to Look
- `src/core/`: lifecycle + QML engine glue (for example `src/core/qmlengine.*`, `src/core/treeland.*`).
- `src/surface/`: surface/window wrappers, state/visibility/geometry (for example `src/surface/surfacewrapper.cpp`).
- `src/workspace/`: workspace model and switching.
- `src/plugins/` + `src/modules/`: integration points and feature composition.
- `src/common/treelandlogging.*`: centralized treeland logging categories.

## Build & Test
```bash
cmake --preset default
cmake --build --preset default
ctest --test-dir build --output-on-failure
```

- Presets in `CMakePresets.json`: `default`, `clang`, `ci`, `ci-clang`.
- `WITH_SUBMODULE_WAYLIB=ON|OFF` selects submodule vs system waylib. Default preference is `ON`.

## Project Rules
- Use C++20 and Qt6 only.
- Keep diffs minimal: touch only task-related files and symbols.
- Avoid broad refactors, unrelated renames, and unrelated reformatting.
- New source files must include an SPDX line consistent with this repo, typically `SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only`.
- Logging changes must use `.agents/skills/logging-guidelines/SKILL.md`.
- If real task work shows that a repo-local skill is wrong, outdated, or incomplete, update the skill proactively instead of preserving known-bad guidance.

## Wayland Protocol Routing
When a task is about adding, modifying, or debugging a Wayland protocol integration, route it to exactly one of these skills:

- Treeland private protocol: `.agents/skills/treeland-private-wayland-protocol/SKILL.md`
- Upstream wlroots/waylib protocol: `.agents/skills/upstream-wayland-protocol-wrapper/SKILL.md`

Do not mix the two paths in one implementation unless the task is explicitly about refactoring boundaries.
