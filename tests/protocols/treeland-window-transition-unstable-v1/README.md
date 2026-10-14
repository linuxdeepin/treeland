# `treeland-window-transition-unstable-v1` 测试规范

## 范围

- XML / interface：`treeland_window_transition_manager_v1` / `treeland_window_transition_rect_v1`
- 测试源码：`tests/protocols/treeland-window-transition-unstable-v1/`
- Fixture：desktop integration fixture（`protocol_test_setup` 取得生产
  `WindowTransitionManagerInterfaceV1`、创建 headless output，并记录
  `ShellHandler::surfaceWrapperAdded` 的 wrapper 以便回读真实 transition 状态）
- 覆盖等级：**P / I / E**

`treeland_window_transition_manager_v1.get_window_transition_rect` 为激活 token 创建
transition rect。测试通过 server bridge 在 compositor 线程回读生产
`WindowTransitionManagerInterfaceV1::hasPendingWindowTransitionRect()`，证明 rect 真的进入
`m_pendingRects`，而不只是请求可被派发。同一 token 再次创建会让前一个 rect 收到一次
`closed`；客户端销毁 rect 后 pending 集合移除，而销毁被覆盖的旧 rect 不会误删当前 rect。

E 级场景走真实 `xdg-activation-v1` 链路：源 toplevel 用激活 token 字符串创建 rect
（geometry + source surface），目标 toplevel 在 map 前 `activate` 该 token，生产侧
`associatePendingRect()` 消费 pending rect 并武装目标 `SurfaceWindowTransition`；回读
`globalRect()` 的尺寸、`active()` 与 `sourceSurface()`，以及被武装 wrapper 的 mapped 状态，
因此断言落在"被武装的是那个尚未 map 的目标窗口"上，而不是"某个窗口被 arm 了"。

两条协议错误路径各自使用独立连接（`wl_resource_post_error` 会终止连接），并断言
`wl_display_get_protocol_error()` 的 interface 与 code。

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| pending 存储 | `get_window_transition_rect(token)` | 生产 `hasPendingWindowTransitionRect(token)` == 1，且没有任何 transition 被武装 | I |
| 过期前不关闭 | 已存储的 pending rect | 未收到 `closed` | P |
| 同 token 覆盖 | 对同一 token 再 `get_window_transition_rect` | 前一个 rect 收到恰好 1 次 `closed`，新的不关闭；再 roundtrip 不重复 | P |
| 旧 rect 析构 | 销毁被覆盖的旧 rect | 该 token 仍在 pending 集合（新 rect 未被误删） | I |
| 客户端销毁 | `treeland_window_transition_rect_v1.destroy` | pending 集合移除该 token | I |
| 激活关联 | 目标 map 前 `xdg_activation_v1.activate(token, target)` | 恰好 1 个 transition 被武装，且属于**未 map** 的窗口，带 40×40 的 rect 与 source surface；该 token 的 pending 被消费，另一个 token 的 rect 不被消费 | E |
| 几何更新 | 关联后 `set_geometry(3, 4, 20, 30)` | 被武装的 rect 变为 20×30，仍 armed | E |
| 0×0 禁用 | `set_geometry(0, 0, 0, 0)` | 不再 `active()`（armed_count == 0），但关联仍在（rect_present == 1，尺寸 0×0） | E |
| 重新启用 | `set_geometry(0, 0, 40, 40)` | 重新 `active()`，rect 40×40 | E |
| source 清空 / 重设 | `set_source_surface(NULL)`，随后连续两次设同一个 surface | 清空后无 source surface 且仍 armed；重设后 source surface 回来且不报错 | E |
| 激活迟到 | 目标已 map 后 `xdg_activation_v1.activate(token, target)` | rect 收到恰好 1 次 `closed`，pending 清空，且没有任何 transition 被武装 | E |
| 无 originating surface | token 未 `set_surface`，目标未 map 时 `activate` | rect 收到恰好 1 次 `closed`，pending 清空，且没有任何 transition 被武装 | E |
| 负几何 | `set_geometry(0, 0, -1, 4)` | `invalid_geometry` 协议错误 | P |
| 角色冲突 | 对已带 role 的 mapped toplevel surface `set_source_surface` | `invalid_surface` 协议错误 | P |

## 生产结果

- `WindowTransitionManagerInterfaceV1::m_pendingRects` 的真实内容，经
  `hasPendingWindowTransitionRect()` 回读；`consume` 与 `discard` 都以 token 为键，因此
  另一个 token 的 rect 会留在集合里。
- `closed` 事件来自 `WindowTransitionRectV1::notifyDiscarded()`（同 token 覆盖、激活迟到、
  无 originating surface 三条路径），断言按次数而非布尔，覆盖协议"至多一次"的约定。
- 被武装目标的 `SurfaceWindowTransition` 状态经 `globalRect()`（尺寸）、`active()`、
  `sourceSurface()` 与所属 wrapper 的 `mapped()` 回读，对应 `Helper` 的
  `associatePendingRect()`；几何更新、0×0 禁用与 source 清空分别对应
  `updateWindowTransitionRect()` / `setWindowTransitionSource()` /
  `clearWindowTransitionSource()`。

## 已知边界 / 下一项结果

- 未覆盖目标销毁触发的 rect `closed`：依赖关闭动画时序，客户端侧没有 roundtrip 之外
  的确定性同步点。
- 未覆盖 `set_geometry` 的 60 s sweep 过期：需要真实等待或给常量加测试注入点。
- 未覆盖跨 client 的 `invalid_surface` 分支（生产里的 `wl_resource_get_client` 不匹配）：
  libwayland-server 对不属于本连接的对象 id 解析为 NULL，生产代码会走"清空 source"
  而不是报错，该分支是否可达需实测确认。
- 只断言 rect 的尺寸，未断言其全局位置（位置依赖 output 布局与源窗口位置）。
- 只断言 source surface 存在，未断言它就是客户端设置的那个 surface（wrapper 未暴露
  surface 标识；加强这一断言需要生产侧新增访问器）。
