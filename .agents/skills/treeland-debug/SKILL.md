---
name: treeland-debug
description: Use this skill whenever a task asks you to debug, diagnose, reproduce, or analyze a problem in the running treeland compositor — window/workspace/input/output/rendering issues, crashes, hangs, focus problems, or requests to inspect live state. Trigger on `treeland-debug`, `remoteDebug`, `WindowTree` Remote Object, `QT_LOGGING_RULES`, treeland logging categories, `journalctl` for treeland, `org.deepin.Compositor1`, window tree inspection, input event injection, or screenshot capture from a live compositor. Also trigger on access-control topics: `pkexec`, `polkit`, `--no-escalate`, `root` privileges for treeland-debug. Pair with the systematic-debugging approach — reproduce, isolate, then fix at the root cause.
---

# Treeland Debug

## Scope
This skill tells you how to inspect and control a **running** treeland compositor to debug it. Use it when the problem is observable at runtime (wrong window state, focus/activation, input not landing, missing/blank output, crashes, hangs, performance). For build/test problems see `AGENTS.md` Build & Test; for protocol integration see the protocol skills; for DConfig see `treeland-dconfig-configuration`.

Primary repos and where things live:

- `src/core/` — lifecycle, QML engine glue (`src/core/qmlengine.*`, `src/core/treeland.*`).
- `src/surface/` — surface/window wrappers, state, visibility, geometry (`surfacewrapper.cpp`).
- `src/seat/` — seat management, `Helper` initialization hub, protocol wiring (`seat/helper.*`).
- `src/workspace/` — workspace model and switching.
- `src/modules/` — feature modules (window-management, input-manager, output-manager, dde-shell, …).
- `src/plugins/` + `src/effects/` — plugin & effect integration.
- `src/common/treelandlogging.*` — centralized logging categories.
- `waylib/` — the wlroots+QtQuick compositor framework underneath (outputs <-> `QQuickWindow`, surfaces <-> `QQuickItem`).
- `wlroots/` + `3rdparty/wlroots/` — vendored wlroots (raw C API via `<wlr_all.h>`).
- `tools/treeland-debug/` — the CLI you use to inspect/control the live compositor.

## Runtime context
treeland can run as a shipped service or as an in-development build:

- **Global mode** (preferred, on DDM): one process manages all users; it runs as the **`dde`** user via the systemd **system** unit `treeland.service` (`User=dde`, installed under `SYSTEMD_SYSTEM_UNIT_DIR`), bus name `org.deepin.Compositor1`. `ExecStart=.../treeland.sh --lockscreen`.
- **User mode**: starts per-user as a normal window manager.
- **In-development (dev) build, nested**: the version under active development is usually **not** the installed release — it is built from the working tree and launched directly as a **separate process running as the current user**, nested inside an existing Wayland or X11 session. See the next section.

The debug Remote Object's local socket is owner-only, so `treeland-debug` must run as a user that can access it. In release builds, `treeland-debug` auto-escalates to **root** via polkit (see Access control below), and root can access any user's socket — so no manual user prefix is needed. In debug builds, run `treeland-debug` as the **same user** as the treeland instance:

- Global service (runs as `dde`, release build) → run `treeland-debug` directly; it auto-escalates to root.
- Nested dev instance (runs as you, debug build) → run as the **current user** in your own session, no prefix.

### Access control
`treeland-debug` restricts who may use it based on build type:

- **Release builds** — only `root` may run `treeland-debug`. A non-root caller is automatically re-executed through `pkexec` (polkit), which prompts for authentication. Pass `--no-escalate` to skip the prompt and fail immediately (useful in scripts or CI). A polkit policy (`misc/polkit/org.deepin.dde.treeland-debug.policy`) is installed so escalation works out of the box.
- **Debug builds** (`TREELAND_DEBUG_DEV_BUILD`) — all users may run `treeland-debug` without restriction.

Because root can access any user's socket, release builds can reach the global `dde` service without a `sudo -u dde` prefix. In debug builds, run as the same user as the treeland instance.

### Debug socket naming
The debug Remote Object publishes on a local socket placed in `QDir::tempPath()` (typically `/tmp`, respects `$TMPDIR`) — not `XDG_RUNTIME_DIR` — so a `treeland-debug` client running under any runtime directory can discover it (the socket is still owner-only, so the client must run as the **same Unix user** as the treeland instance, or as **root** — see Access control). The base name is build-specific:

