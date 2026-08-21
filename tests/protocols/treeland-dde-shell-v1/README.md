# `treeland-dde-shell-v1` 测试规范

## 范围

- 测试源码：`tests/protocols/treeland-dde-shell-v1/`、`tests/protocols/treeland-dde-shell-desktop-v1/`、`tests/protocols/treeland-dde-shell-lockscreen-desktop-v1/`、`tests/protocols/treeland-dde-shell-multitask-desktop-v1/`、`tests/protocols/treeland-dde-shell-picker-desktop-v1/`
- Fixture：协议 fixture；带 headless output 和 mapped xdg-toplevel 的 desktop fixture。
- 覆盖等级：DDE surface 元数据为 **E**；辅助 DDE 资源为 **P/I**。

协议 fixture 中标记为 `*_dispatch` 的 case 只证明 request 被生产资源接受且连接未发生
protocol error；它们计入 XML request 覆盖，不计入业务语义覆盖。每个 shell-surface 元数据
request 则在同一 case 的 roundtrip 后读取服务端状态，分别断言其结果。

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 |
| --- | --- | --- |
| DDE surface 元数据 | `get_shell_surface` 后依次发送 position `(42,24)`、Overlay role、auto-placement `37`、四个布尔标志 | 生产 `SurfaceWrapper` 成为 DDE/Overlay，读取到相同 position、placement、skip 和 focus 状态 |
| 锁屏 | `get_treeland_lockscreen` 后发送 `lock` | 真实 `LockScreen` container 可用且可见；`Helper` 从 Normal 切换为 LockScreen 模式 |
| 窗口选择器 | 创建并 map xdg-toplevel，`get_treeland_window_picker` 后发送 `pick("protocol picker")` | 真实 `WindowPicker` 创建；选择该 mapped `WSurfaceItem` 后客户端收到其真实 Wayland client PID |
| manager 资源 | 创建 checker、active、multitask、picker、lockscreen | 资源均可创建并接收规定事件 |
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

窗口选择器桌面测试复用真实 mapped xdg-toplevel。客户端先创建
`treeland_window_picker_v1`，再发送 `pick("protocol picker")`。生产侧的时序为：

```text
get_treeland_window_picker
  → DDEShellManagerInterfaceV1::PickerCreated
  → Helper::handleWindowPicker（预先连接 WindowPickerInterface::pick）
pick("protocol picker")
  → WindowPickerInterface::pick
  → Helper 创建真实 WindowPicker 并设置 hint
  → WindowPicker::selectWindow(mapped WSurfaceItem)
  → WindowPicker::windowPicked
  → WClient::getCredentials
  → WindowPickerInterface::sendWindowPid
  → 客户端 treeland_window_picker_v1.window(pid)
```

这里的 `selectWindow()` 是 `WindowPicker` 的统一生产选择入口；鼠标按下路径也会调用它。
测试不以伪造协议事件或 hover 坐标猜测来代替选中：它将 fixture 已验证为 mapped 且在
workspace 中的真实 `WSurfaceItem` 交给该入口，并断言客户端事件中的 PID 等于测试客户端
自身 PID。等待只由 Wayland round-trip 和该事件驱动，不使用 timeout。

该测试同时覆盖并防回归一个真实时序缺陷：若 `Helper` 在
`requestPickWindow` 后才连接 `WindowPickerInterface::pick`，首条 `pick` 请求已经发出，
因而永远不会创建 picker。生产连接必须建立在 `PickerCreated`，再由后续 `pick` 创建 QML
picker。

## 已知边界 / 下一项结果

lockscreen 的 `lock` 已有 **E** 级流程覆盖。窗口选择器的上述 **E** 级测试已实现，
但本次修改后的测试仍须在当前提交上成功执行后，才能把它标为已验证结果。
`shutdown`/`switch_user` 会进入外部会话或 greeter 行为，尚未在测试中替代这些系统服务。
multitask 仍只验证 `toggle` 信号，尚未验证真实插件/UI 的进入、退出和状态；overlap checker
仍为资源/事件覆盖。
