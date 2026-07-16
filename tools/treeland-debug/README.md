# treeland-debug

`treeland-debug` is an extensible C++ command-line inspector for Treeland
debug Remote Objects. Its initial inspector reads `WindowTreeRemote` through
the static Replica generated from `src/modules/resource/treelandwindowtree.rep`
and prints the result as JSON.

## Build and install

The client requires CMake, Qt6 Core, Qt6 RemoteObjects, and the Qt6 `repc` and
`moc` tools. Build it from the Treeland repository root:

```bash
cmake -S . -B build -DBUILD_TREELAND_DEBUG=ON
cmake --build build --target treeland-debug
sudo cmake --install build --component treeland-debug
```

The executable is installed to `/usr/local/bin/treeland-debug` by default.
Set `CMAKE_INSTALL_PREFIX` during configuration to use another prefix.

## DDE mode

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

## Usage

Print the complete layout tree:

```bash
sudo -u dde -- /usr/local/bin/treeland-debug --tree
```

`--tree` is the default when neither `--tree` nor `--cursor` is specified.

Print the cursor position:

```bash
sudo -u dde -- /usr/local/bin/treeland-debug --cursor
```

Connection options:

```bash
treeland-debug \
  --url local:org.deepin.dde.treeland.debug \
  --name WindowTree \
  --timeout-ms 30000
```

Default values match Treeland:

- URL: `local:org.deepin.dde.treeland.debug`
- Replica name: `WindowTree`

The layout JSON contains layers, workspaces, windows, geometry, visibility,
activation state, and the current Treeland mode.

## License

treeland is licensed under Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only.
