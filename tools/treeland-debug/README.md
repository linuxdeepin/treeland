# treeland-debug

`treeland-debug` is an adb-style command-line inspector and controller for the
Treeland compositor. It talks to Treeland's debug Remote Object
(`WindowTreeRemote`, generated from `src/modules/resource/treelandwindowtree.rep`)
over Qt Remote Objects and exposes a set of subcommands for inspecting the
window tree, controlling windows, injecting input events and grabbing images.

It supports two modes, mirroring `adb`:

- **Non-shell (one-shot) mode** — the default: `treeland-debug <command> [args]`.
- **Shell mode** — an interactive REPL: `treeland-debug shell`.

## Build and install

The client requires CMake, Qt6 Core and Qt6 RemoteObjects, plus the Qt6 `repc`
and `moc` tools. Build it from the Treeland repository root:

```bash
cmake -S . -B build -DBUILD_TREELAND_DEBUG=ON
cmake --build build --target treeland-debug
sudo cmake --install build --component treeland-debug
```

The executable is installed to `/usr/local/bin/treeland-debug` by default.
Set `CMAKE_INSTALL_PREFIX` during configuration to use another prefix.

## Enabling the debug source in Treeland

Treeland runs as the `dde` user in global mode. Its Qt Remote Object server uses
owner-only local socket access, so run this client as `dde`.

Before starting Treeland, enable the `debugSource` DConfig option as the `dde`
user; otherwise the `WindowTree` Remote Object source is absent:

```bash
sudo -u dde -- dde-dconfig set \
  -a org.deepin.dde.treeland \
  -r org.deepin.dde.treeland \
  -k debugSource \
  -v true
```

Restart Treeland after changing this option.

## Global options

```
--url <url>          Remote object host URL (default: local:org.deepin.dde.treeland.debug)
--name <name>        Remote object name (default: WindowTree)
--timeout-ms <n>     Request timeout in milliseconds (default: 30000)
--json               Emit machine-readable JSON for `windows`/`clients`
-h, --help           Show help
-v, --version        Show version
```

Backward compatibility: `--tree` and `--cursor` are accepted as aliases for the
`tree` and `cursor` commands.

## Commands

Run `sudo -u dde -- treeland-debug <command>` for any of the following.

### Inspection

| Command | Description |
| --- | --- |
| `tree` | Print the complete layout tree as JSON (default). |
| `cursor` | Print the cursor position as JSON. |
| `windows` | List all toplevel windows (table, or JSON with `--json`). |
| `clients` | List connected Wayland clients and the windows each owns. |
| `top [interval-ms]` | Live, `top`-like refreshing view of clients and windows (default 1000 ms, Ctrl+C to quit). |

