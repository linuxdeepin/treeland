# `DesktopIntegrationFixture` 测试规范

这不是一个 Wayland 私有协议，而是所有 desktop 协议测试共用的生产桌面基座。

## 建立过程

1. `protocol-test-desktop-main.cpp` 调用 `Treeland::preInit({ .headless = true })`，
   创建 `QGuiApplication` 和完整生产 `Treeland`。
2. `Treeland` 初始化已经创建 `Helper`；因此 `Helper` 所有正常初始化连接都存在，
   包括 `ShellHandler`、`Workspace`、`RootSurfaceContainer`、seat、渲染窗口以及各
   协议 manager。
3. 每个 desktop 测试的 `protocol_test_desktop_setup()` 调用
   `protocol_test_create_headless_output(helper->backend(), false)`。backend 已启动，
   新 output 的生产回调创建 root output/container 条目；主循环确认
   `rootSurfaceContainer()->outputs()` 非空、且 target 的可选
   `protocol_test_desktop_ready()` 返回 true 后，才启动客户端线程。runner 会在 global
   或 user DConfig 初始化完成时重新检查该条件。
4. 客户端使用 `protocol_test_xdg_toplevel_create()`：创建 `wl_surface`、
   `xdg_surface`、`xdg_toplevel`，等待 configure，提交并 map。
5. 生产 `ShellHandler::surfaceWrapperAdded` 发出时，fixture 保存该
   `SurfaceWrapper`，并断言 `workspace()->surfaces()` 包含它。

## 基线断言

| 客户端动作 | 生产对象 | 必须断言 |
| --- | --- | --- |
| 创建并 map xdg-toplevel | headless backend / root output | 至少一个真实 root output 已存在 |
| 同上 | `ShellHandler` | 发出 `surfaceWrapperAdded` |
| 同上 | `Workspace` | 新 `SurfaceWrapper` 已加入 `workspace()->surfaces()` |

## 适用范围与边界

该 fixture 证明窗口进入生产桌面对象图，但不自动证明某项协议已产生业务效果。各
协议文档必须继续说明：客户端具体发送的请求、哪个生产对象接收请求、读取了哪个
状态或事件作为结果。它不是 `RenderedOutputFixture`：没有统一的像素读回能力。
