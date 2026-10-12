# `treeland-dde-shell-v1` 测试规范

## 范围

- 测试源码：`tests/protocols/treeland-dde-shell-v1/`、`tests/protocols/treeland-dde-shell-desktop-v1/`、`tests/protocols/treeland-dde-shell-lockscreen-desktop-v1/`、`tests/protocols/treeland-dde-shell-multitask-desktop-v1/`
- Fixture：协议 fixture；带 headless output 和 mapped xdg-toplevel 的 desktop fixture。
- 覆盖等级：DDE surface 元数据为 **E**；辅助 DDE 资源为 **P/I**。

> 注意：`treeland_window_picker_v1`（窗口选择器）功能已从服务端与客户端移除，
> 本规范不再描述其实现、生产流程与测试用例。

协议 fixture 中标记为 `*_dispatch` 的 case 只证明 request 被生产资源接受且连接未发生
protocol error；它们计入 XML request 覆盖，不计入业务语义覆盖。每个 shell-surface 元数据
request 则在同一 case 的 roundtrip 后读取服务端状态，分别断言其结果。

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 |
| --- | --- | --- |
| DDE surface 元数据 | `get_shell_surface` 后依次发送 position `(42,24)`、Overlay role、auto-placement `37`、四个布尔标志 | 生产 `SurfaceWrapper` 成为 DDE/Overlay，读取到相同 position、placement、skip 和 focus 状态 |
| 锁屏 | `get_treeland_lockscreen` 后发送 `lock` | 真实 `LockScreen` container 可用且可见；`Helper` 从 Normal 切换为 LockScreen 模式 |
| manager 资源 | 创建 checker、active、multitask、lockscreen | 资源均可创建并接收规定事件 |
| shell 资源状态 | 在协议 fixture 中使用普通 `wl_surface` | 协议对象记录 shell-surface 状态与销毁 |

## 已证明的生产链路

客户端先创建并 map xdg-toplevel；共享 desktop fixture 断言 `ShellHandler` 已创建并由
`Workspace` 持有对应 `SurfaceWrapper`。在 xdg surface 建立期间，客户端调用
`get_shell_surface(surface)`，随后发送：`set_surface_position(42, 24)`、
`set_role(OVERLAY)`、`set_auto_placement(37)`、`set_skip_switcher(1)`、
`set_skip_dock_preview(1)`、`set_skip_muti_task_view(1)` 和
`set_accept_keyboard_focus(0)`。

服务端 round-trip 后并不读取 DDE 协议资源的缓存字段，而是读取该真实 wrapper：
`isDDEShellSurface()`、`surfaceRole()`、`clientRequstPos()`、`autoPlaceYOffset()`、
`skipSwitcher()`、`skipDockPreView()`、`skipMutiTaskView()`、
`acceptKeyboardFocus()`。随后客户端把 `skip_dock_preview` 改为 `0`，同一 wrapper
必须立即反映 `0`。这证明动态请求到达窗口管理对象。

锁屏桌面测试在真实 `Helper` 和已加载的 lockscreen 插件上创建 DDE lockscreen 资源。
发送 `treeland_lockscreen_v1.lock` 前，测试确认真实 `LockScreenContainer` 可用、未锁定，
且 `Helper::currentMode() == Normal`；请求之后读取生产对象，要求
`LockScreen::isLocked()` 为真且 `Helper::currentMode() == LockScreen`。调用链为：

```text
lock
  → LockScreenInterface::lock
  → Helper::handleLockScreen
  → Helper::showLockScreen
  → Helper::prepareLockScreenTransition
  → LockScreen::lock
```

workspace 的淡出是动画，测试不以任意延时或瞬时 opacity 判断其最终帧；模式转换和
lockscreen 可见性是同步且稳定的生产业务结果。

## 已知边界 / 下一项结果

lockscreen 的 `lock` 已有 **E** 级流程覆盖。
`shutdown`/`switch_user` 会进入外部会话或 greeter 行为，尚未在测试中替代这些系统服务。
multitask 仍只验证 `toggle` 信号，尚未验证真实插件/UI 的进入、退出和状态；overlap checker
仍为资源/事件覆盖。
