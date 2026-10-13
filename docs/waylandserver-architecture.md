# WaylandServer composition

`WaylandServer` is the `Treeland.WaylandServer` QML singleton and the
C++ desktop entry point. It composes the Wayland runtime and feature managers.
The former `Helper` compatibility class has been removed; production C++, QML,
plugins and protocol fixtures now refer directly to `WaylandServer`.
Its properties, enums, signals, invokables and slots retain their behavior.

## Responsibilities

| Component | Responsibility |
| --- | --- |
| `WaylandServer` | Owns the Wayland runtime and QtQuick scene, creates globals, composes feature services, coordinates desktop modes and applies ready user settings. |
| `CompositorInputFilter` | Preserves the ordered input dispatch path, event-seat scope, gestures, shortcuts and relative pointer forwarding. |
| `FocusController` | Implements activation policy and activation protocol requests; delegates per-seat focus state to `SeatSurfaceManager`. |
| `SurfaceProtocolController` | Binds surface lifecycle to decoration, personalization, foreign-toplevel and region-watch protocols. |
| `DragController` | Owns per-seat drag listener bundles and forwards existing drag notifications. |
| `IdleManager` (`src/idle/`) | Owns idle protocol listeners and evaluates the existing inhibition policy. |
| `CaptureManager` (`src/capture/`) | Binds capture protocols, manages the selection UI and handles foreign-toplevel capture requests. |
| `ShowDesktop` (`src/showdesktop/`) | Owns show-desktop state, transitions and focus restoration. |
| `LockScreenManager` (`src/lockscreen/`) | Binds lock-screen implementations, logind notifications and external session locks. |
| `DConfigManager` (`src/dconfig/`) | Owns cached configuration objects, waits for active-user/seat configuration initialization and discards stale user-switch completions. |

Existing `SeatManager`, `OutputManager`, `ShellHandler`, `SessionManager`,
`InputManager` and `WallpaperManager` retain their responsibilities.
The new feature controllers receive dependencies explicitly and do not look up
`WaylandServer::instance()`. Callers use the WaylandServer API directly.

## Startup order

`WaylandServer::init()` composes the following stages:

1. Connect application/session signals and obtain QML singleton models.
2. Set up the scene, backend and seat manager.
3. `installDesktopProtocols()` registers the protocols and QML types installed
   before starting the display.
4. Start the `WServer` display.
5. `initializeSeatsAndShell()` installs the input filter, assigns devices and
   initializes the shell.
6. `installRuntimeProtocols()` creates rendering resources and installs the
   remaining capture, surface, input, session-lock and desktop protocols.
7. Start the backend and notify `OutputManager` that its initial scan has started.

This extraction preserves the original ordering, protocol versions, failure
branches and asynchronous output/configuration handling. Starting the display
still does not mean output initialization has completed.

## Lifetime boundaries

`WaylandServer` owns `WServer` as a QObject child and owns the renderer and
allocator handles through `WUniquePointer`, which invokes their wlroots destroy
functions. It observes the display-owned compositor handle through `WPointer`;
`WPointer` does not transfer ownership.

Feature controllers own their listener state. `IdleManager` and
`CaptureManager` detach their top-level native listeners before scene teardown.
The shell and root scene are removed while feature bindings still exist. Remaining
controller state is then released, before QObject child cleanup destroys the
Wayland runtime. Drag listener ownership remains grouped by `WListenerOwner`.

QML properties and desktop methods forward to the corresponding controller.
Desktop mode remains coordinated by `WaylandServer`; per-seat focus state
remains in `SeatSurfaceManager`. `DConfigManager::configReady` feeds the
existing configuration-application path, including `configChanged()` and input,
wallpaper, cursor and workspace synchronization.

This is an architectural extraction. It does not add new compositor behavior,
change existing policies, or add a runtime restart contract.
