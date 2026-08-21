---
name: wayland-protocol-test
description: Use when adding, extending, refactoring, or debugging Treeland Wayland protocol tests under tests/protocols, including protocol test CMake targets, C Wayland clients, setup.cpp fixtures, protocol_test_setup, protocol_test_run, client_connect, client_bind, invoke_on_server_thread, headless outputs, or CTest protocol-test execution. Route protocol implementation itself to treeland-private-wayland-protocol or upstream-wayland-protocol-wrapper as appropriate.
---

# Wayland Protocol Test

## Goal

Exercise the real Treeland server path with a C Wayland client. Do not replace
protocol requests, client events, or production state with a mock or an
equivalent C++ client.

## Read First

1. `tests/protocols/README.md`
2. `tests/protocols/framework/README.md`
3. The target protocol's `README.md`, XML, `CMakeLists.txt`, `setup.cpp`, and C client.
4. `tests/protocols/framework/ProtocolTest.cmake`

For protocol implementation work, select exactly one complementary skill:

- Treeland-owned `treeland_*` XML or `QtWaylandServer::*`: `treeland-private-wayland-protocol`.
- Standard, ext, unstable, or wlroots-backed protocol wrappers: `upstream-wayland-protocol-wrapper`.

## Add A Test

Create `tests/protocols/<protocol-name>/` with:

```text
<protocol-name>/
├── CMakeLists.txt
├── setup.cpp
└── <protocol-name>.c             # C Wayland client
```

Register the directory in `tests/protocols/CMakeLists.txt`, then use the shared
CMake helper:

```cmake
treeland_add_protocol_test(
    NAME example_v1
    XML "${TREELAND_PROTOCOLS_DATA_DIR}/example-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/example-v1.c"
)
```

Use an underscore-separated `NAME`. Derive the generated client header name
from `XML`; include `<xml-basename>-client-protocol.h` rather than guessing or
copying a generated filename. Add dependent protocol XML files with
`EXTRA_XMLS`; the helper generates client bindings for both `XML` and every
extra XML.

## Fixture And Client Contract

Implement the fixture hook using existing production objects:

```cpp
void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);
}
```

- Do not attach a global already provided by Treeland's startup path.
- Use `find_server_interface<T>(helper)` only when the fixture needs to inspect
  an existing server interface; assert that the production object is present.
- Add a headless output when the request requires a client-bindable `wl_output`.
- Implement `int protocol_test_run(const char *socket_name);` in the C client.
- Use `client_connect()`, `client_bind()`, and `client_disconnect()` for the
  registry and connection.
- Keep client code to C, `wayland-client`, and scanner-generated C APIs; do not
  include Qt or Treeland C++ APIs.
- Use `invoke_on_server_thread()` only to read production state or inject a
  deliberate fixture stimulus that has no corresponding client request. Never
  use it to fake a request's result.

## Test Shape And Evidence

- Give each semantic case a name. After a request, synchronise with a
  roundtrip, specified event, signal, model change, or future completion; do
  not use arbitrary sleeps or cross-case state machines.
- A `dispatch`-named case may prove only dispatchability, not business semantic
  coverage.
- Keep dependent steps in one `protocol()` case and one Wayland connection.
  Split only scenarios that can run from a fresh client connection.
- Assert a client event payload or observable production result. "No crash" is
  not success, especially for headless input and rendering cases.
- Update the protocol directory's `README.md` and `tests/protocols/INDEX.md`
  when coverage or remaining boundaries change.

## Validate

Reconfigure after adding a directory or changing a preset:

```bash
cmake --preset default
```

Build and run the named target through CTest. For example:

```bash
cmake --build --preset default --target test_treeland_keyboard_state_notify_unstable_v1
ctest --test-dir build --output-on-failure \
  -R '^test_treeland_keyboard_state_notify_unstable_v1$'
```

Run with `-V` to inspect compositor and Wayland logs. Run the full group with:

```bash
ctest --test-dir build --output-on-failure -L protocols
```

Prefer CTest over directly launching a test executable because it provides the
headless backend, renderer, timeout, and skip environment.
