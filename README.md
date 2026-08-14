# Treeland

treeland is a wayland compositor based on wlroots and QtQuick, designed to provide efficient and flexible graphical interface support.

## Dependencies

Check the `debian/control` file to understand specific build and runtime dependencies, or use `cmake` to check for missing necessary components.

Core build dependencies:

- [waylib](https://github.com/vioken/waylib): A Wayland compositor development library based on wlroots and QtQuick
  - Qt >= 6.8.0
  - wlroots = 0.19
- [treeland-protocols](https://github.com/linuxdeepin/treeland-protocols): Private Wayland protocols used by treeland

Recommended runtime dependencies:

- [ddm](https://github.com/linuxdeepin/ddm): A display manager optimized for multiple users

## Building

Treeland uses cmake for building. The WITH_SUBMODULE_WAYLIB option can force the use of the waylib code from the submodule. If you want to use the system-provided waylib, set this option to OFF.

Using the system-provided waylib:

```shell
$ git clone git@github.com:linuxdeepin/treeland.git
$ cd treeland
$ cmake -Bbuild -DWITH_SUBMODULE_WAYLIB=OFF
$ cmake --build build
```

Using the waylib from the submodule:

```shell
$ git clone git@github.com:linuxdeepin/treeland.git --recursive
$ cd treeland
$ cmake -Bbuild -DWITH_SUBMODULE_WAYLIB=ON
$ cmake --build build
```

## Packaging

A `debian` folder is provided to build the package under the *deepin* linux desktop distribution. To build the package, use the following command:

```shell
$ sudo apt build-dep . # install build dependencies
$ dpkg-buildpackage -uc -us -nc -b # build binary package(s)
```

## treeland-debug

`treeland-debug` is an adb-style command-line inspector and controller for the
running Treeland compositor. It connects to Treeland's debug Remote Object over
Qt Remote Objects and exposes subcommands for inspecting the window tree,
controlling windows, injecting input events and grabbing screenshots. Like `adb`,
it runs in two modes:

- **Non-shell (one-shot) mode** — the default: `treeland-debug <command> [args]`.
- **Shell mode** — an interactive REPL: `treeland-debug shell`.

The client builds as part of the normal Treeland build (it is on by default; pass
`-DBUILD_TREELAND_DEBUG=OFF` to skip it):

```bash
cmake -B build
cmake --build build --target treeland-debug
sudo cmake --install build --component treeland-debug
```

### Enabling the debug source

The debug Remote Object is opt-in. Enable the `debugSource` DConfig option as the
`dde` user (Treeland runs as `dde` in global mode and its local socket is
owner-only), then restart Treeland:

```bash
sudo -u dde -- dde-dconfig set \
  -a org.deepin.dde.treeland \
  -r org.deepin.dde.treeland \
  -k debugSource \
  -v true
```

All commands below are run as the `dde` user, e.g.
`sudo -u dde -- treeland-debug windows`.

### Global options

| Option | Default | Description |
| --- | --- | --- |
| `--url <url>` | `local:org.deepin.dde.treeland.debug` | Remote object host URL. |
| `--name <name>` | `WindowTree` | Remote object name. |
| `--timeout-ms <n>` | `30000` | Request timeout in ms (non-negative integer). |
| `--json` | off | Emit machine-readable JSON for `windows`/`clients`. |
| `-h, --help` | — | Show help. |
| `-v, --version` | — | Show version. |
| `--tree` / `--cursor` | — | Backward-compatible aliases for the `tree` / `cursor` commands. |

### Command reference

Window-control commands accept a target given by **numeric `id`** (printed by
`windows` / `clients` / `top`) or by **`appId`** (the first matching window is
used).

#### Inspection

| Command | Arguments | Output |
| --- | --- | --- |
| `tree` | _(none, default)_ | JSON — the complete layout tree. |
| `cursor` | _(none)_ | JSON `{"x","y"}` — cursor position. |
| `windows` | _(none)_ | Window table; `--json` for a JSON array. |
| `clients` | _(none)_ | Client + window table; `--json` for a JSON array. |
| `top` | `[interval-ms]` (default 1000) | Live, `top`-like refreshing client view (Ctrl+C to quit). |

#### Window control

| Command | Arguments | Output |
| --- | --- | --- |
| `activate` | `<id>` | `ok` / `failed`. |
| `close` | `<id>` | `ok` / `failed`. |
| `minimize` | `<id>` | `ok` / `failed`. |
| `maximize` | `<id>` | `ok` / `failed` (toggles maximized). |
| `fullscreen` | `<id>` | `ok` / `failed` (toggles fullscreen). |
| `move` | `<id> <x> <y>` | `ok` / `failed`. |
| `resize` | `<id> <w> <h>` | `ok` / `failed`. |
| `workspace` | `<id> <ws-id>` | `ok` / `failed` (move window to a workspace). |

#### Input / event injection

| Command | Arguments | Output |
| --- | --- | --- |
| `move-cursor` | `<x> <y>` | `ok` / `failed`. |
| `event motion` | `<x> <y>` | `ok` / `failed`. |
| `event button` | `<left\|right\|middle\|code> [press\|release\|click]` | `ok` / `failed` (default `click`). |
| `event key` | `<name\|evdev-code> [press\|release\|tap]` | `ok` / `failed` (default `tap`). |

Pointer buttons use Linux input codes (`left`=0x110, `right`=0x111,
`middle`=0x112, or a numeric code). Keyboard keys use Linux evdev keycodes;
common names are recognised (`esc`, `enter`, `space`, `tab`, `a`–`z`, `0`–`9`,
arrow keys, `f1`–`f12`, `home`/`end`/`pageup`/`pagedown`/`insert`/`del`, …) or a
raw code may be passed. Keys are delivered to the keyboard-focused surface —
`activate` a window first to target it; pointer buttons go to the surface under
the cursor.

#### Image capture

Screenshots are rendered server-side and written to a PNG; the file path is
printed. If `file` is omitted a path under `/tmp` is generated.

| Command | Arguments | Output |
| --- | --- | --- |
| `screenshot output` | `[name] [file]` | PNG file path (stdout). |
| `screenshot window` | `<id> [file]` | PNG file path (stdout). |
| `screenshot screen` | `[file]` | PNG file path (stdout). |

#### Interactive

| Command | Arguments | Output |
| --- | --- | --- |
| `shell` | _(none)_ | REPL (`treeland>`) accepting all commands plus `help`/`exit`. |
| `help` | _(none)_ | Full help text. |

### Output formats

`tree` and `cursor` always print JSON. `windows` and `clients` print a
human-readable table by default and a JSON array with `--json`.

Window JSON object (`WindowInfo`):

| Field | Type | Notes |
| --- | --- | --- |
| `id` | integer | Stable window id; accepted by control commands. |
| `appId` | string | Application id. |
| `title` | string | Window title. |
| `output` | string | Output name. |
| `container` | string | Container name. |
| `workspace` | integer | Workspace id. |
| `layer` | integer | Layer id. |
| `z` | integer | Z order. |
| `type` | integer | Window type. |
| `state` | integer | `0` Normal, `1` Maximized, `2` Minimized, `3` Fullscreen, `4` Tiling. |
| `visible` | boolean | Visibility. |
| `active` | boolean | Active / focused flag. |
| `geometry` | object | `{"x","y","width","height"}`. |
| `titlebarGeometry` | object | Same shape. |
| `boundingRect` | object | Same shape. |
| `iconGeometry` | object | Same shape. |
| `position` | object | `{"x","y"}`. |

Client JSON object (`ClientInfo`): `id` (integer), `pid` (integer, `0` when
unavailable), `executable` (string), `windows` (array of `WindowInfo`).

The `tree` JSON is
`{"currentMode": str, "layers": [{"name", "layer", "windows": [WindowInfo], "workspaces": [{"id", "isActive", "windows": [WindowInfo]}]}]}`.

### Exit codes

`0` on success; `1` on connection failure, RPC failure, an unknown command, or a
control command that returns `failed`.

## GitHub Actions / CI

This project uses GitHub Actions for continuous integration. The following workflows are configured:

- **waylib builds**: Triggered when `waylib/**` files are modified
- **treeland builds**: Main project builds

## Getting Involved

- [Code contribution via GitHub](https://github.com/linuxdeepin/treeland/)
- [Submit bug or suggestions to GitHub Issues or GitHub Discussions](https://github.com/linuxdeepin/developer-center/issues/new/choose)

## License

treeland is licensed under Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only.