- **Release build**: `org.deepin.dde.treeland.debug`
- **Debug build**: `org.deepin.dde.treeland.debug-dev`

When a name is already held by another running instance, a numeric suffix is appended (`-1`, `-2`, …, up to `-99`), selected with a non-blocking `flock` advisory lock — the same approach libwayland uses for Wayland sockets (`src/modules/resource/treelanddebugsocket.*`). This means **multiple treeland instances can coexist** (global service + session service + a nested dev build) without socket conflicts; you no longer need to stop one to run another. A stale socket file left by a crashed instance is reclaimed automatically.

A **debug-built** `treeland-debug` client automatically prefers the debug socket (`…debug-dev`) and falls back to the release socket (`…debug`) when the debug instance is unreachable; a **release-built** client uses the release socket. An explicit `--url` always overrides this with no implicit processing.

## Debugging the in-development (dev) build
When the bug is in the version you are currently developing, you run that build yourself as a current-user process, nested inside an existing Wayland or X11 session:

```bash
# from the build tree, nested inside an existing Wayland session
WLR_BACKENDS=wayland ./build/src/treeland.sh
# nested inside an existing X11 session
WLR_BACKENDS=x11 ./build/src/treeland.sh
```

`treeland.sh` wraps `treeland` (adding the pixman software-rendering fallback on `dri2` failure, same as the service). Debugging this instance:

- **Debug source is already on**: in Debug builds (`TREELAND_DEBUG_BUILD`) the debug Remote Object is enabled **by default** (`ALWAYS_ENABLE_TREELAND_DEBUG` is defined for the Debug config; pass `-DTREELAND_DEBUG_SOURCE=OFF` to CMake to keep it off) — skip the DConfig step below. Only release builds need `remoteDebug` set.
- **No `sudo -u dde`**: run `treeland-debug` as the **current user**, same session as the instance. Because the socket is owner-only, this attaches to your own instance by default.
- **Logs go to the terminal**, not journalctl: treeland's stdout/stderr (Qt categories) land where you launched it. Set `QT_LOGGING_RULES` in that process's environment, and pass `--console-log` to force console logging on a release build (debug builds always log to console).
- **No socket conflict with the global service**: a debug build uses the distinct `-dev` socket name and auto-suffixes for concurrent instances (see Debug socket naming above), so the nested dev instance coexists with the global `treeland.service` — no need to stop it. A debug-built `treeland-debug` prefers your dev instance by default.

## Enable the debug source (first step)
The `WindowTree` debug Remote Object is **off by default in release builds** (Debug builds already have it on). If you need it on a release build, turn it on once:

```bash
sudo -u dde -- dde-dconfig set \
  -a org.deepin.dde.treeland \
  -r org.deepin.dde.treeland \
  -k remoteDebug \
  -v true
```

Toggling `remoteDebug` takes effect **immediately** — the remote source is created or destroyed on the fly, no restart needed (the compositor watches `remoteDebugChanged` at runtime). `treeland-debug` connects to the build-specific debug socket (see Debug socket naming above), remote object `WindowTree`.

## treeland-debug CLI
`treeland-debug` is an adb-style inspector/controller. Two modes: one-shot `treeland-debug <cmd> [args]` and REPL `treeland-debug shell`. In release builds, `treeland-debug` auto-escalates to root via polkit; in debug builds, run as the same user as the treeland instance. `--json` gives machine-readable output for `tree`/`cursor`/`windows`/`clients`.

Global options: `--url <url>` (default: auto — build-specific socket; a debug build prefers the debug socket and falls back to the release socket), `--name <name>` (default `WindowTree`), `--timeout-ms <n>` (default 30000), `--json`. Pass `--url` explicitly to target a specific instance (used verbatim, no implicit processing).

### Inspect
| Command | Output |
| --- | --- |
| `tree` | Full layout tree (JSON by default). `{"currentMode", "layers":[{name,layer,windows,workspaces:[{id,isActive,windows}]}]}` |
| `cursor` | Cursor position `{"x","y"}`. |
| `windows` | Toplevel windows table / JSON array. |
| `clients` | Clients + their windows (incl. `pid`, `executable`, `command`). |
| `top [interval-ms]` | Live `top`-like clients/windows view (Ctrl+C to quit). |
| `scene [id]` | QtQuick scene tree of one window (by id/appId) or the whole scene — for menus/popups/decorations not in the layout `tree`. |

