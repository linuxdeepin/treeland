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

The client requires CMake, Qt6 Core, Qt6 RemoteObjects, Qt6 HttpServer and
Qt6 WebSockets (the latter two back the `listen` HTTP/WebSocket server and are
hard build-time requirements), plus the Qt6 `repc` and `moc` tools. Build it
from the Treeland repository root:

```bash
cmake -S . -B build -DBUILD_TREELAND_DEBUG=ON
cmake --build build --target treeland-debug
sudo cmake --install build --component treeland-debug
```

The executable is installed to `/usr/local/bin/treeland-debug` by default.
Set `CMAKE_INSTALL_PREFIX` during configuration to use another prefix.

## Enabling the debug source in Treeland

Treeland runs as the `dde` user in global mode. Who may connect to its debug
socket depends on the build type:

- **Debug builds** — the development socket is owner-only (mode `0600`); run
  `treeland-debug` as the same UID as the compositor. No polkit prompt is needed.
- **Release builds** — the socket is `chmod 0000` (the kernel then rejects every
  `connect()` except root's), so the client must run as `root`; a non-root
  caller is escalated through `pkexec` automatically (see
  [Access control](#access-control)). Once `treeland-debug no-auth` has asked the
  compositor to open a window, the socket accepts every user instead and the
  client connects without a prompt.

`remoteDebug` is Treeland's master switch for debugging, and it is **off by
default** -- debugging hands out window contents and input injection, which a
compositor running inside a user session must not offer unasked. While it is on,
Treeland publishes its debug socket and serves the `WindowTree` Remote Object to
whoever connects; a plain command never writes DConfig, it just connects.
Release builds only let root connect (mode 0000) -- turn the switch off to forbid
debugging entirely, and the socket disappears with it. A debug build
(`CMAKE_BUILD_TYPE=Debug`) is the exception: it ignores the switch and always
publishes its `-dev` socket, because that instance is the one being developed.

In a Debug build, Treeland always uses the current user's
`/tmp/treeland-debug-<uid>` directory by default instead of publishing in
`/run/treeland-debug`. The directory is owned by the current UID and mode
`0700`, and its socket is mode `0600`, so other unprivileged UIDs cannot reach
it (root retains privileged access). The Debug CLI checks this directory first,
then the shared global and legacy locations and the current user's runtime
directory; it does not enumerate other users' runtime directories. Release-socket fallback is preserved.

For non-Debug compositors launched directly by a non-`dde` user without
`TREELAND_RUN_MODE=user`, `/run/treeland-debug` is used only when writable;
otherwise the socket is published in that process's `$XDG_RUNTIME_DIR`.

The remote source is a stateless shell: the window tree, screenshots and scene
trees are all collected per call. The one session-scoped resource is input-event
capture, which is armed only while a client is polling for events and torn down
after it goes idle, so nothing but the socket exists while nobody is connected.

Because the client has nothing to restore, a crash, `SIGKILL` or a closed
terminal cannot leave any state behind: there is no session-scoped setting at
all. If you find no socket, either the switch is off (the default) or Treeland
is not running; `treeland-debug` says so on stderr. Turn it on -- or off again --
with the one command that writes the switch, `treeland-debug debug on|off`:

```bash
treeland-debug debug on    # or: treeland-debug debug off / treeland-debug debug
```

The switch is a per-user DConfig key and Treeland reads the one of the user it
runs as, so the command finds that user through the running compositor and
escalates through `pkexec` when it is not you (`--no-escalate` reports the
requirement instead of prompting). With root, the same thing by hand is
`dde-dconfig -u <compositor uid> set -a org.deepin.dde.treeland -r
org.deepin.dde.treeland -k remoteDebug -v true`.

Toggling `remoteDebug` takes effect immediately: the compositor watches
`remoteDebugChanged` and creates or destroys the Remote Object source on the
fly, so no restart is needed and no client is involved. Any number of clients
can attach at once; none of them own the switch, so one exiting cannot cut
another one off.

## Access control

`treeland-debug` restricts who may use it depending on the build type:

- **Release builds** — only `root` may run `treeland-debug`; the compositor
  side enforces the same rule on the socket itself (mode 0000, so any non-root
  `connect()` is refused with `EACCES`). The client first tries to connect as
  itself -- an unprivileged caller cannot even reach a root-only socket, so that
  attempt is skipped rather than probed for the whole timeout -- and, only if it
  fails, re-executes itself through `pkexec` (polkit), which prompts for
  authentication. Pass `--no-escalate` to
  skip the prompt and fail instead (useful in scripts or CI).
  `treeland-debug no-auth` lifts this for a temporary debugging window: it asks
  the compositor (the `requestNoAuth` method of `WindowTreeRemote`) to make the
  socket (0666) and its own debug directory (0711) reachable for every local
  user, so no prompt appears afterwards. Only a caller that already got through
  the sealed socket can ask, and in a release build that is root alone: mode 0000
  blocks the compositor's own user too, only `CAP_DAC_OVERRIDE` passes. The
  window closes after 30 minutes without a debug call, and every call pushes it
  out again; closing it also drops the connections opened while it was open. It
  only applies to the global service's own directory (`/run/treeland-debug`, not
  its `$XDG_RUNTIME_DIR`) -- a session instance lives in `/run/user/<uid>`, which
  is never widened, so there root stays required (and `no-auth` fails).
- **Debug builds** — the `-dev` socket is mode `0600` and normally lives in
  `/tmp/treeland-debug-<uid>` (mode `0700`), so only the owning UID can reach it
  and it does not add a socket to the host's `/run` directory. The Debug client
  checks this directory first, then performs the existing shared-location
  searches and release fallback. Explicit `TREELAND_DEBUG_DIR` can override the
  server directory. The socket name keeps it separate from a release instance,
  whose sealed socket still requires root. Non-Debug session instances use their
  runtime directory and remain subject to that directory's permissions.

`listen` and `debug` are the exceptions to all of this: `listen` starts the
HTTP/WebSocket server before any connection exists, and `debug` writes the
switch of the compositor's user, so neither connects first. `listen` therefore
never escalates -- run it as root (or as `dde` during a no-auth window) when the
compositor is a release build. `debug` escalates only when the compositor runs
as another user, which is exactly when DConfig needs root.

A polkit policy (`misc/polkit/org.deepin.dde.treeland-debug.policy`) is
installed alongside the executable so that the `pkexec` escalation works out of
the box on release builds. If `pkexec` is not available, non-root callers
receive an error explaining how to proceed.

## Global options

```
--url <url>          Remote object host URL (default: $TREELAND_DEBUG_SOCKET,
                     else automatic probing; Debug builds check their own
                     /tmp/treeland-debug-<uid> first, then shared global/legacy
                     locations and their own runtime directory. They do not
                     enumerate other UIDs' runtime directories.)
--name <name>        Remote object name (default: WindowTree)
--timeout-ms <n>     Request timeout in milliseconds (default: 30000)
--json               Emit machine-readable JSON instead of the text view
--no-escalate        Do not attempt polkit privilege escalation; report the
                     requirement instead (scripts, CI)
--log-level <level>  Verbosity of this client's own stderr diagnostics: error,
                     warning, info (default) or debug. Only info and debug differ
                     today; debug adds discovery detail such as the socket a
                     fallback connection came from.
-h, --help           Show help
-v, --version        Show version
```

The socket is chosen in this order: `--url`, then `$TREELAND_DEBUG_SOCKET`, then
the built-in probing. `TREELAND_DEBUG_SOCKET` takes a socket path (or a bare
socket name) and is treated like `local:<value>`; a value with a scheme is used
as-is. It is handy when the compositor runs under a runtime directory the
client cannot guess:

```bash
TREELAND_DEBUG_SOCKET=/run/treeland-debug/org.deepin.dde.treeland.debug treeland-debug windows
```

Backward compatibility: `--tree` and `--cursor` are accepted as aliases for the
`tree` and `cursor` commands.

When neither `--url` nor `$TREELAND_DEBUG_SOCKET` is given, the client tries each
candidate socket path followed by the instances Treeland published under a
`"<name>-<N>"` suffix (taken when another instance already holds the base name), so
the build-type preference still wins. Every connected run names the socket it read at
the end (and the shell does it on entry), so a pasted output always says whose it is;
`help`, `--version`, `debug` and `listen` never connect, so they print no source line.
Which candidate it fell back from is client-side discovery detail: that
`(fallback from ...)` suffix is printed only with `--log-level debug`, and it also
covers attaching to a `-<N>` instance of the preferred build type -- not only a
cross-build-type fallback. `error`/`warning`/`info` behave identically today, because
no current message is classed below Info. Each candidate is probed with `--timeout-ms` shared between the
candidates still to be tried, but never for less than one short probe (about three
seconds): a live socket answers in milliseconds, so a probe only bounds the wait for
one that is slow or dead. A dead socket therefore delays the preferred instance
without starving it, and discovery can take up to `--timeout-ms` plus one probe per
candidate -- which also gives a compositor that publishes its source late the time
to do so. An explicit `--url` or `$TREELAND_DEBUG_SOCKET` is used verbatim and tries
nothing else.

## Output format

The human-readable output follows the x11 tools, so anyone used to `xwininfo`,
`xprop` and `xdotool` reads it the same way -- in one command instead of five
programs:

- **Lists** (`windows`, `clients`, `top`, `shortcuts`) are a header row plus
  exactly one line per object, with no continuation lines, so they stay
  readable and greppable.
- **Geometry** is `WxH+X+Y`, the `xwininfo -geometry` / `xdotool
  getwindowgeometry` order: `800x600+100+50`. A negative coordinate keeps its
  own sign (`800x600-10+50`); unlike x11 it is not an offset from the right
  edge, because a window can sit left of the layout origin on a multi-output
  desktop.
- **One object in full** (`window <id>`, `idle`) is a `Key: value` block with
  one field per line and the keys padded, like `xwininfo -id`; `window` ends
  with a bare `-geometry WxH+X+Y` line that can be pasted into a report.
- **Nested structure** (`tree`, `scene`) keeps tree branch characters.
- **Source** is named at the end of every connected run on stderr --
  `treeland-debug: connected to local:/run/treeland-debug/...` -- so the data
  never floats free of the instance it came from, and `--json`/piped stdout stay
  parseable. The shell prints the same line as its login banner instead, and the
  commands that never connect (`help`, `--version`, `debug`, `listen`) print no
  source line.
- **`--json`** is the machine-readable form of the same data: the `listen`
  HTTP/WebSocket/MCP API returns exactly the same records (as its `data`
  field), so a script and a browser read one contract.

Every window record names its **shell surface type** -- `xdg-toplevel`,
`xdg-popup`, `xwayland`, `layer-shell`, `input-popup`, `lock-screen`,
`splash-screen` -- in the `TYPE` column of the tables, in the tree, and as
`shellType` in JSON. That is what tells an application window apart from a
layer-shell panel, a popup or an XWayland window.

[Example output](#example-output) below shows every command's actual output.

## Example output

One example per command, on a two-output desktop (`eDP-1`, `HDMI-A-1`) with a
`dde-file-manager` window on workspace 1, the dock as a `layer-shell` surface,
an XWayland app and the lock screen. Ids, timestamps and sizes differ from run
to run; the layout shown is exactly what the commands print.

### Inspection

`tree` -- the layout tree, no header row: layers, then workspaces, then
windows. `*` marks the active window, `+` inside `[outputs]` the output driving
its frame callbacks, and `+Min` a minimized window:

```console
$ treeland-debug tree
Mode: Normal
  (per window: active id type app-id state geometry [outputs] title)
├─ Layer: normal (2)
│  ├─ Workspace 1 (active)
│     └─ * 93824992268800    xdg-toplevel   dde-file-manager        Normal          800x600+100+50      [+eDP-1]  Documents
│  └─ Workspace 2
│     └─  (no windows)
├─ Layer: top (3)
│  └─   93824992269056    layer-shell    dde-dock                Normal          1920x40+0+1040      [eDP-1,+HDMI-A-1]
└─ Layer: overlay (4)
   └─   93824992269568    lock-screen    dde-lock                Fullscreen      1920x1080+0+0       [+eDP-1]
```

`windows` -- one header row, one line per window, and a two-line legend under
the table. A window with no title prints a title column that is simply empty:

```console
$ treeland-debug windows
ID              TYPE           APP-ID                  STATE       ACTIVE  MIN    OUTPUT            GEOMETRY            TITLE
93824992268800  xdg-toplevel   dde-file-manager        Normal      yes     no     +eDP-1            800x600+100+50      Documents
93824992269056  layer-shell    dde-dock                Normal      no      no     eDP-1,+HDMI-A-1   1920x40+0+1040
93824992269312  xwayland       steam                   Tiling      no      yes    +HDMI-A-1         1240x800-10+50      Steam
93824992269568  lock-screen    dde-lock                Fullscreen  no      no     +eDP-1            1920x1080+0+0
  (TYPE = shell surface type; '+' in OUTPUT marks the frame-pacing output;
   GEOMETRY is WxH+X+Y; `treeland-debug window <id>` shows one window in full)
```

`window <id|appId>` -- the full record, `Key: value`, one field per line, empty
fields omitted, and a bare `-geometry` line last for pasting into a report:

```console
$ treeland-debug window 93824992268800
Window id: 93824992268800 "Documents"
  App ID:               dde-file-manager
  Shell type:           xdg-toplevel
  Layer:                2  Z: 3
  Workspace:            1
  State:                Normal
  Visible:              yes
  Active:               yes
  Minimized:            no
  Output:               eDP-1
  Outputs:              eDP-1
  Frame pacing:         eDP-1
  Geometry:             800x600+100+50
  Position:             100,50
  Bounding rect:        800x600+100+50
  Titlebar:             800x40+100+50
  Icon:                 24x24+140+60
  Frames:               12345
  Damage:               300x200+120+80
  Input region:         [100,50 800x600]
  Opaque region:        [100,90 800x560]
  Last leave:           HDMI-A-1 (enabled: yes, emptied outputs: no)
  Last outputs update:  enabledChanged (enabled: 1/2, computed: 1, automatic: yes)
-geometry 800x600+100+50
```

`Damage`, `Titlebar`, `Icon`, `Bounding rect`, `Last leave` and `Last outputs
update` only appear when the compositor reported them, so a quiet window prints
a shorter block.

An `appId` that matches more than one window shows the first one and says so on
stderr, which keeps `--json` output parseable:

```console
$ treeland-debug window dde-file-manager
treeland-debug: 2 windows match 'dde-file-manager'; showing id 93824992268800
Window id: 93824992268800 "Documents"
  ...
```

`clients` -- one block per connected Wayland client (hex id, pid, appId,
window count, `exe`/`cmd`), followed by that client's windows in the same table
as `windows`:

```console
$ treeland-debug clients
client 0x560000000001  pid=4242  appId=dde-file-manager  windows=1
  exe: /usr/bin/dde-file-manager
  cmd: dde-file-manager -p
  ID              TYPE           APP-ID                  STATE       ACTIVE  MIN    OUTPUT            GEOMETRY            TITLE
  93824992268800  xdg-toplevel   dde-file-manager        Normal      yes     no     +eDP-1            800x600+100+50      Documents

client 0x560000000002  pid=5150  windows=2
  exe: /usr/bin/steam
  ID              TYPE           APP-ID                  STATE       ACTIVE  MIN    OUTPUT            GEOMETRY            TITLE
  93824992269312  xwayland       steam                   Tiling      no      yes    +HDMI-A-1         1240x800-10+50      Steam
  93824992269056  layer-shell    dde-dock                Normal      no      no     eDP-1,+HDMI-A-1   1920x40+0+1040
```

`cursor` -- one line, labelled, `xdotool getmouselocation` style. `window:0`
means no window is under the pointer (or the compositor did not answer), so the
shape never changes:

```console
$ treeland-debug cursor
x:100 y:50 window:93824992268800
$ treeland-debug --json cursor
{
    "window": 93824992268800,
    "x": 100,
    "y": 50
}
```

`idle` -- a `Key: value` block, then one line per idle-inhibit source. Note that
the second source below is `active=no`, so it does not count towards
`Inhibited: yes`:

```console
$ treeland-debug idle
  Idle:                 12.3s since last input
  Inhibited:            yes
  Screensaver:          no
  Inhibitors:           2
  [idle-inhibit] pid=6001 appId=vlc window=93824992269440 "Big Buck Bunny" active=yes held=5m12s exe=/usr/bin/vlc
  [screensaver] pid=6100 appId=(unknown) app="dde-screensaver" reason="playing a slideshow" active=no held=5.0s
```

`shortcuts` -- a header row and one line per binding; gesture rows leave `FLAGS`
empty (flags are a key-binding concept):

```console
$ treeland-debug shortcuts
NAME                        TRIGGER                 ACTION                      FLAGS
launcher                    Meta                    toggle-launcher             press
terminal                    Ctrl+Alt+T              launch-terminal             press|repeat
workspace-left              swipe 3-finger left     switch-workspace-left
expo                        hold 4-finger           toggle-expo
```

`top` -- a live screen repainted in place every `interval-ms` (default 1000).
Each frame carries its timestamp, the client/window counts and an idle summary,
then one line per window, most active first, with the frame delta since the
previous frame:

```console
$ treeland-debug top
treeland-debug top — 2026-11-18T14:32:07  (clients: 2  windows: 3)
  idle: 12.3s  inhibited: no  sources: 0 (0 active)
  ID              TYPE           APP-ID                  STATE       MIN     FRAMES  GEOMETRY            MARKER
  93824992268800  xdg-toplevel   dde-file-manager        Normal      no        1234  800x600+100+50      F (focus)
  93824992269312  xwayland       steam                   Tiling      yes         57  1240x800-10+50
  93824992269056  layer-shell    dde-dock                Normal      no           0  1920x40+0+1040      C (under cursor)
```

`scene [id]` -- the QtQuick item tree behind a window (or the whole render
scene), pre-formatted by the compositor: `ClassName "objectName" [x,y WxH]
z=<z> op=<opacity>`, with `hidden` on invisible items:

```console
$ treeland-debug scene 93824992268800
TreelandWindow [0.0,0.0 800.0x600.0] z=0.0 op=1.00
  QQuickRectangle [0.0,0.0 800.0x40.0] z=1.0 op=1.00
    QQuickText "titleLabel" [12.0,8.0 120.0x24.0] z=0.0 op=1.00
  QQuickItem "surfaceContent" [0.0,40.0 800.0x560.0] z=0.0 op=1.00
  QQuickPopupItem "popup" [200.0,300.0 240.0x120.0] hidden z=3.0 op=0.00
```

`events` -- a live stream, one line per event: time, event type (the same names
the JSON `type` field uses, padded), the compositor's description, then
`-> <window id>` when the event has a target:

```console
$ treeland-debug events
treeland-debug events — streaming every 50ms (Ctrl+C to quit)

14:32:07.512  motion                 782,410 -> 93824992268800
14:32:08.004  focus                  -> 93824992269312
14:32:08.311  shortcut-triggered     "terminal" -> launch-terminal
14:32:09.077  shortcut-registered    "expo" = hold 4-finger -> toggle-expo
```

`watch <id> [interval-ms]` -- samples one window every `interval-ms` (default
250 ms). The first sample prints a header, then every refresh prints one line,
`*`-prefixed when something changed since the previous sample:

```console
$ treeland-debug watch 93824992268800
treeland-debug watch 93824992268800 — every 250ms (Ctrl+C to quit)

Window 93824992268800 dde-file-manager (Documents)
  type=xdg-toplevel state=Normal geometry=800x600+100+50
14:32:07  (no change)
14:32:08    * frames 12346 (+1); damage 800x560+100+90
14:32:09    * state Normal -> Maximized; geometry 1920x1080+0+0
```

### Window control

The control commands print a single `ok` (or `failed`, with a non-zero exit
code). `activate`/`close`/… take an `id` or an `appId`:

```console
$ treeland-debug activate dde-file-manager
ok
$ treeland-debug maximize 93824992268800
ok
$ treeland-debug move 93824992268800 200 120
ok
$ treeland-debug resize 93824992268800 1024 768
ok
$ treeland-debug workspace 93824992268800 2
ok
$ treeland-debug close steam
ok
```

### Input and pointer events

`move-cursor` and the `event` subcommands also print `ok`; nothing is echoed
back because the effect shows up in `cursor`, `watch` or `events`:

```console
$ treeland-debug move-cursor 100 50
ok
$ treeland-debug event motion 100 50
ok
$ treeland-debug event button left click
ok
$ treeland-debug event key enter tap
ok
```

### Image capture

`screenshot` writes a PNG and prints the path it wrote (`/tmp/treeland-debug-
<ms>.png` when no file is given, `.png` appended when the name has no suffix).
On a terminal that supports inline images the picture is also previewed:

```console
$ treeland-debug screenshot output
/tmp/treeland-debug-1763447100123.png
$ treeland-debug screenshot window 93824992268800 ~/shots/win.png
/home/demo/shots/win.png
```

### The debug switch and access

`debug` prints the switch state it read back, and says what the compositor will
do next; the running-as state is included so a per-user switch is unambiguous:

```console
$ treeland-debug debug
remoteDebug is off (uid 1000)
$ treeland-debug debug on
remoteDebug is on; treeland publishes its debug socket now
$ treeland-debug debug off
remoteDebug is off; treeland drops its debug socket now
```

`no-auth` confirms the 30-minute window it opened:

```console
$ sudo treeland-debug no-auth
the debug socket is open to every user for 30 minutes
```

### listen (HTTP / WebSocket / MCP)

`listen` prints the address it bound, then serves:

```console
$ treeland-debug listen --host 127.0.0.1 --port 8080
treeland-debug listening on 127.0.0.1:8080
```

The HTTP API answers with the same records the CLI prints, wrapped in
`{"ok":…}`, compact:

```console
$ curl -s 127.0.0.1:8080/api/cursor
{"data":{"window":93824992268800,"x":100,"y":50},"ok":true}
$ curl -s '127.0.0.1:8080/api/window?target=dde-file-manager'
{"data":{"active":true,"appId":"dde-file-manager",…,"shellType":"xdg-toplevel","state":0,"type":0,…},"ok":true}
$ curl -s -X POST -d '{"state":"on"}' 127.0.0.1:8080/api/debug
{"data":{"enabled":true,"uid":1000},"ok":true}
$ curl -s -X POST -d '{"target":"93824992268800"}' 127.0.0.1:8080/api/activate
{"data":true,"ok":true}
```

Errors keep the same shape and a non-200 status:
`{"ok":false,"error":"no window matches 'foo'"}`.

### shell

The REPL accepts any command above plus `help` and `exit`; each result is
printed exactly as the one-shot form would print it:

```console
$ treeland-debug shell
treeland-debug shell — type 'help' for commands, 'exit' to quit
treeland-debug: logged in to local:/run/treeland-debug/org.deepin.dde.treeland.debug
treeland> cursor
x:100 y:50 window:93824992268800
treeland> activate dde-file-manager
ok
treeland> exit
```

`debug` is the one exception: it writes DConfig outside the debug socket, so the
REPL tells you to run it as a one-shot command instead of escalating a polkit
prompt from inside the shell.

### help

`--help` / `help` prints the whole command reference, grouped like the
[Commands](#commands) section:

```console
$ treeland-debug --help
Usage: treeland-debug [global-options] <command> [args]

An adb-style inspector and controller for the Treeland compositor. Runs in
one-shot (non-shell) mode by default; use the `shell` command for an
interactive REPL.

Global options:
  --url <url>          Remote object host URL (default: $TREELAND_DEBUG_SOCKET,
                       else the build-specific socket found by probing)
  ...
Output conventions (the same as the x11 tools, in one command):
  ...

Inspection:
  tree                       Print the complete window tree (human-readable, use --json for JSON)
  cursor                     Print the cursor position, xdotool-style (x:N y:N window:N,
                             window 0 = none under the cursor)
  windows                    List all toplevel windows, with their shell type
  ...
```

## Commands

Run `treeland-debug <command>` for any of the following.

### Access

| Command | Description |
| --- | --- |
| `debug [on\|off\|status]` | Read or write treeland's `remoteDebug` switch (default: `status`). The only command that runs without the debug socket -- it is what makes one exist, so it works while the switch is off. Escalates through `pkexec` when the compositor runs as another user. A debug-build compositor ignores the switch, so this only changes what a release build does. |
| `no-auth` | Ask the compositor to let any local user connect to this debug socket for 30 minutes (see Access control above; needs root unless the window is already open). |

### Inspection

| Command | Description |
| --- | --- |
| `tree` | Print the complete layout tree (human-readable by default; `--json` for JSON). |
| `cursor` | Print the cursor position, xdotool-style: `x:100 y:50 window:93824992268800`. |
| `windows` | List every window (table, or JSON with `--json`): id, shell type, appId, state, active/minimized, outputs, geometry. |
| `window <id\|appId>` | Show one window in full, xwininfo-style: shell type, container, layer/z, workspace, state, outputs, geometry, SSD rects, frame counter, damage, input and opaque regions. |
| `clients` | List connected Wayland clients and the windows each owns. |
| `idle` | Show the idle state (time since the last input activity), whether idle is inhibited, and every idle-inhibit source (window/idle-inhibitor or screensaver) holding it off. |
| `shortcuts` | List the registered shortcuts (key bindings and touchpad gestures). |
| `top [interval-ms]` | Live, `top`-like refreshing view of clients and windows (default 1000 ms, Ctrl+C to quit). Includes an idle summary line. |
| `scene [id]` | Dump the QtQuick scene tree of one window (by `id`/`appId`) or, with no id, the whole render scene. Useful for inspecting menus, popups and decorations that do not show up in the layout `tree`. |

`shortcuts` prints a header row and then one line per registered shortcut —
name, trigger (`Ctrl+Alt+T`, `swipe 3-finger left`, `hold 4-finger`), target
action and, for key bindings, the trigger flags
(`press`/`release`/`repeat`; gesture rows leave that column empty). Use `--json` for
`[{name,type,key,flags,action,fingers,direction}]`, where `type` is
`0`=key, `1`=swipe, `2`=hold. Registrations come from clients, so the list is
empty until a shell binds shortcuts through `treeland-shortcut-manager-v3`.

The `windows`/`clients`/`top` views report a stable numeric `id` for every
window. That id (or the window's `appId`) is accepted by every control command.

`idle` reports the time since the last input activity (the compositor has no
global idle threshold — clients such as the screensaver pick their own via
`ext-idle-notify-v1`), whether idle notification is currently suppressed, and
every idle-inhibit source currently held. The summary is a `Key: value` block
(`Idle`/`Inhibited`/`Screensaver`/`Inhibitors`), followed by one line per
inhibitor with its `key=value` fields: `zwp_idle_inhibit_manager_v1`
surfaces (identified by window id/appId/title) and `treeland_screensaver_v2`
inhibits (identified by the client-provided application name and reason).
`--json` emits the same data with `idleMs`/`heldMs` in milliseconds.

The list includes sources that are not in effect right now (for example an
idle-inhibitor window that is minimized); the `active` field tells them apart,
and only `active` sources count towards `inhibited`. `surfaceId` is a window id
usable with the control commands only when the inhibitor is held on a toplevel
surface — a layer-shell or subsurface inhibitor (common for video players)
reports a surface id that `windows`/`top` do not list.

```bash
treeland-debug idle
treeland-debug --json idle
```

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
treeland-debug windows
treeland-debug close dde-file-manager
treeland-debug maximize 93824992268800
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
treeland-debug activate dde-file-manager
treeland-debug event key enter tap
treeland-debug event button left click
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
treeland-debug screenshot window 93824992268800
```

### Damage diagnostics

Damage diagnostics are controlled through `treeland-debug`; no logging
environment variable is required.

| Command | Description |
| --- | --- |
| `damage highlight on\|off` | Toggle the compositor's rendered damage overlay. |
| `damage record <output> <directory>` | Record every committed frame from one output as PNG, with sidecar JSON containing the frame sequence, timestamp, buffer size, damage regions in buffer-pixel coordinates, and capture error (if any). |
| `damage stop` | Stop an active recording from another `treeland-debug` process. |

The recorder reads the output buffer synchronously after each committed render
so that image pixels correspond to the recorded frame rather than a later
compositor state. This intentionally stalls rendering while recording; use it
for short diagnostic captures. It records the fully composited output (including
blur), not each client's source `wl_buffer`. Start with an empty or new
directory; existing non-empty directories are rejected to prevent overwrites.
`Ctrl+C` stops the recorder. If its client disconnects or stops sending
keepalives, the compositor also stops recording within five seconds.

Each `frame-NNNNNNNN.json` sidecar has:

- `flushDamage`: `full` plus the flush damage rectangles mapped to buffer pixels.
- `renderDamage`: `full` plus the render damage rectangles mapped to buffer pixels.
- `damageCoordinateSpace`: `bufferPixels`; rectangle coordinates align with the PNG.
- `bufferSize`, `timestampMs`, `sequence`, `output`, and `captureError`.

The image is named `frame-NNNNNNNN.png`; when readback fails, the sidecar is
still saved with the error and no PNG. Compare successive PNGs to inspect
visible output changes, and compare the sidecar regions with the pixels that
changed. The recorder preserves every committed frame in sequence; the
synchronous readback is deliberately intrusive and is not a performance
benchmark.

```bash
treeland-debug damage highlight on
treeland-debug damage record DP-1 /tmp/treeland-damage
# Reproduce the issue, then press Ctrl+C.
treeland-debug damage highlight off
```

### Shortcut monitoring

`events` also carries the shortcut stream, so `treeland-debug events` is the
place to watch shortcuts being bound and used:

- `type 6` — `shortcut registered: "<name>" = <trigger> -> <action> [flags]`
- `type 7` — `shortcut unregistered: "<name>"`; also emitted for every binding
  when the active session changes (the registry is cleared and re-populated)
  and for the old name when a binding is re-bound under a new one.
- `type 8` — `shortcut triggered: "<name>" -> <action> [flags]`, with `target`
  set to the window that was keyboard-focused when the shortcut fired (sampled
  before the action runs, so a workspace or task switch does not report the
  destination window). Gesture triggers (swipe and hold) are reported too,
  marked `(gesture)` and with `target` 0 — a gesture has no keyboard focus to
  blame.

`treeland-debug events` prints the event type next to the timestamp
(`motion`, `button`, `key`, `shortcut-registered`, …), the same names the
`type` field uses.

Like the other live streams, capture is armed only while a client is polling
`getEvents()`, so bind a shortcut that was already registered before you
attached (restart the client that owns it, or re-login) to see a registration
event, and use any bound shortcut — key, swipe or hold — to see a trigger
event.

### Interactive shell mode

```bash
treeland-debug shell
```

Starts a REPL (`treeland>`) that accepts any of the commands above, plus `help`
and `exit`. This is the *shell* mode; running a single command on the command
line is the *non-shell* mode. Like an ssh login banner, it names the socket the
session reads as soon as it starts, so a shell attached to another instance does
not look like your own desktop.

### HTTP / WebSocket server mode

```bash
treeland-debug listen [--port <port>] [--host <addr>]
```

Starts an HTTP + WebSocket server (default `0.0.0.0:8080`) that exposes many
CLI capabilities over the network — intended as the backend for a browser-based
graphical debugger. Damage frame recording is CLI-only because it streams
frames to local files. All screenshot data is returned through the API as
`image/png` response bytes, not files on disk.

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
| GET | `/api/cursor` | — | Cursor position JSON, plus `window` (id under the cursor, `0` for none) |
| GET | `/api/windows` | — | All windows JSON |
| GET | `/api/clients` | — | All clients JSON |
| GET | `/api/idle` | — | Idle state and idle-inhibit sources JSON |
| GET | `/api/no-auth` | — | Ask the compositor to open this debug socket to every local user for 30 minutes. This API has no authentication of its own, so bind it to a trusted address (see Access control). |
| GET | `/api/shortcuts` | — | Registered shortcuts JSON |
| GET | `/api/focused` | — | Focused window id |
| GET | `/api/cursor-window` | — | Window under cursor id |
| GET | `/api/window` | `?target=...` | One window's full record (the same JSON `treeland-debug --json window <id>` prints) |
| GET | `/api/debug` | — | Treeland's `remoteDebug` switch: `{enabled, uid}` |
| POST | `/api/debug` | `{"state":"on\|off"}` | Turn the switch on or off. The server cannot escalate, so a switch that needs root fails with the reason instead of prompting. It also blocks its event loop while `dde-dconfig` answers (a 3 s budget for the set and the read-back together), so live subscriptions stall for that long at worst. |
| GET | `/api/scene` | `?target=...` | QtQuick scene tree of a window (or the whole scene if `target` is omitted), as pre-formatted text |
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
mirror the HTTP API and the CLI command names (`{"command":"tree"}`,
`{"command":"window","target":"..."}`, `{"command":"debug","state":"on"}`,
`{"command":"activate","target":"..."}` etc.). Screenshots are returned as a binary frame followed by a JSON ack.

Live subscriptions:

| Action | Message | Description |
| --- | --- | --- |
| Subscribe | `{"command":"subscribe","type":"top","id":"...","intervalMs":1000}` | Live window/client list |
| Subscribe | `{"command":"subscribe","type":"events","id":"...","intervalMs":1000}` | Live event stream |
| Subscribe | `{"command":"subscribe","type":"watch","target":"...","id":"...","intervalMs":250}` | Live watch on a window |
| Unsubscribe | `{"command":"unsubscribe","id":"..."}` | Stop a subscription |

#### MCP (Model Context Protocol) server

The HTTP server also exposes a single MCP endpoint at `POST /mcp` that speaks
JSON-RPC 2.0 over the [Streamable HTTP](https://modelcontextprotocol.io/specification/2025-06-18/basic/transports)
transport. This lets any MCP-compatible AI client — Claude Desktop, Cursor,
VS Code Copilot, etc. — directly inspect and control the Treeland compositor
without a separate bridge process.

**Endpoint:** `http://<host>:<port>/mcp`  
**Protocol version:** `2025-06-18`  
**Capabilities:** tools (no resources, no prompts, no subscriptions)

The server is stateless: every `tools/call` creates a fresh session to the
compositor, performs the operation, and disconnects. No `Mcp-Session-Id` is
returned, so clients need not send `DELETE` to terminate.

Supported JSON-RPC methods:

| Method | Description |
| --- | --- |
| `initialize` | Protocol handshake; returns server info and capabilities |
| `notifications/initialized` | Notification (no response body; HTTP 202) |
| `tools/list` | Returns the full tool catalog |
| `tools/call` | Invokes a tool by name with arguments |
| `ping` | Health check |

The 27 available tools mirror the HTTP REST API and CLI commands:

| Tool | Args | Description |
| --- | --- | --- |
| `tree` | — | Layout tree JSON |
| `cursor` | — | Cursor position |
| `windows` | — | All windows |
| `clients` | — | All clients |
| `idle` | — | Idle state and idle-inhibit sources |
| `no-auth` | — | Open this debug socket to every local user for 30 minutes |
| `shortcuts` | — | Registered shortcuts |
| `focused` | — | Focused window id |
| `cursor_window` | — | Window under cursor id |
| `window` | `target` | One window's full record |
| `debug` | `state?` | Read (`state` omitted) or write (`"on"`/`"off"`) the `remoteDebug` switch |
| `scene` | `target?` | QtQuick scene tree of a window (or whole scene) |
| `events` | `since?` | Debug events after seq N |
| `activate` | `target` | Activate window |
| `close` | `target` | Close window |
| `minimize` | `target` | Minimize window |
| `maximize` | `target` | Toggle maximized |
| `fullscreen` | `target` | Toggle fullscreen |
| `move` | `target, x, y` | Move window |
| `resize` | `target, width, height` | Resize window |
| `workspace` | `target, workspaceId` | Move to workspace |
| `move_cursor` | `x, y` | Move cursor |
| `event_motion` | `x, y` | Pointer motion |
| `event_button` | `button, action?` | Pointer button (left/right/middle) |
| `event_key` | `key, action?` | Key event (Qt key name or code) |
| `screenshot_output` | `name?` | Screenshot of an output (PNG image) |
| `screenshot_window` | `target` | Screenshot of a window (PNG image) |

`target` accepts a numeric window id or an `appId` string. Screenshots are
returned as MCP `image` content blocks with base64-encoded PNG data.

**Claude Desktop configuration** (`claude_desktop_config.json`):

```json
{
  "mcpServers": {
    "treeland-debug": {
      "url": "http://localhost:8080/mcp"
    }
  }
}
```

For other MCP clients, use the same URL as the server endpoint. No headers or
authentication are required.

## JSON output

`--json` is available on every inspection command and prints the same data the
HTTP API returns. A window record includes the stable `id`, `appId`, `title`,
`type` (the numeric shell-surface type) alongside `shellType` (its name), the
geometry, state, `outputs` (every output the window is on),
`framePacingOutput` (the output driving its frame callbacks: the entered output
with the highest refresh rate) and (for `clients`) the owning client
`pid`/`executable`/`command` (the full command line, read from
`/proc/<pid>/cmdline`). The `output` field is the output the compositor treats
as the window's owner -- a window spanning several outputs reports all of them
in `outputs` (taken from the compositor's window wrapper, so a pre-launch
splash window reports its outputs too). The `windows`/`clients`/`tree` views
list every output and prefix the frame-pacing one with `+` (`*` keeps its
existing meaning there: the active window).

The single-window views -- `window <id>`, `GET /api/window` and the MCP
`window` tool -- carry what the one-line lists leave out: the server-side
decoration geometry (`geometry` is the content rect, `titlebarGeometry` the SSD
titlebar, `boundingRect` the whole frame), the frame counter, the damage rect,
and the window's input and opaque regions.
Those two regions are sets of surface-local boxes (`inputRegion`/`opaqueRegion`
in JSON, a list of `{x,y,width,height}` rects; the text views print the first
few boxes and summarise the rest).

`(empty)` means the committed region has no boxes. A `NULL` input region is
*not* empty: the protocol defines it as infinite, and wlroots clips every region
to the surface size, so it is reported as one box covering the whole surface.
An empty `inputRegion` therefore means the client committed an empty region
(the surface accepts no input), or the surface has not committed a buffer yet.
The same applies to `opaqueRegion`: empty means no opaque area was declared
(also its value before the first commit), not that the window is actually
transparent.

`damage`/region data is serialised on every `windows`/`clients` call, so a
client committing a region with thousands of boxes inflates the polls of `top`
as well (which does not display them); use `window <id>` when you need the
regions and a cheap `windows` poll while you look at something else.

`WindowInfo` is a `WindowTreeRemote` POD, and Qt Remote Objects serialises a POD
by field order with no version negotiation: a `treeland-debug` built from a
different revision than the running `treeland` can mis-parse every window after
the first. Build both from the same revision.

## License

treeland is licensed under Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only.
