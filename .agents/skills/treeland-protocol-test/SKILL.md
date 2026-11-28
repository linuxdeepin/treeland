---
name: treeland-protocol-test
description: "在 `tests/protocols` 下新增、扩展、重构或调试 Wayland 协议测试：C 版客户端与 fixture 契约、`treeland_add_protocol_test` CMake 注册、可观察证据的断言写法、CTest 执行与 skip 处理。在创建协议测试目录与 `setup.cpp`、实现 `protocol_test_run()`、用 `client_connect`/`client_bind`/`invoke_on_server_thread`、排查 ctest 超时或残留后台进程时使用。协议实现见 `treeland-protocol` 或 `treeland-protocol-upstream`。"
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
- Accounts fixture 中的 UID 必须在 NSS 中实际存在；真实 DConfig 服务会独立校验目标 UID，仅在 Accounts mock 中声明用户无法通过该校验。配置 setter 异步执行时，发送依赖该配置的协议请求前应确认服务端读回值及生成类属性均已同步。
- 只有在 fixture 需要检查已存在的服务端接口时才用 `find_server_interface<T>(server)`；并断言该生产对象确实存在。
- 请求需要客户端可绑定的 `wl_output` 时，添加 headless output。
- 在 C 客户端中实现 `int protocol_test_run(const char *socket_name);`。
- registry 与连接使用 `client_connect()`、`client_bind()`、`client_disconnect()`。
- 客户端代码只允许 C、`wayland-client` 与 scanner 生成的 C API；不要 include Qt 或 Treeland 的 C++ API。
- `invoke_on_server_thread()` 只用于读取生产状态，或注入没有对应客户端请求的定向 fixture 激励。绝不用它伪造请求结果。

### 像素测试的 fixture 条件

- 框架设置 `TREELAND_DISABLE_WALLPAPER=1`。使用保留 output buffer 的软件渲染路径时，
  需要固定的不透明场景背景来重绘窗口移走后暴露的像素；否则旧窗口残影和阴影累积会
  污染整屏对比。先在没有被测操作时验证重复渲染稳定，再比较操作前后，不能通过
  屏蔽区域或像素容差来掩盖未定位的差异。
- `add_headless_output()` 会立即启用输出，可能使框架先于生产 `OutputManager` 将其加入
  layout。需要真实渲染且发生重复 output/cursor layer 注册时，只向已有 headless backend
  添加输出，让生产路径完成注册和启用；参考
  `tests/protocols/wayland-ext-image-copy-capture-v1/toplevel-setup.cpp`。
- 检查捕获/离屏渲染是否影响屏幕时，先回读已提交 buffer，再单独验证下一次重绘；
  不要在取证前强制重绘，否则会覆盖共享 QSG 节点状态污染等瞬态问题。动画场景用
  capture renderer 的 `beforeRendering` / `afterRendering` 比较 live 节点状态，
  不用跨帧固定矩阵误判正常移动。
- 整图比较需要固定光标/悬停等动态内容。指针夹具可在取基线前隐藏光标，仍保留真实
  seat/QtQuick 输入路径；不要屏蔽差异像素。移动/还原后同时比较完整捕获图像和实际
  output buffer，GPU 或物理 scanout 未实测时明确记录，不能用 pixman 通过代替。
- renderer 的 preferred read format 不一定有 QImage 映射；明确选择支持的读回格式并
  检查分配和读回结果，禁止把空图像地址交给 `wlr_texture_read_pixels()`。夹具窗口引用
  用 `QPointer`，主动销毁前暂停“不应变化”的整屏比较，避免异步 frame 回调访问已销毁窗口。

### 捕获 / 录屏类协议的资源生命周期

- 服务端对象的所有者必须是请求它的客户端资源加上 session，而不是被捕获的窗口。
  `accept` 只创建 client resource；离屏视口/副本子树在第一个 session 的 `start()` 里建，
  最后一个 `stop()` 里同步释放，最后一个 client resource 也消失后再销毁包装对象。
  窗口失效只负责 `finish()` → `stopped`，不能作为唯一的回收路径。
- source 回收判断与 `finish()` 必须排队到 native teardown 返回之后：`stop()` 仍在
  wlroots 的 `session_destroy()` 里，client resource 的 destroy listener 则早于 native 摘链。
  listener 只摘自己的监听器，不修改 `source.resources`；queued 检查入口先清除去重标志，
  再读取 resource/session 的最新状态。最后一个 `stop()` 仍同步释放视口。
- 用 fixture 直接计数 source 包装对象及离屏视口（`findChildren<WOutputViewport*>()` + `offscreen()`），
  断言：只创建 source 时包装对象为 1、视口为 0；同一 source 的首个 session 创建 1 条
  管线，增加 session 不再增加管线；销毁最后一个 session 后视口回到 0；
  会话期间提前销毁 source resource 时包装对象与视口仍各为 1，且继续能捕获到 frame。
  另外验证空闲 source 独立回收、同批停止/重启，以及 resource/session 都销毁后包装对象归零。