Every window has a stable numeric `id` (and an `appId`); either is accepted by all control commands. Use `windows`/`clients`/`top` to discover ids.

### Window control
`activate`, `close`, `minimize`, `maximize`, `fullscreen`, `move <id> <x> <y>`, `resize <id> <w> <h>`, `workspace <id> <ws-id>`. Each prints `ok`/`failed`.

### Input injection
`move-cursor <x> <y>`, `event motion <x> <y>`, `event button <btn> [press|release|click]`, `event key <key> [press|release|tap]`. Pointer buttons use Linux input codes (`left`=0x110, `right`=0x111, `middle`=0x112); numeric keys pass through as **Qt::Key** values (not Linux evdev keycodes) or use common names (`enter`, `esc`, `space`, `a`–`z`, `0`–`9`, `f1`–`f12`, arrows). Keys go to the **keyboard-focused** surface — `activate` a window first; pointer buttons go to the surface under the cursor — move the cursor there first.

### Screenshot
`screenshot output [name] [file]` and `screenshot window <id> [file]`. Rendered server-side, PNG bytes returned to the client, which writes the file and prints the path (auto path under `/tmp` if `file` omitted).

### Shell / listen
`shell` — interactive REPL. `listen [--port <p>] [--host <a>]` — HTTP/WebSocket server exposing the same capabilities for a browser frontend (`/api/*`, `ws://host:port/ws`). It is **unauthenticated** and binds `0.0.0.0:8080` by default — for local debugging always bind to the loopback: `listen --host 127.0.0.1 --port 8080`. See `tools/treeland-debug/README.md` for the full REST/WS reference.

### Quick start
```bash
# enable once (takes effect immediately, no restart) — only for release builds; Debug builds are on by default
sudo -u dde -- dde-dconfig set -a org.deepin.dde.treeland -r org.deepin.dde.treeland -k remoteDebug -v true

# inspect — release builds auto-escalate to root via polkit; debug builds run as current user
treeland-debug tree
treeland-debug --json windows
treeland-debug clients

# control
treeland-debug activate dde-file-manager
treeland-debug maximize 93824992268800
treeland-debug close dde-file-manager

# inject input
treeland-debug event key enter tap
treeland-debug event button left click

# screenshot
treeland-debug screenshot output DP-1 /tmp/ss.png   # syntax: screenshot output <output-name> [file]
treeland-debug screenshot window 93824992268800
```

## Reading logs
treeland uses Qt logging categories, all centralized in `src/common/treelandlogging.*`. Category string ids follow `treeland.<module>[.<submodule>]` (e.g. `treeland.surface`, `treeland.input`, `treeland.workspace`, `treeland.output`, `treeland.seat`, `treeland.protocol`, `treeland.wallpaper`, `treeland.xwayland`, `treeland.shell.xdg`, `treeland.popup.focus`, `treeland.debug`). waylib categories are in `waylib/src/server/wayliblogging.*` with ids `waylib.*`.

Control log verbosity with `QT_LOGGING_RULES`. In global mode `treeland.service` is a **system** unit, so set the variable via a systemd drop-in (a system service has no `--user` environment). For a nested dev instance, set it in the environment of the launched process instead (see the dev-build section above).

```bash
sudo mkdir -p /etc/systemd/system/treeland.service.d
printf '[Service]\nEnvironment=QT_LOGGING_RULES=treeland.surface.debug=true;treeland.input.debug=true\n' | sudo tee /etc/systemd/system/treeland.service.d/debug.conf
sudo systemctl daemon-reload
sudo systemctl restart treeland.service
```

Note: the shipped unit sets `StandardOutput=null` / `StandardError=null`, so treeland's own stdout/stderr (including Qt logging) is **discarded** and will not appear in the journal. To capture it, also add `StandardOutput=journal` / `StandardError=journal` to the drop-in above. The journal itself is the system journal (not `--user`):

```bash
# recent logs from the global treeland service (runs as dde)
sudo journalctl -u treeland.service -n 200 --no-pager
# or with full verbosity + follow
sudo journalctl -u treeland.service -f
```

