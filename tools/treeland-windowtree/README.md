# treeland-windowtree

Python client for Treeland's `WindowTreeRemote` Qt Remote Object.

The client is built from the static Replica generated from
`src/treeland_windowtree.rep`; it does not use `QRemoteObjectDynamicReplica`.

## Build

```bash
uv sync
```

`uv` installs the Python build requirements. The native extension also needs the
system Qt6 Remote Objects development tools:

- `pkg-config`
- Qt6 Core and RemoteObjects development files
- `repc` and `moc` from Qt6

The build defaults to `/usr/lib/qt6/libexec/repc` and
`/usr/lib/qt6/libexec/moc`. Override with `REPC=/path/to/repc` or
`MOC=/path/to/moc` if needed.

## Install

Install this library into the current uv environment:

```bash
uv pip install -e .
```

Install it into another uv project so that project can import
`treeland_windowtree`:

```bash
cd /path/to/other/project
uv pip install -e /home/uos/Downloads/treeland-windowtree
```

Verify the install:

```bash
uv run python -c "from treeland_windowtree import WindowTreeClient; print(WindowTreeClient)"
```

For a non-editable install, use:

```bash
uv pip install /home/uos/Downloads/treeland-windowtree
```

## Usage

```python
from treeland_windowtree import WindowTreeClient

client = WindowTreeClient()
layers = client.get_full_layout_tree()
cursor = client.cursor_position()
```

CLI:

```bash
uv run treeland-windowtree
uv run treeland-windowtree --cursor
```

Default connection values match Treeland:

- URL: `local:org.deepin.dde.treeland.debug`
- Replica name: `WindowTree`

The returned tree is converted to Python dictionaries/lists, including nested
`QList<LayerInfo>`, `QList<WorkspaceInfo>`, and `QList<WindowInfo>` values.

## License

treeland is licensed under Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only.
