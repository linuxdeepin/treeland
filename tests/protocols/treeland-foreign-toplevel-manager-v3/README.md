# `treeland-foreign-toplevel-manager-v3` 测试规范

## 范围

- 测试源码：`tests/protocols/treeland-foreign-toplevel-manager-v3/`
- Fixture：带 headless output 和已 configure、mapped xdg-toplevel 的 desktop fixture。
- 覆盖等级：**E**。

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 |
| --- | --- | --- |
| toplevel 发现 | map xdg-toplevel | `ShellHandler` 创建 workspace `SurfaceWrapper`，manager 恰好发送一个带 identifier 的 foreign handle |
| Dock preview | 对该 identifier 发 `show(10,20,BOTTOM)`、`show_list(30,40,LEFT)`、未知 identifier、`show_tooltip_surface`（无内容、换 surface、同 surface 换位置）、context close | 生产 preview/tooltip/close 回调收到预期参数；`show` 报 thumbnail 布局、`show_list` 报 list 布局；未知 identifier 产生空 surface 列表而非协议错误；tooltip 回调收到客户端 surface 及其已提交的尺寸；无内容的 show 不产生回调 |
| 最小化与关闭 | 对 handle 发 `set_minimized`、`unset_minimized`、`close` | 真实 wrapper 的 minimized 状态变化；xdg client 收到 close |
| 最大化 | 对 handle 发 `set_maximized`、`unset_maximized` | 同一真实 `SurfaceWrapper` 进入、退出 `Maximized` 状态 |
| 全屏 | 对 handle 发 `set_fullscreen(NULL)`、`unset_fullscreen` | 同一真实 `SurfaceWrapper` 进入、退出 `Fullscreen` 状态 |
| 布局与最小化正交 | `set_maximized`/`set_fullscreen` 后发 `set_minimized`、`unset_minimized`，再 `unset_maximized`/`unset_fullscreen` | 最小化期间 `SurfaceWrapper::surfaceState()` 保持布局状态且窗口不可见；恢复后仍为该布局状态并重新可见；取消布局状态后回到 `Normal` 且不再最小化 |
| 激活 | 对 handle 发 `activate(wl_seat)` | `WaylandServer` 将 wrapper 设为 activated，且它成为该 seat 的真实 keyboard focus surface |
| 窗口代表区域 | 对 handle 的 mapped xdg `wl_surface` 发 `set_icon_geometry(11,12,130,140)` | production rectangle handler 将局部坐标换算为 wrapper 的 `iconGeometry`：`wrapper.position + QRect(11,12,130,140)` |

## 已证明的生产链路

客户端 map xdg-toplevel 后，测试等待 configure，并同时断言：`ShellHandler` 创建的
wrapper 已 mapped、已加入 `Workspace`，且 manager 的 `toplevel` 事件只给出一个
真实 `treeland_foreign_toplevel_handle_v3` 和 identifier。该 identifier 被放入
`show` 的 `wl_array`，客户端发送 `show(identifiers, 10, 20, BOTTOM)`；服务端生产
manager 发出 `requestDockPreview`，fixture 读取到绝对坐标 `(10,20)`、`BOTTOM` 和
一个真实 `WSurface`。

状态转换关闭几何动画：desktop fixture 通过 `animationConfig` 关闭 `maximize`/
`fullscreen`/`restoreWindow` 三个动画开关，使每个状态请求立即落地 `surfaceState`，
不依赖渲染线程驱动的 `OpacityAnimator`。每次状态请求后 fixture render 一帧以刷新排队的
场景图状态，再读取 wrapper 状态。这样测试验证的是协议状态转换本身，而不是几何动画的
插值或收敛时序；正常桌面进程的动画行为不受影响。

同一 context 的 `show_tooltip_surface(surface, 1, 2, LEFT)` 在 surface 还没有提交内容时
属于客户端违约，服务端不得发出 `requestDockPreviewTooltip`；该 surface 提交内容后再次 show
(5, 6, TOP)，回调才拿到这块 `wl_surface`，其逻辑尺寸（29x19）取自已提交的 buffer；换一块
51x17 的 surface 再 show，回调换成新的尺寸与坐标；对已显示的同一个 surface 再 show 到新位置
(11, 12, LEFT) 不需要新的提交，回调直接带新坐标。`context.close` 到达 `requestDockClose`。此外，foreign
handle 的 `set_minimized`/`unset_minimized` 分别使该 wrapper 的 shell surface 进入/退出
minimized；`set_maximized`/`unset_maximized` 分别改变 wrapper 的 `Maximized` 状态；
`set_fullscreen(NULL)`/`unset_fullscreen` 分别改变其 `Fullscreen` 状态。这里读取的是
compositor 的 `SurfaceWrapper::surfaceState()`，不是 foreign handle 自身缓存的 state event。

最小化标志与布局状态正交：在 `Maximized` 或 `Fullscreen` 状态下发 `set_minimized`，测试断言
`SurfaceWrapper::isMinimized()` 为真、`surfaceState()` 仍为原布局状态、且窗口不可见；
`unset_minimized` 后布局状态不变且窗口重新可见；随后 `unset_maximized`/`unset_fullscreen`
必须回到 `Normal`、可见且不再最小化。这覆盖了“全屏→最小化→恢复→取消全屏后卡在最小化”的
回归路径，而不只是顺序地单独测试各状态。

`activate(wl_seat)` 进入 manager 连接的
`WaylandServer::forceActivateSurface(wrapper, ..., seat)`，测试读取 wrapper 的 activated 标记和
root surface container 默认 seat container 的 `keyboardFocusSurface()`，两者都必须指向同一
mapped wrapper。`set_icon_geometry` 则
经过 `iconGeometryChanged` 的生产连接，以发送该请求的 mapped xdg surface 查找 dock wrapper，
再把局部矩形加到它的位置后写入被测 wrapper 的 `iconGeometry`。测试断言最终 geometry 的
相对关系，故窗口不在 `(0,0)` 时仍成立。`handle.close` 让客户端 xdg-toplevel 接到 close。
这些断言均指向同一个 mapped 窗口，不是单独创建的协议对象。

## 已知边界 / 下一项结果

已覆盖 handle 的 `set/unset_minimized`、`set/unset_maximized`、
`set/unset_fullscreen`、`activate`、`set_icon_geometry`、`close`，以及 manager/context 的
`stop`、`get_dock_preview_context`、`show`、`show_tooltip_surface`、`close`、`destroy`。

布局与最小化的组合已覆盖 `Maximized`、`Fullscreen` 两种布局；`Tiling` 没有对应的 foreign
handle 请求，仍未被组合覆盖。

仍未验证 dock UI 或 preview 的渲染像素；`output_enter` / `output_leave`、title、app-id、pid、
parent 和 foreign handle 对 state event 的线上内容也尚未逐项断言。`set_fullscreen` 当前传
`NULL` output，因此未覆盖“指定 `wl_output` 是 fullscreen hint”这一分支。fixture 关闭了几何
动画，只读取最终 wrapper 状态，不构成对动画插值或最终帧像素的验证。