`treeland.sh` wrapper: on startup it checks `--try-exec`; if it fails with `failed to create dri2 screen` it falls back to `WLR_RENDERER=pixman` software rendering (VirtualBox without 3D). If a bug only reproduces with the hardware renderer, note that fallback may mask it.

## Diagnostic workflows
Start from the symptom and pick the smallest reliable check. Reproduce first, then isolate the layer (treeland vs waylib vs wlroots vs client).

**Wrong window state (max/min/fullscreen/tiling, geometry, visibility):**
1. `treeland-debug --json windows` — read `state`, `visible`, `active`, `geometry`, `workspace`, `layer`, `frames`, `damage` for the target window (match by `appId`).
2. `treeland-debug tree` — confirm which layer/workspace the window sits in.
3. `treeland-debug scene <id>` — if the layout looks right but rendering is wrong, check the QtQuick scene (menus/popups/decorations).
4. Cross-check with logs: `QT_LOGGING_RULES='treeland.surface.debug=true;treeland.workspace.debug=true'`.

**Focus / activation wrong:**
1. `treeland-debug --json windows` — check `active` flags; `treeland-debug cursor` and `treeland-debug --json cursor-window` (HTTP API only) to see what's under the cursor.
2. `treeland-debug activate <id|appId>` and re-check.
3. Relevant categories: `treeland.seat`, `treeland.popup.focus`, `treeland.activation`.

**Input not landing / wrong target:**
1. `treeland-debug move-cursor <x> <y>` then `treeland-debug event button left click`; `treeland-debug event key enter tap`.
2. Confirm pointer/keyboard focus via `windows` (`active`) — keys go to keyboard-focused surface, buttons to surface under cursor.
3. Categories: `treeland.input`, `treeland.seat`, `waylib.input.pointer`, `waylib.seat`. Check seatd: `sudo journalctl -u dde-seatd -f` and that `LIBSEAT_BACKEND=seatd`/`SEATD_SOCK=/run/dde-seatd.sock` are set (see `misc/systemd/treeland.service.in`).

**Output blank / missing / wrong resolution:**
1. `treeland-debug tree` — confirm outputs present; `treeland-debug --json windows` per-output.
2. `treeland-debug screenshot output <name> <file>` to see what the compositor actually renders.
3. Categories: `treeland.output`, `waylib.output`, `waylib.output.drm`. If hardware rendering fails, the `treeland.sh` pixman fallback is active.

**Crash / hang:**
1. Check service state and restart logs: `sudo journalctl -u treeland.service -n 200 --no-pager` (add a `StandardError=journal` drop-in first — see Reading logs).
2. If ASAN is enabled, logs go to `/tmp/treeland-asan*` files, not journalctl (see `treeland.service.in`).
3. Reproduce with minimal steps using `treeland-debug` (inject events, activate/close windows) and note the last actions before the crash.
4. For hangs, use `top`/`clients` to see if a client is stuck, and gdb attach (`sudo gdb -p <pid>`) if needed.

**Client-side suspicion:** check the client's pid via `treeland-debug clients`; verify the client is actually talking to the compositor (its `windows`, `frames` incrementing). A frozen client will show `frames` not advancing.

## Rules
- In release builds, `treeland-debug` auto-escalates to root via polkit (root can access any user's socket), so no manual user prefix is needed. In debug builds, run as the **same user** as the treeland instance. The debug socket is owner-only. Pass `--no-escalate` to skip polkit escalation in scripts/CI.
- Use `--json` for machine-readable state you need to parse; use `windows`/`clients`/`top` to discover stable window ids before controlling them.
- Screenshots are a strong ground truth: if the layout `tree` is right but the image is wrong, the issue is in rendering/QtQuick scene, not layout.
- Prefer root-cause fixes; the live tool is for observation and reproduction, not a substitute for reading the code path that owns the failing state.
- When a runtime change is needed, prefer the appropriate skill: logging changes -> `logging-guidelines`; DConfig -> `treeland-dconfig-configuration`; protocol -> protocol skills.

## Verification
If your fix is code, build it (see `AGENTS.md`) and re-run the same `treeland-debug` inspection to confirm the live state now matches the expectation. If the fix changed observable behavior, state which `treeland-debug` command confirms it.
