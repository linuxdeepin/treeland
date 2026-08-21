# 协议测试指南

本目录用于测试 Treeland 对 Wayland 协议的实际服务端行为。每个 CTest
可执行文件都在独立进程内运行，协议客户端必须使用 C 和 `wayland-scanner`
生成的 client API 连接测试 server。不要让多个协议共用一个测试 server，也不要用
C++ client 替代协议端的 C 测试。

每个可执行文件由 QTest 的 `protocol()` 用例组织；CTest 负责启动独立进程、注入 headless
环境、设置超时并将退出码 `77` 记为 Skip。QTest 不替代 C client：协议 request/event 的实际
驱动与断言仍在 `client_test.c` 中。

所有测试均由 `treeland_add_protocol_test()` 启动完整的 `Treeland`、`Helper`、seat、
workspace 和 headless backend。协议对象由生产启动路径创建；测试只取得现有对象并观察其
行为，不重复注册 global。

测试不应因为 headless 环境缺少物理输入或像素结果而把“无崩溃”当作成功；应通过
虚拟键盘、真实 mapped surface、生产信号或协议事件构造并观察确定性结果。

## 新增一个协议测试

为每个协议创建 `tests/protocols/<protocol-name>/`，通常需要以下三个文件：

```text
<protocol-name>/
├── CMakeLists.txt
├── setup.cpp       # C++：服务端最小装配
└── <protocol-name>.c # C：真实 Wayland client、断言和事件 listener
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

每个 `setup.cpp` 必须实现：

```cpp
void protocol_test_setup(Helper *helper)
{
    auto *protocol = find_server_interface<MyProtocolInterface>(helper);
    Q_ASSERT(protocol);
    add_headless_output(helper->backend(), false);
}
```

不得在测试中 `attach` 已由生产启动路径提供的协议 global。若请求要求 `wl_output`，fixture
必须创建可被 client bind 的 output，不能在没有 output 时把该 case 标记为通过。

`client_test.c` 必须实现：

```c
int protocol_test_run(const char *socket_name);
```

`protocol_test_setup()` 由 QTest 的 `initTestCase()` 调用；`protocol_test_run()` 则由
`protocol()` slot 在 client pthread 中调用。后者返回 `0` 表示通过，其他返回值会使该 QTest
用例失败。

使用 `client_connect()`、`client_bind()` 与
`client_disconnect()` 管理 registry 和连接。client 代码只能使用 C、
`wayland-client` 和 scanner 生成的 C API，不能 include Qt 或 Treeland C++ API。

每个测试以具名函数组织；每个语义 case 在发 request 后完成 roundtrip，并明确断言服务端状态
或 client listener 收到的 event。仅验证请求可被派发的 case 必须在名称中标明 `dispatch`，
且不能作为业务语义覆盖统计。禁止使用 `case 0`、`static step` 一类跨 case 状态机。
`invoke_on_server_thread()` 只用于在 compositor 线程读取生产状态或触发没有
client request 对应的刻意刺激，不能用它伪造某个 request 的业务结果。

一个 `protocol()` QTest 用例可以包含同一协议场景的多个有序 C 步骤。例如先创建 resource、
再发送配置 request、最后断言 event 的步骤共享同一 Wayland 连接与资源，不能为了显示成多个
QTest data row 而拆开。只有彼此独立、可从全新 client 连接运行的场景才应拆为独立 target。

公共/upstream 协议也使用这套 client 框架，但其 server wrapper 应遵循
`upstream-wayland-protocol-wrapper` 规范；Treeland 自有 `treeland_*` 协议遵循
`treeland-private-wayland-protocol` 规范。

## 运行

首次配置、切换 preset，或新增测试目录后，先重新配置：

```bash
cmake --preset default
```

以 keyboard-state-notify 为例，测试名和构建 target 均由 `NAME` 转换而来：

```bash
cmake --build --preset default --target test_treeland_keyboard_state_notify_unstable_v1
ctest --test-dir build --output-on-failure \
  -R '^test_treeland_keyboard_state_notify_unstable_v1$'
```

查看完整 Wayland/合成器日志时使用 `-V`：

```bash
ctest --test-dir build -V -R '^test_treeland_keyboard_state_notify_unstable_v1$'
```

运行所有本目录注册的协议测试：

```bash
ctest --test-dir build --output-on-failure -L protocols
```

### 可选 `/dev/uinput` 输入集成测试

`treeland-input-manager-uinput-v1` 默认不注册。它会创建一个 `BUS_VIRTUAL` uinput
键盘，验证该设备经 udev/libinput/wlroots 进入 `WBackend::inputAdded` 和
`inputRemoved` 后，input-manager 向 Wayland client 发送 Keyboard capability 的
available/unavailable 事件；测试不会写入按键事件。

仅在 Linux 且当前用户可写 `/dev/uinput` 时启用：

```bash
cmake --preset default -DTREELAND_ENABLE_UINPUT_PROTOCOL_TESTS=ON
cmake --build --preset default --target test_treeland_input_manager_uinput_v1
ctest --test-dir build --output-on-failure \
  -R '^test_treeland_input_manager_uinput_v1$'
```

没有 `/dev/uinput` 写权限时，runner 在启动前以退出码 `77` 将该用例标记为
**Skipped**；这不是通过。启用开关后执行全量协议测试时，该可选 target 也会被包含：

```bash
ctest --test-dir build --output-on-failure -L protocols
```

优先通过 CTest 运行。CTest 会注入 headless backend、renderer 和超时设置；直接执行
`build/tests/protocols/.../test_*` 会遗漏这些环境设置，输出的 `(EE) could not connect to
wayland server` 也可能只是收尾阶段的 Xwayland 噪声。判断测试是否通过应以 CTest 的退出码
和该用例的明确断言为准。
