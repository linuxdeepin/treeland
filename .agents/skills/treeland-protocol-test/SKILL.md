---
name: treeland-protocol-test
description: "用于新增、扩展、重构或调试 `tests/protocols` 下的协议测试。触发场景：协议测试 CMake 目标、C 版 Wayland 客户端、`setup.cpp` fixture、`protocol_test_setup`、`protocol_test_run`、`client_connect`、`client_bind`、`invoke_on_server_thread`、headless output、CTest 协议测试执行。协议实现本身见 treeland-protocol 或 treeland-protocol-upstream。"
---

# Wayland 协议测试

> **铁律**：断言必须来自真实 C 客户端事件或可观察的生产对象状态。mock、换成 C++ 客户端、"没崩溃" 都不算证据。

## 目标
用 C 版 Wayland 客户端跑通真实的 Treeland 服务端路径。不要用 mock、也不要换成等价的 C++ 客户端，去替代协议请求、客户端事件或生产状态。

## 先读
1. `tests/protocols/README.md`
2. `tests/protocols/framework/README.md`
3. 目标协议的 `README.md`、XML、`CMakeLists.txt`、`setup.cpp` 与 C 客户端
4. `tests/protocols/framework/ProtocolTest.cmake`

若属于协议实现工作，再按需选一个实现类 skill：

- Treeland 自有 `treeland_*` XML 或 `QtWaylandServer::*`：`treeland-protocol`
- standard/ext/unstable/wlroots 协议封装：`treeland-protocol-upstream`

## 新增测试

- [ ] 建目录 `tests/protocols/<protocol-name>/`，含 `CMakeLists.txt`、`setup.cpp`、`<protocol-name>.c`（C 客户端）
- [ ] 写 `treeland_add_protocol_test(...)`（模板见下）
- [ ] 在 `tests/protocols/CMakeLists.txt` 里 `add_subdirectory(<protocol-name>)`
- [ ] 在 C 客户端实现 `protocol_test_run()`，并断言真实客户端事件或可观察的生产结果（见「Fixture 与客户端契约」「测试形态与证据」）
- [ ] 覆盖或剩余边界有变化时，更新该协议目录的 `README.md` 与 `tests/protocols/INDEX.md`
- [ ] `cmake --preset default` 后跑 `ctest --test-dir build -R '^test_<name>$' -V`

目录结构：

```text
<protocol-name>/
├── CMakeLists.txt
├── setup.cpp
└── <protocol-name>.c             # C Wayland 客户端
```

在 `tests/protocols/CMakeLists.txt` 中注册该目录，然后使用共享的 CMake helper：

```cmake
treeland_add_protocol_test(
    NAME example_v1
    XML "${TREELAND_PROTOCOLS_DATA_DIR}/example-v1.xml"
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/example-v1.c"
)
```

`NAME` 用下划线分隔；CMake target 是 `test_<name>`（`-` 会转成 `_`，见 `ProtocolTest.cmake`）。生成的客户端头文件名由 `XML` 推导；include `<xml-basename>-client-protocol.h`，不要猜或复制别的生成文件名。需要依赖其他协议 XML 时用 `EXTRA_XMLS`，helper 会为 `XML` 和每个额外 XML 都生成客户端绑定。

## Fixture 与客户端契约
用现有生产对象实现 fixture 钩子：

```cpp
void protocol_test_setup(WaylandServer *server)
{
    add_headless_output(server->backend(), false);
}
```

- 不要重复注册 Treeland 启动路径已经提供的 global。
- 只有在 fixture 需要检查已存在的服务端接口时才用 `find_server_interface<T>(server)`；并断言该生产对象确实存在。
- 请求需要客户端可绑定的 `wl_output` 时，添加 headless output。
- 在 C 客户端中实现 `int protocol_test_run(const char *socket_name);`。
- registry 与连接使用 `client_connect()`、`client_bind()`、`client_disconnect()`。
- 客户端代码只允许 C、`wayland-client` 与 scanner 生成的 C API；不要 include Qt 或 Treeland 的 C++ API。
- `invoke_on_server_thread()` 只用于读取生产状态，或注入没有对应客户端请求的定向 fixture 激励。绝不用它伪造请求结果。

## 测试形态与证据
- 每个语义用例都要有名字。发出请求后用 roundtrip、指定事件、信号、模型变化或 future 完成来同步；不要用随意 sleep 或跨用例的状态机。
- 命名为 `dispatch` 的用例只能证明可分发，不能证明业务语义覆盖。
- 有依赖关系的步骤放在同一个 `protocol()` 用例和同一条 Wayland 连接里。只有能从新连接重新开始的场景才拆分。
- 断言客户端事件载荷或可观察的生产结果。“没崩溃”不算成功，headless 输入与渲染用例尤其如此。
- 覆盖范围或剩余边界变化时，更新该协议目录的 `README.md` 与 `tests/protocols/INDEX.md`。

## 验证
`tests/protocols/` 整棵树只在 `BUILD_TREELAND_PROTOCOL_TESTS` 为 `ON`（默认值）时构建。`TREELAND_ENABLE_GPU_PROTOCOL_TESTS`（DMA-BUF 渲染器）默认 `OFF`，这些用例不会出现在 CTest 里而不是失败；需要 `/dev/uinput` 或可用 `dde-seatd` 的用例始终会构建，运行时 skip。

新增目录或修改 preset 后重新 configure：

```bash
cmake --preset default
```

通过 CTest 构建并运行指定目标，例如：

```bash
cmake --build --preset default --target test_treeland_keyboard_state_notify_unstable_v1
ctest --test-dir build --output-on-failure \
  -R '^test_treeland_keyboard_state_notify_unstable_v1$'
```

加 `-V` 查看合成器与 Wayland 日志。运行整组：

```bash
ctest --test-dir build --output-on-failure -L protocols
```

部分用例按设计会 skip（没有 `/dev/uinput`、没有 GPU、没有 D-Bus 服务）。CTest 把这种情况记为退出码 `77` 或 `SKIP   :` 行，二者都在 `tests/protocols/framework/ProtocolTest.cmake` 中配置；skip 不算覆盖。报告某个场景通过之前，先看 `-V` 输出。

优先用 CTest 而不是直接启动测试可执行文件，因为 CTest 会提供 headless 后端、渲染器、超时与 skip 环境。