- `request_frame` 的 `schedule_frame` 只表示该 session 自上次 frame 后累积了 damage，
  不能作为是否存在待处理请求的标志。copy manager 的 `new_session` 接到
  `WExtImageCaptureSourceV1Impl::trackSession()`，从公开的 `session.frame` 跟踪 capturing
  frame，并在其 destroy 事件中移除；强制出帧需求也必须按 frame 记录，取消首帧不能
  带着另一条静止请求继续绘制。无累计 damage、无内容变化的请求保持挂起，不重绘、不复制。
- 捕获视口只在需要绘制时启用 `live`。视觉 commit、子面新增/删除/堆叠/位置、视口参数
  与独立 QML 子树变化都要标脏，并唤醒已有请求，包括借用输出关闭的情况。空提交与
  frame-callback-only commit 不标脏；绘制产生的 renderer 状态变化与非递归
  `ShaderEffectSource` 的层纹理刷新不能当作新内容，真正的纹理参数变化仍须通知。
  强制绘制静止内容时发送空 source damage，避免给其他静止 session 制造 damage。
- 以真实输出 frame / `renderEnd` 同步静止断言，不加 sleep 或轮询。覆盖请求之后才发生
  的主面/子面/SSD 变化、静止请求跨暂停恢复、单帧取消、不同 session 的首帧，以及
  关屏兜底与失败恢复。像素断言的刺激必须可见，例如标题区域应有足够宽度。
- disabled output 不发送 frame，借用其时钟的离屏捕获需要独立的排队渲染兜底。
  用真实 power-management 请求覆盖：先发 capture 再在同一批请求中关屏、关屏后
  提交新内容并再次捕获、复电和取消排队请求；关闭原输出后重建会话，应解析窗口当前
  可用的输出。fixture 只负责添加第二块输出、移动窗口以及读取生产状态，不替代关屏
  或捕获请求；同时断言视口复用和兜底不提交真实输出。

## 测试形态与证据
- 每个语义用例都要有名字。发出请求后用 roundtrip、指定事件、信号、模型变化或 future 完成来同步；不要用随意 sleep 或跨用例的状态机。
- 命名为 `dispatch` 的用例只能证明可分发，不能证明业务语义覆盖。
- 有依赖关系的步骤放在同一个 `protocol()` 用例和同一条 Wayland 连接里。只有能从新连接重新开始的场景才拆分。
- 断言客户端事件载荷或可观察的生产结果。“没崩溃”不算成功，headless 输入与渲染用例尤其如此。
- 覆盖范围或剩余边界变化时，更新该协议目录的 `README.md` 与 `tests/protocols/INDEX.md`。

## 验证
`tests/protocols/` 整棵树只在 `BUILD_TREELAND_PROTOCOL_TESTS` 为 `ON`（默认值）时构建。`TREELAND_ENABLE_GPU_PROTOCOL_TESTS`（DMA-BUF 渲染器）和 `TREELAND_ENABLE_UINPUT_PROTOCOL_TESTS` 默认 `OFF`，对应可选用例不注册到 CTest；开启 uinput 测试后，没有 `/dev/uinput` 写权限或可用 `dde-seatd` 时运行时 skip。`DISABLE_XWAYLAND=ON` 时不构建或注册 xwindow-control 测试。

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

`ProtocolTest.cmake` 通过 `ENVIRONMENT_MODIFICATION` 设置 `QT_NO_XDG_DESKTOP_PORTAL=1`，禁用 Qt 的 portal 集成；fixture 覆盖 `ENVIRONMENT` 不影响该设置。修改后需重新 configure，更新 CTest 配置。

保留 `test-dconfig-service` 的独立进程组和 `stop()` 整组终止逻辑，不要改回只终止单个 PID 或只按 PPID 回收；机制见 `tests/protocols/framework/README.md`。

排查残留：

```bash
# 1. 隔离总线与 DConfig 进程
ps -eo pid,ppid,pgid,etimes,args | grep -E 'unix:abstract=treeland-protocol-test|dde-dconfig-daemon -p /tmp/treeland-protocol-dconfig'

# 2. DConfig 临时目录（调用 stop() 后应被清理）
find /tmp -maxdepth 1 -name 'treeland-protocol-dconfig-*'
```

若 QTest 全部 PASS、测试进程已退出，但 CTest 仍超时，先确认生成的 CTest 配置包含上述设置，再检查是否有后台服务持有输出管道。不要将这种后台进程残留误判为协议失败，或用增加 timeout、标记 skip 掩盖问题。
