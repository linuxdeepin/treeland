# Protocol test framework

Each protocol test is one CTest executable with a private, headless `WServer`.
The protocol client is always C and talks to the server through the generated
Wayland client bindings. The server setup is C++ because it attaches Treeland,
waylib, or wlroots interfaces.

To add a test, create a directory containing `setup.cpp`, `client_test.c`, and
`CMakeLists.txt`:

```cmake
treeland_add_protocol_test(
    NAME example_v1
    XML "${TREELAND_PROTOCOLS_DATA_DIR}/example-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/client_test.c"
)
```

`setup.cpp` implements `void protocol_test_setup(WServer *)` and attaches the
protocol plus only the globals it needs. `client_test.c` implements
`int protocol_test_run(const char *socket_name)`. It may call
`protocol_test_invoke_server()` when a case needs a server-side test action;
the callback must be an `extern "C"` function defined by `setup.cpp`.

Keep ordinary protocol behaviour driven by client requests and server signals.
Use the server callback only for deliberate test stimuli, such as injecting an
event that has no client request counterpart.
