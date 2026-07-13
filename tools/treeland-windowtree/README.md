# treeland-windowtree

Python client for Treeland's `WindowTreeRemote` Qt Remote Object.

The client uses the static Replica generated from
`src/modules/resource/treelandwindowtree.rep`; the definition is copied into
the source distribution so isolated wheel builds do not require the Treeland
source tree.

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

Build distributable artifacts, including an isolated wheel:

```bash
uv build
```

The resulting source distribution and wheel are written to `dist/`.


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

### CMake installation

The top-level CMake build can build and install the client without `uv`.
Install the distribution's pybind11 CMake development package first (for
example, `pybind11-dev` on Debian-based distributions), then run from the
Treeland repository root:

```bash
cmake -S . -B build -DBUILD_TREELAND_WINDOWTREE=ON
cmake --build build --target treeland-windowtree-core
sudo cmake --install build --component treeland-windowtree
```

The install step copies the Python package to the Python site-packages
directory and installs the `treeland-windowtree` launcher to
`/usr/local/bin` by default.

Run the CMake-installed client as `dde`:

```bash
sudo -u dde -- /usr/local/bin/treeland-windowtree
```


### uv installation for DDE mode

Treeland runs as the `dde` user in global mode. Its Qt Remote Object server
uses owner-only local socket access, so install the wheel in a root-owned,
world-readable virtual environment and run this client as `dde`. Do not install
the wheel into the externally managed system Python.

```bash
uv build
sudo install -d -m 0755 /opt/treeland-windowtree
sudo "$HOME/.local/bin/uv" venv \
  --python /usr/bin/python3 \
  /opt/treeland-windowtree/venv
sudo "$HOME/.local/bin/uv" pip install \
  --python /opt/treeland-windowtree/venv/bin/python \
  dist/*.whl
sudo -u dde -- /opt/treeland-windowtree/venv/bin/python \
  -m treeland_windowtree.cli
```

Read the cursor position with:

```bash
sudo -u dde -- /opt/treeland-windowtree/venv/bin/python \
  -m treeland_windowtree.cli --cursor
```

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
