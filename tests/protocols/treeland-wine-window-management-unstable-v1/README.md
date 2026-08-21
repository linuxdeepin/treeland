# `treeland-wine-window-management-unstable-v1` 测试规范

## 范围

- XML / interface：`treeland_wine_window_manager_v1` / `treeland_wine_window_control_v1`
- 测试源码：`tests/protocols/treeland-wine-window-management-unstable-v1/`
- Fixture：desktop integration fixture（`protocol_test_setup` 创建 headless output，
  `ShellHandler` 为 xdg_toplevel 创建真实 `SurfaceWrapper`）
- 覆盖等级：**P / E**

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 |
| --- | --- | --- |
| 初始化 | `get_window_control` | `window_id(>0)` + `configure_position(serial=0)` + `configure_stacking(topmost=0)` |
| Set position | `set_position(100, 200, 1)` | `configure_position` 的 serial 回显为 1 |
| Set z-order topmost | `set_z_order(HWND_TOPMOST, 0)` | `configure_stacking(topmost=1)` |
| Set z-order notopmost | `set_z_order(HWND_NOTOPMOST, 0)` | `configure_stacking(topmost=0)` |

## 已证明的生产链路

客户端创建 xdg_toplevel 后，`ShellHandler` 创建真实 `SurfaceWrapper`。客户端调用
`get_window_control`，`WineWindowControl` 查找到该 wrapper，记录 `window_id`（单调递增），
连接 `xChanged`、`yChanged` 和 `alwaysOnTopChanged` 信号，然后发送初始
`configure_position(serial=0)` 和 `configure_stacking(topmost=0)`。初始事件证明 wrapper
创建后位置默认 (0,0) 且非置顶。

`set_position(100, 200, 1)` 调用 `wrapper->setPosition(QPointF(100, 200))`。生产代码中
`m_suppressPositionEvents` 标志被设为 `true` 以避免 wrapper 原生位置变化信号回显
serial=0；round-trip 后标志复位，后续 `xChanged`/`yChanged` 信号正常触发
`sendConfigurePosition`。客户端收到的 `configure_position` 中 serial=1 证明请求被正确
执行并回显。

`set_z_order(HWND_TOPMOST, 0)` 调用 `wrapper->setAlwaysOnTop(true)` →
`alwaysOnTopChanged(true)` 信号触发 → 客户端收到 `configure_stacking(topmost=1)`。
`set_z_order(HWND_NOTOPMOST, 0)` 调用 `wrapper->setAlwaysOnTop(false)` 或
`stackToLast()` → 客户端收到 `configure_stacking(topmost=0)`。证明 alwaysOnTop
信号链路完整，协议事件与 wrapper 真实置顶状态一致。

## E 级观察：QQuickItem 几何与 Z 值

E 级测试通过 `invoke_on_server_thread(wine_wm_read_state, &state)` 直接读取
`SurfaceWrapper` 的 QQuickItem 属性，验证协议事件与渲染树状态一致：

| 阶段 | `state.x` | `state.y` | `state.z` | `state.effective_always_on_top` | `state.parent_item_count` |
| --- | --- | --- | --- | --- | --- |
| `get_window_control` 后 | ≥0 | ≥0 | **0** | **0** | **≥1** |
| `set_position(100,200)` 后 | **100** | **200** | 0 | 0 | ≥1 |
| `set_z_order(TOPMOST)` 后 | 100 | 200 | **1** | **1** | ≥1 |
| `set_z_order(NOTOPMOST)` 后 | 100 | 200 | **0** | **0** | ≥1 |

- `x`/`y`：直接对应 `QQuickItem::x()` / `QQuickItem::y()`，由 `setPosition` 驱动。
- `z`：对应 `QQuickItem::z()`。`HWND_TOPMOST` 时 wrapper 被移到 always-on-top 子容器，
  z=1；`HWND_NOTOPMOST` 时移回普通子容器，z=0。
- `effective_always_on_top`：对应 `SurfaceWrapper::effectiveAlwaysOnTop()`。
  当 wrapper 处于 TOPMOST 层时为 1，即使 wrapper 本身的 `alwaysOnTop()` 为 false，
  只要其处于 topmost 容器中 `effectiveAlwaysOnTop()` 即返回 true。
- `parent_item_count`：对应 `parentItem()->childItems().size()`，验证 surface item
  始终在父容器中（minimize 时会从渲染树摘除，E 级不测试该场景）。

服务端实现位于 `src/modules/wine-window-management/winewindowmanagement.h`、
`winewindowmanagement.cpp`。

## 已知边界 / 下一项结果

- `hwnd_bottom` z-order：需要多个同 session 窗口构造堆叠顺序。
- `hwnd_insert_after`：需要指定 sibling window，依赖多窗口场景。
- 无效 `sibling_id` 的错误处理：未验证服务端是否发送协议错误。
- double-bind 错误：同一 surface 重复调用 `get_window_control` 的行为未测试。
- 屏幕外坐标：`set_position` 到超出 output 范围的坐标时行为未验证。
