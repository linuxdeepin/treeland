# 协议测试指南

本目录用于测试 Treeland 对 Wayland 协议的实际服务端行为。每个 CTest
可执行文件都在进程内启动一个最小、独立的 headless `WServer`；协议客户端
必须使用 C 和 `wayland-scanner` 生成的 client API 连接该 server。不要让多个
协议共用一个测试 server，也不要用 C++ client 替代协议端的 C 测试。

## 新增一个协议测试

为每个协议创建 `tests/protocols/<protocol-name>/`，通常只需要以下三个文件：

```text
<protocol-name>/
├── CMakeLists.txt
├── setup.cpp       # C++：服务端最小装配
└── client_test.c   # C：真实 Wayland client、断言和事件 listener
```

在本目录的 `CMakeLists.txt` 中加入 `add_subdirectory(<protocol-name>)`，然后在
协议目录写：

```cmake
treeland_add_protocol_test(
    NAME example_v1
    XML "${TREELAND_PROTOCOLS_DATA_DIR}/example-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/client_test.c"
)
```

`NAME` 用下划线，供 CMake target 使用；scanner 输出文件名由 `XML` 自动推导，
client 中应 include `<xml-basename>-client-protocol.h`。不要自行猜测或复制其他
协议的生成文件名。

## 服务端与客户端契约

`setup.cpp` 必须实现：

```cpp
void protocol_test_setup(WServer *server)
{
    server->attach<MyProtocolInterface>();
}
```

只挂载该协议实际依赖的 globals，例如 `WSeat`、output 或 compositor；若测试
请求要求 `wl_output`，fixture 必须创建可被 client bind 的 output，不能在没有
output 时把该 case 标记为通过。

`client_test.c` 必须实现：

```c
int protocol_test_run(const char *socket_name);
```

使用 `protocol_test_connect()`、`protocol_test_bind()` 与
`protocol_test_disconnect()` 管理 registry 和连接。client 代码只能使用 C、
`wayland-client` 和 scanner 生成的 C API，不能 include Qt 或 Treeland C++ API。

测试以具名函数和 `cases[]` 表组织；每个 case 在发 request 后完成 roundtrip，
并明确断言服务端状态或 client listener 收到的 event。禁止使用 `case 0`、
`static step` 一类跨 case 状态机。`protocol_test_invoke_server()` 只用于没有
client request 对应的刻意服务端刺激，不能用它伪造某个 request 的业务结果。

公共/upstream 协议也使用这套 client 框架，但其 server wrapper 应遵循
`upstream-wayland-protocol-wrapper` 规范；Treeland 自有 `treeland_*` 协议遵循
`treeland-private-wayland-protocol` 规范。

## 运行

```bash
cmake --build --preset default --target test_example_v1
ctest --test-dir build -R '^test_example_v1$' --output-on-failure
ctest --test-dir build -R '^test_example_v1$' -V
```

`ctest` 默认仅在失败时显示 client 的 stdout；使用 `-V` 可看到 `cases[]` 中每个
case 的结果。
