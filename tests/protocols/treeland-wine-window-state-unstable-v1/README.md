# `treeland-wine-window-state-unstable-v1` 测试规范

## 范围

- XML / interface：`treeland_wine_window_state_manager_v1` / `treeland_wine_window_state_v1`
- 测试源码：`tests/protocols/treeland-wine-window-state-unstable-v1/`
- Fixture：desktop integration fixture（`protocol_test_setup` 创建 headless output，
  `ShellHandler` 为 xdg_toplevel 创建真实 `SurfaceWrapper`）
- 覆盖等级：**P / E**

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 |
| --- | --- | --- |
| 初始 state | 创建 xdg_toplevel + `get_window_state` | `state_changed(0)`（无 MINIMIZED、无 ATTENTION） |
| Minimize | 服务端 minimize wrapper | `state_changed(MINIMIZED)` |
| Set attention | 窗口处于 minimized（非激活）状态时 `set_attention(0,0)` | `state_changed(MINIMIZED \| ATTENTION)` |
| Clear attention | `clear_attention()` | `state_changed(MINIMIZED)`（无 ATTENTION） |
| Unminimize | `unminimize()` | `state_changed(0)`（无 MINIMIZED） |

## 已证明的生产链路

客户端创建 xdg_toplevel 后，`ShellHandler::onXdgToplevelSurfaceAdded` 创建真实
`SurfaceWrapper` 并加入 `Workspace`。客户端调用 `get_window_state` 时，
`WineWindowState` 通过 `Helper::rootSurfaceContainer()->getSurface()` 查找到该 wrapper，
连接 `minimizeChanged` 和 `attentionChanged` 信号。初始 `state_changed(0)` 证明 wrapper
创建后未最小化、未需要关注，与协议事件一致。

Fixture 对 wrapper 调用 `minimize()` → `minimizeChanged(true)` 信号触发
`WineWindowState::send_state_changed` → 客户端收到 `state_changed(MINIMIZED)`。证明
minimize 信号链路完整。

在 minimized 状态下，客户端发送 `set_attention(0,0)`。生产代码中
`SurfaceWrapper::setAttention(true)` 对已激活窗口有守卫（直接拒绝），因此 fixture 先
minimize 使窗口失活，确保 `setAttention(true)` 可正常执行。`attentionChanged(true)` 信号
触发 → 客户端收到 `state_changed(MINIMIZED|ATTENTION)`。`clear_attention()` 调用
`SurfaceWrapper::setAttention(false)` → `attentionChanged(false)` → 客户端收到
`state_changed(MINIMIZED)`。

`unminimize()` 调用 `SurfaceWrapper::restoreFromMinimized()` → `minimizeChanged(false)`
→ 客户端收到 `state_changed(0)`。整个过程证明 `minimizeChanged` 和 `attentionChanged`
信号均通过 `WineWindowState` 正确转发为协议事件，且事件中的 state 位掩码与 wrapper 真实
状态完全一致。

## E 级观察：QQuickItem 可见性

E 级测试通过 `invoke_on_server_thread(wine_ws_read_state, &state)` 直接读取
`SurfaceWrapper` 的 QQuickItem 属性，验证协议事件与渲染树状态一致：

| 阶段 | `state.minimized` | `state.visible` | 含义 |
| --- | --- | --- | --- |
| `get_window_state` 后 | 0 | **1** | surface 已在渲染树中可见 |
| `minimize()` 后 | 1 | **0** | surface 从渲染树隐藏（`isVisible()=false`） |
| `set_attention()` 后 | 1 | **0** | attention 标志置位，但 surface 仍隐藏 |
| `clear_attention()` 后 | 1 | **0** | attention 标志清除，surface 仍隐藏 |
| `unminimize()` 后 | 0 | **1** | surface 恢复到渲染树，可见 |

`visible` 字段对应 `SurfaceWrapper::isVisible()` → 最终映射到
`QQuickItem::isVisible()`。协议层 `MINIMIZED` 状态位与 QQuickItem 渲染树可见性严格同步，
minimize 时 `SurfaceWrapper::setVisible(false)` 将 surface item 从父容器摘除，
restore 时重新加入。

服务端实现位于 `src/modules/wine-window-state/winewindowstate.h`、
`winewindowstate.cpp`。

## 已知边界 / 下一项结果

- `activate` 请求：需要窗口处于非激活且非最小化状态，headless 环境中难以构造该条件。
- `activate_denied` 事件：同上，依赖 activate 请求路径。
- double-bind 错误：同一 surface 重复调用 `get_window_state` 的行为未测试。
- inert 对象：xdg_toplevel 销毁后 `wine_window_state_v1` 对象应变为 inert，未验证。
