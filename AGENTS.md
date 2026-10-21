# Treeland 智能体指南

## 全局认识
- `treeland` 是基于 `wlroots` + Qt6/QtQuick（Scene Graph 渲染）的 Wayland 合成器。
- `wlroots` 以源码内联方式引入：上游源码在 `3rdparty/wlroots`，由 `wlroots/CMakeLists.txt` 编译成 CMake 模块（目标 `Wlroots::wlroots`）。waylib 直接调用 wlroots 的 C API —— 用 `<wlr_all.h>`，不要 include 单个 `<wlr/...>` 头文件 —— 并在此之上提供 Qt/C++ 的对象与生命周期层（`WObject`/`WServerInterface`、`WPointer`、作用域化的 `wl_signal` 监听）。
- `waylib/` 建立在 `wlroots` 之上，提供对 QtQuick 友好的合成器框架（output 对应 `QQuickWindow`，surface 对应 `QQuickItem`）。
- Treeland 自身的策略代码在 `src/`（workspace/seat/input/output、plugin/module、XWayland 集成）。

## Skills
开始任务前先读对应 skill，不要等到第一次尝试失败再读。

| Skill | 适用场景 | 何时读 |
|-------|----------|--------|
| [treeland-build](.agents/skills/treeland-build/SKILL.md) | CMake preset、Debug 构建、安装到 `/usr`、配置选项 | 配置、编译或安装前 |
| [treeland-protocol](.agents/skills/treeland-protocol/SKILL.md) | treeland 自有 `treeland_*` xml、`QtWaylandServer::*`、`src/modules/*` | 新增或调试私有协议 |
| [treeland-protocol-upstream](.agents/skills/treeland-protocol-upstream/SKILL.md) | standard/ext/unstable/wlr 协议、waylib `WServerInterface` 封装 | 新增或调试上游协议 |
| [treeland-protocol-test](.agents/skills/treeland-protocol-test/SKILL.md) | `tests/protocols/`、C 版 Wayland 客户端、CTest 协议测试目标 | 新增或修复协议测试 |
| [treeland-logging](.agents/skills/treeland-logging/SKILL.md) | `treelandlogging.*` / `wayliblogging.*`、日志级别 | 新增、修改或评审日志 |
| [treeland-dconfig](.agents/skills/treeland-dconfig/SKILL.md) | `misc/dconfig/*.json`、生成的配置封装类 | 新增或修改配置项 |
| [treeland-debug](.agents/skills/treeland-debug/SKILL.md) | `treeland-debug` CLI、运行时状态、运行日志 | 复现或诊断运行中的合成器 |

## 代码在哪里
- `src/core/`：生命周期与 QML 引擎胶水（例如 `src/core/qmlengine.*`、`src/core/treeland.*`）。
- `src/surface/`：surface/window 封装、状态/可见性/几何（例如 `src/surface/surfacewrapper.cpp`）。
- `src/seat/`：合成器 seat 管理。桌面启动、协议组装与 Wayland 运行时在 `src/core/waylandserver.*`。
- `src/input/`：输入管理、seat 设备、按用户区分的 seat 配置。
- `src/workspace/`：workspace 模型与切换。
- `src/dconfig/`：`DConfigManager` 与生成的配置封装类。
- `src/plugins/` + `src/modules/`：集成点与功能组合。
- `src/common/treelandlogging.*`：treeland 集中式日志分类。
- `tests/protocols/`：协议集成测试，每个协议一个目录。

## 构建与测试
```bash
cmake --preset ci
cmake --build --preset ci
ctest --preset ci
```

- 日常开发优先用 `ci` preset：它对齐 CI 的 `-Wall -Wextra -Werror` 编译选项，能在本地提前发现会导致 CI 失败的告警。`CMakePresets.json` 里其他 preset：`default`、`clang`、`ci-clang`。
- `WITH_SUBMODULE_WAYLIB=ON|OFF` 选择使用子模块 waylib 还是系统 waylib，默认偏好 `ON`。
- 协议测试在 `BUILD_TREELAND_PROTOCOL_TESTS=ON`（默认值）时构建，用 `ctest --test-dir build -L protocols` 运行。`TREELAND_ENABLE_GPU_PROTOCOL_TESTS`（DMA-BUF 渲染器）默认 `OFF`；需要 `/dev/uinput` 或 `dde-seatd` 的用例始终会构建，运行时自动 skip。

Debug 构建（`-DCMAKE_BUILD_TYPE=Debug`）、安装到 `/usr`、全部配置选项与常见问题见 `.agents/skills/treeland-build/SKILL.md`。

## 项目规则
- 只用 C++20 和 Qt6。
- `connect()` 与信号监听优先用成员函数指针；lambda 仅转发给单个成员函数时不要用 lambda。
- 避免复杂 lambda；lambda 体超过几行就抽成具名成员函数。
- 保持 diff 最小：只碰与任务相关的文件和符号。
- 不要做与任务无关的大范围重构、重命名或重新格式化。
- 新增源文件必须带与仓库一致的 SPDX 行，通常是 `SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only`。非源码文件（配置、资源、翻译、打包）由 `REUSE.toml` 覆盖，不需要逐文件声明。
- 日志相关改动必须遵循 `.agents/skills/treeland-logging/SKILL.md`。
- 如果实际任务中发现仓库内的 skill 有错、过期或不完整，要主动更新该 skill，不要沿用已知错误的指引。

## Wayland 协议路由
协议类改动需要一个实现类 skill；当行为可在协议线上观察时，再加测试 skill：

- Treeland 私有协议：`.agents/skills/treeland-protocol/SKILL.md`
- 上游 wlroots/waylib 协议：`.agents/skills/treeland-protocol-upstream/SKILL.md`
- 协议测试：`.agents/skills/treeland-protocol-test/SKILL.md`

除非任务本身就是重构边界，否则不要在同一次改动里混用两条实现路径。
