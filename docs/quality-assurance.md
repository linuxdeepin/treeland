# Treeland 自动化测试与质量保证方案

## 目标

Treeland 是基于 wlroots、Waylib 和 QtQuick 的 Wayland compositor，质量风险主要集中在协议生命周期、窗口/输出状态机、Qt 对象所有权、系统集成和可视 UI 行为。测试体系应按层拆分，避免只依赖“能编译通过”。

## PR 必跑门禁

- `commitlint`：保持 `fix:`, `feat:`, `refactor:` 等提交格式。
- `clang-format`：基于仓库 `.clang-format` 检查 C/C++ 格式。
- `cppcheck`：覆盖常见 C/C++ 静态缺陷。
- CMake 构建：至少覆盖 `WITH_SUBMODULE_WAYLIB=ON`。
- CTest：运行 `tests/` 中注册的单元测试和协议测试。

推荐命令：

```bash
cmake -B build -DWITH_SUBMODULE_WAYLIB=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

## 单元测试

单元测试优先覆盖不依赖真实 compositor 会话的模块，例如：

- `src/input/gestures.*`：手势方向、进度、触发/取消状态机。
- `src/utils/propertymonitor.*`：Qt 属性变更监听。
- `src/utils/cmdline.*`：命令行参数拆分与转义。
- 可拆离的窗口状态、输出配置、快捷键解析逻辑。

新增测试放在 `tests/unit/test_<module>/`，每个目录包含独立 `CMakeLists.txt`，并从 `tests/unit/CMakeLists.txt` 汇总。

### 单元测试使用方法

首次运行前先完成 CMake 配置和构建：

```bash
cmake -B build -DWITH_SUBMODULE_WAYLIB=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

运行所有单元测试：

```bash
ctest --test-dir build -R '^test_(gestures|propertymonitor)$' --output-on-failure
```

运行单个测试目标：

```bash
ctest --test-dir build -R test_gestures --output-on-failure
ctest --test-dir build -R test_propertymonitor --output-on-failure
```

也可以直接执行构建产物，便于本地调试 Qt Test 输出：

```bash
./build/tests/unit/test_gestures/test_gestures
./build/tests/unit/test_propertymonitor/test_propertymonitor
```

新增单元测试时，按现有目录结构创建 `tests/unit/test_<name>/main.cpp` 和
`tests/unit/test_<name>/CMakeLists.txt`，再把子目录加入 `tests/unit/CMakeLists.txt`。

## Wayland 协议测试

继续扩展现有 `tests/test_protocol_*`。每个协议至少覆盖：

- global bind / destroy 生命周期。
- request 参数边界和非法请求处理。
- event 顺序。
- 多 client 并发。
- client 提前退出时资源释放。
- compositor 退出时无 UAF、double free 或悬挂回调。

优先补充 `shortcut`、`personalization`、`window-management`、`capture`、`foreign-toplevel`、`dde-shell` 和 `app-id-resolver`。

## Headless 集成测试

建立轻量 compositor harness，在虚拟输出或 headless backend 中启动 Treeland，再启动测试 client。关键场景：

- 窗口 map / unmap / close。
- 焦点切换、最大化、全屏和 show desktop。
- 输出增删、scale 和 transform 变化。
- 锁屏路径：`DDM` 与 `EXT_SESSION_LOCK_V1`。
- client 异常退出和协议资源清理。

失败时保留 Treeland 日志、client 日志、截图和 core backtrace 作为 CI artifact。

当前仓库提供了初始 harness：`tests/integration/headless`。默认 CTest 只执行
`treeland --try-exec` 的 headless 环境 smoke test；真实 compositor 启动测试需要显式开启：

```bash
TREELAND_RUN_HEADLESS_COMPOSITOR=1 ctest --test-dir build -R test_headless_treeland --output-on-failure
```

后续可在这个 harness 中继续加入测试 client 启动、窗口 map/unmap、协议 round-trip 和失败日志归档。

### Headless 集成测试使用方法

默认运行 headless harness：

```bash
ctest --test-dir build -R test_headless_treeland --output-on-failure
```

默认模式只验证 `treeland --try-exec` 可以在 headless 环境中启动并正常退出，适合放入
PR CI。测试会通过 CTest 自动传入 `TREELAND_TEST_BINARY=$<TARGET_FILE:treeland>`。

开启真实 compositor smoke test：

```bash
TREELAND_RUN_HEADLESS_COMPOSITOR=1 ctest --test-dir build -R test_headless_treeland --output-on-failure
```

该模式会设置临时 `XDG_RUNTIME_DIR`、`WLR_BACKENDS=headless`、
`WLR_RENDERER=pixman` 和独立 `WAYLAND_DISPLAY`，启动 Treeland 后等待 Wayland
socket 创建，然后终止进程。若本机 wlroots/headless backend、渲染器或运行时权限不完整，
该测试可能失败；这种模式更适合 nightly、专用 CI runner 或本地验证。

直接调试时可手动指定二进制：

```bash
TREELAND_TEST_BINARY=./build/src/treeland ./build/tests/integration/headless/test_headless_treeland
```

## Qt/QML 与视觉回归

对 lockscreen、multitask view、window picker、wallpaper 等 QML UI 做两类测试：

- Qt Quick Test：组件能加载，required property 完整，signal/model 更新正确。
- 截图 smoke：固定分辨率虚拟输出，检查关键区域非空、主要元素存在。避免早期使用严格像素对比。

## Sanitizer 与压力测试

每晚或手动 CI 跑 ASan/UBSan：

```bash
cmake -B build-asan -DADDRESS_SANITIZER=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-asan
ctest --test-dir build-asan --output-on-failure
```

压力测试应循环创建/销毁 client、切换窗口焦点、模拟输出变化、切换壁纸、lock/unlock 和 capture 会话，用于暴露生命周期问题。

## Packaging QA

发行前验证 Debian/deepin/Arch 构建，并检查安装产物：

- D-Bus service 与 systemd unit 可解析。
- session 文件、dconfig schema、QML import 路径有效。
- plugins 安装到 `TREELAND_PLUGINS_INSTALL_PATH`。
- `misc/` 与 `protocols/` 变更包含兼容性说明。
