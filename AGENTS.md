# Repository Guidelines

## Project Structure & Module Organization

Treeland is a CMake-based C++/Qt Wayland compositor. Core code lives in
`src/`, with major areas split into `core/`, `input/`, `output/`, `surface/`,
`workspace/`, `modules/`, and `plugins/`. Wayland protocol definitions are in
`protocols/`; runtime resources, D-Bus interfaces, dconfig files, shaders, icons, and
systemd/session files are under `misc/`. Test clients and manual demos live in
`examples/`, while automated protocol tests are in `tests/`. In-tree dependencies are
kept in `waylib/` and `qwlroots/`; touch them only when the change is intentionally
within those components.

## Build, Test, and Development Commands

- `cmake -B build -DWITH_SUBMODULE_WAYLIB=ON` configures with bundled `waylib` and
  `qwlroots`.
- `cmake -B build -DWITH_SUBMODULE_WAYLIB=OFF` configures against system-provided
  Waylib.
- `cmake --build build` builds Treeland and enabled subdirectories.
- `ctest --test-dir build --output-on-failure` runs registered CTest tests.
- `cmake -B build -DBUILD_TREELAND_EXAMPLES=ON` also builds example clients.
- `dpkg-buildpackage -uc -us -nc -b` builds Debian packages after installing build
  dependencies with `sudo apt build-dep .`.

## Coding Style & Naming Conventions

Use C++20 and C23 as configured in `CMakeLists.txt`. Follow `.editorconfig`: UTF-8,
LF endings, final newline, spaces, and 4-space indentation except YAML uses 2 spaces
and Makefiles use tabs. Format C/C++ changes with the repository `.clang-format`
(Qt/WebKit-derived style, 100-column limit, right-aligned pointers, grouped includes).
Prefer existing Qt naming patterns: classes in `UpperCamelCase`, functions and
variables in `lowerCamelCase`, and CMake options in `UPPER_SNAKE_CASE`.

## Testing Guidelines

Add protocol or behavior tests under `tests/test_<area>/` with a local
`CMakeLists.txt`, then register them from `tests/CMakeLists.txt`. Keep names
descriptive, following existing patterns such as
`test_protocol_shortcut` and `test_protocol_window-management`. Run `ctest` from the
configured build tree before submitting changes; for UI or compositor behavior, add or
update an `examples/test_*` client when it helps reproduce the scenario.

## Commit & Pull Request Guidelines

Recent history uses Conventional Commit-style subjects such as `fix: ...`,
`feat: ...`, and `refactor: ...`; scope each commit to one logical change. Pull
requests should describe behavior changes, list validation commands, and link related
issues. Include screenshots or short recordings for visible UI, shell, wallpaper,
lockscreen, or multitask-view changes. CI runs commit lint, cppcheck, and component
builds for Treeland, Waylib, and qwlroots.

## Security & Configuration Tips

Do not commit local build trees, generated packages, or machine-specific session
configuration. Treat files in `misc/dbus`, `misc/systemd`, `misc/dconfig`, and
`protocols` as public integration contracts; document compatibility effects when
changing them.