The `windows`/`clients`/`top` views report a stable numeric `id` for every
window. That id (or the window's `appId`) is accepted by every control command.

### Window control

Targets may be given by numeric `id` (printed by `windows`/`clients`/`top`) or
by `appId` (the first matching window is used).

| Command | Description |
| --- | --- |
| `activate <id>` | Activate (focus) a window. |
| `close <id>` | Close a window. |
| `minimize <id>` | Minimize a window. |
| `maximize <id>` | Toggle maximized state. |
| `fullscreen <id>` | Toggle fullscreen state. |
| `move <id> <x> <y>` | Move a window to `(x, y)`. |
| `resize <id> <w> <h>` | Resize a window to `(w, h)`. |
| `workspace <id> <ws-id>` | Move a window to a workspace. |

```bash
sudo -u dde -- treeland-debug windows
sudo -u dde -- treeland-debug close dde-file-manager
sudo -u dde -- treeland-debug maximize 93824992268800
```

### Input / event injection

| Command | Description |
| --- | --- |
| `move-cursor <x> <y>` | Move the cursor to `(x, y)`. |
| `event motion <x> <y>` | Move the cursor to `(x, y)`. |
| `event button <btn> [press\|release\|click]` | Send a pointer button; `btn` = `left\|right\|middle\|<code>` (default `click`). |
| `event key <key> [press\|release\|tap]` | Send a keyboard event; `key` = name or raw Linux evdev keycode (default `tap`). |

Pointer buttons use Linux input codes (`BTN_LEFT` = 0x110, `BTN_RIGHT` = 0x111,
`BTN_MIDDLE` = 0x112). Keyboard keys use Linux evdev keycodes; common names are
recognised (`enter`, `esc`, `space`, `tab`, `left`/`right`/`up`/`down`, `a`–`z`,
`0`–`9`, `f1`–`f12`, …). Keys are delivered to the currently keyboard-focused
surface, so `activate` a window first to target it. Pointer buttons are
delivered to the surface that currently holds pointer focus; move the cursor
onto a window before clicking.

```bash
sudo -u dde -- treeland-debug activate dde-file-manager
sudo -u dde -- treeland-debug event key enter tap
sudo -u dde -- treeland-debug event button left click
```

### Image capture

Screenshots are rendered server-side (via the same GPU texture read-back the
screenshot protocol uses) and returned to the `treeland-debug` client as PNG
bytes; the client writes them to a file and prints the path. The compositor
itself never touches the filesystem.

| Command | Description |
| --- | --- |
| `screenshot output [name] [file]` | Grab an output (by id/name; primary if omitted). |
| `screenshot window <id> [file]` | Grab a single window. |

If `file` is omitted a path under `/tmp` is generated.

```bash
sudo -u dde -- treeland-debug screenshot window 93824992268800
```

### Interactive shell mode

```bash
sudo -u dde -- treeland-debug shell
```

Starts a REPL (`treeland>`) that accepts any of the commands above, plus `help`
and `exit`. This is the *shell* mode; running a single command on the command
line is the *non-shell* mode.

### HTTP / WebSocket server mode

```bash
sudo -u dde -- treeland-debug listen [--port <port>] [--host <addr>]
```

Starts an HTTP + WebSocket server (default `0.0.0.0:8080`) that exposes every
CLI capability over the network — intended as the backend for a browser-based
graphical debugger. All data is returned through the API: screenshots come back
as `image/png` response bytes, not files on disk.

Each HTTP request (and each WebSocket one-shot command) creates its own
short-lived session that connects to the compositor, performs the operation,
then disconnects — the server consumes **zero compositor-side resources while
idle**. Live WebSocket subscriptions (`top`, `events`, `watch`) hold a session
open only for the lifetime of the subscription.

CORS headers (`Access-Control-Allow-Origin: *`) are set on every response and
OPTIONS preflight is handled, so a browser frontend can call the API directly.

#### HTTP REST API

| Method | Path | Body / Query | Description |
| --- | --- | --- | --- |
| GET | `/api/tree` | — | Layout tree JSON |
| GET | `/api/cursor` | — | Cursor position JSON |
| GET | `/api/windows` | — | All windows JSON |
| GET | `/api/clients` | — | All clients JSON |
| GET | `/api/focused` | — | Focused window id |
| GET | `/api/cursor-window` | — | Window under cursor id |
| GET | `/api/events` | `?since=N` | Debug events after seq N |
| GET | `/api/screenshot/output` | `?name=...` | PNG bytes (`image/png`) |
| GET | `/api/screenshot/window` | `?target=...` | PNG bytes (`image/png`) |
| POST | `/api/activate` | `{"target":"..."}` | Activate window |
| POST | `/api/close` | `{"target":"..."}` | Close window |
| POST | `/api/minimize` | `{"target":"..."}` | Minimize window |
| POST | `/api/maximize` | `{"target":"..."}` | Toggle maximized |
| POST | `/api/fullscreen` | `{"target":"..."}` | Toggle fullscreen |
| POST | `/api/move` | `{"target":"...","x":N,"y":N}` | Move window |
| POST | `/api/resize` | `{"target":"...","width":N,"height":N}` | Resize window |
| POST | `/api/workspace` | `{"target":"...","workspaceId":N}` | Move to workspace |
| POST | `/api/move-cursor` | `{"x":N,"y":N}` | Move cursor |
| POST | `/api/event/motion` | `{"x":N,"y":N}` | Pointer motion |
| POST | `/api/event/button` | `{"button":"...","action":"..."}` | Pointer button |
| POST | `/api/event/key` | `{"key":"...","action":"..."}` | Key event |

`target` may be a numeric window id or an `appId` (first match is used).
Responses are JSON: `{"ok":true,"data":...}` or `{"ok":false,"error":"..."}`.

#### WebSocket API

Connect to `ws://<host>:<port>/ws` and send JSON messages. One-shot commands
mirror the HTTP API (`{"command":"tree"}`, `{"command":"activate","target":"..."}`
etc.). Screenshots are returned as a binary frame followed by a JSON ack.

Live subscriptions:

| Action | Message | Description |
| --- | --- | --- |
| Subscribe | `{"command":"subscribe","type":"top","id":"...","intervalMs":1000}` | Live window/client list |
| Subscribe | `{"command":"subscribe","type":"events","id":"...","intervalMs":1000}` | Live event stream |
| Subscribe | `{"command":"subscribe","type":"watch","target":"...","id":"...","intervalMs":250}` | Live watch on a window |
| Unsubscribe | `{"command":"unsubscribe","id":"..."}` | Stop a subscription |

## JSON output

`tree` and `cursor` always print JSON. `windows` and `clients` print a
human-readable table by default; pass `--json` for machine-readable JSON, which
includes the stable `id`, `appId`, `title`, geometry, state and (for `clients`)
the owning client `pid`/`executable`.

## License

treeland is licensed under Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only.
