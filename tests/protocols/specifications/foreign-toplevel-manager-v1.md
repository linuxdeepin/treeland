# `treeland-foreign-toplevel-manager-v1` 测试规范

## 范围

- 测试源码：`tests/protocols/treeland-foreign-toplevel-manager-v1/`
- Fixture：带 headless output 和已 configure、mapped xdg-toplevel 的 desktop fixture。
- 覆盖等级：**E**。

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 |
| --- | --- | --- |
| toplevel 发现 | map xdg-toplevel | `ShellHandler` 创建 workspace `SurfaceWrapper`，manager 恰好发送一个带 identifier 的 foreign handle |
| Dock preview | 对该 identifier 发 `show(10,20,BOTTOM)`、未知 identifier、tooltip、context close | 生产 preview/tooltip/close 回调收到预期参数；未知 identifier 产生空 surface 列表而非协议错误 |
| 最小化与关闭 | 对 handle 发 `set_minimized`、`unset_minimized`、`close` | 真实 wrapper 的 minimized 状态变化；xdg client 收到 close |
| 最大化 | 对 handle 发 `set_maximized`、`unset_maximized` | 同一真实 `SurfaceWrapper` 进入、退出 `Maximized` 状态 |
| 全屏 | 对 handle 发 `set_fullscreen(NULL)`、`unset_fullscreen` | 同一真实 `SurfaceWrapper` 进入、退出 `Fullscreen` 状态 |
| 激活 | 对 handle 发 `activate(wl_seat)` | `Helper` 将 wrapper 设为 activated，且它成为该 seat 的真实 keyboard focus surface |
| 窗口代表区域 | 对 handle 的 mapped xdg `wl_surface` 发 `set_rectangle(11,12,130,140)` | production rectangle handler 将局部坐标换算为 wrapper 的 `iconGeometry`：`wrapper.position + QRect(11,12,130,140)` |

## 已证明的生产链路

客户端 map xdg-toplevel 后，测试等待 configure，并同时断言：`ShellHandler` 创建的
wrapper 已 mapped、已加入 `Workspace`，且 manager 的 `toplevel` 事件只给出一个
真实 `treeland_foreign_toplevel_handle_v1` 和 identifier。该 identifier 被放入
`show` 的 `wl_array`，客户端发送 `show(identifiers, 10, 20, BOTTOM)`；服务端生产
manager 发出 `requestDockPreview`，fixture 读取到绝对坐标 `(10,20)`、`BOTTOM` 和
一个真实 `WSurface`。

状态转换保留生产 `SurfaceWrapper` 的几何动画语义。desktop fixture 仅把
`Helper::setAnimationSpeed()` 设为 `0.1f`，缩短而不跳过动画；每次状态请求先 render 一帧，
若 animation 仍在运行，则连接对应 geometry item 的 `finished()` 信号并等待该信号。这个
`QEventLoop` 是有明确完成信号的事件握手，不以固定时长的 sleep、`QTimer` 或轮询猜测动画
是否完成；正常桌面进程的动画速度不受影响。

同一 context 的 `show_tooltip("dock-tooltip", 5, 6, TOP)` 到达
`requestDockPreviewTooltip`；`context.close` 到达 `requestDockClose`。此外，foreign
handle 的 `set_minimized`/`unset_minimized` 分别使该 wrapper 的 shell surface 进入/退出
minimized；`set_maximized`/`unset_maximized` 分别改变 wrapper 的 `Maximized` 状态；
`set_fullscreen(NULL)`/`unset_fullscreen` 分别改变其 `Fullscreen` 状态。这里读取的是
compositor 的 `SurfaceWrapper::surfaceState()`，不是 foreign handle 自身缓存的 state event。

`activate(wl_seat)` 进入 manager 连接的
`Helper::forceActivateSurface(wrapper, ..., seat)`，测试读取 wrapper 的 activated 标记和
root surface container 默认 seat container 的 `keyboardFocusSurface()`，两者都必须指向同一
mapped wrapper。`set_rectangle` 则
经过 `rectangleChanged` 的生产连接，以发送该请求的 mapped xdg surface 查找 dock wrapper，
再把局部矩形加到它的位置后写入被测 wrapper 的 `iconGeometry`。测试断言最终 geometry 的
相对关系，故窗口不在 `(0,0)` 时仍成立。`handle.close` 让客户端 xdg-toplevel 接到 close。
这些断言均指向同一个 mapped 窗口，不是单独创建的协议对象。

## 已知边界 / 下一项结果

已覆盖 handle 的 `set/unset_minimized`、`set/unset_maximized`、
`set/unset_fullscreen`、`activate`、`set_rectangle`、`close`，以及 manager/context 的
`stop`、`get_dock_preview_context`、`show`、`show_tooltip`、`close`、`destroy`。

仍未验证 dock UI 或 preview 的渲染像素；`output_enter` / `output_leave`、title、app-id、pid、
parent 和 foreign handle 对 state event 的线上内容也尚未逐项断言。`set_fullscreen` 当前传
`NULL` output，因此未覆盖“指定 `wl_output` 是 fullscreen hint”这一分支。fixture 的
测试只等待动画的完成事件并读取最终 wrapper 状态，不构成对动画插值或最终帧像素的验证。
