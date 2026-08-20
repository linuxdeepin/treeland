# `wlr-foreign-toplevel-management-unstable-v1` 测试规范

## 范围

- XML / interface：`3rdparty/wlroots/protocol/wlr-foreign-toplevel-management-unstable-v1.xml`，
  `zwlr_foreign_toplevel_manager_v1` / `zwlr_foreign_toplevel_handle_v1` v3。
- 测试源码：`tests/protocols/wlr-foreign-toplevel-management-unstable-v1/`。
- Fixture：真实 Treeland desktop integration fixture、headless output、mapped xdg toplevel。
- 覆盖等级：P / E。

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 初始枚举 | map 并设置 title/app-id 的 xdg toplevel | 一个标准 handle，`title/app_id/state/done`，以及同一生产 `SurfaceWrapper` 已进入 workspace | P / E |
| 状态控制 | set/unset minimized、maximized、fullscreen；activate | 同一 `SurfaceWrapper` 的真实状态与默认 seat focus 相应变化 | P / E |
| 客户端关闭 | `set_rectangle`、`close`、handle `destroy` | rectangle request 可派发；生产 xdg toplevel 收到 `close`，handle 正确释放 | P / E |
| manager 生命周期 | `stop` | 唯一 `finished`，随后释放 inert manager proxy | P |

## 生产结果

wlroots 提供 native manager；waylib 的 `WForeignToplevel` 创建并持有它，Treeland 在
`Helper::init()` 中 attach 该 wrapper。测试只连接已发布的标准 global，绝不创建第二个
manager。它与 Treeland 的 `treeland_foreign_toplevel_manager_v1` 自定义扩展是不同协议。

测试客户端先 map 一个真实 xdg toplevel。`ShellHandler` 创建同一个生产
`SurfaceWrapper`，将其加入 `Workspace`；标准 `WForeignToplevel` 再为该 toplevel 发布
`zwlr_foreign_toplevel_handle_v1`。初始 `toplevel/title/app_id/state/done` listener 和
fixture 读取到的 wrapper 必须对应这同一窗口。

随后客户端对这个标准 handle 发送 `set/unset_minimized`、`set/unset_maximized` 与
`set/unset_fullscreen`。fixture 不读取 foreign handle 的缓存，而是读取该
`SurfaceWrapper` 的真实 shell/window state；每次几何状态变化都等待生产动画的 `finished`
信号后才断言。`activate(wl_seat)` 还必须使同一 wrapper 成为默认 seat container 的真实
`keyboardFocusSurface()`。`close` 必须到达这个 mapped xdg client 的 `xdg_toplevel.close`
listener。

因此上述窗口发现、状态控制、焦点与关闭均为 E 级。`set_rectangle` 只证明标准 request
可被派发（P）：Treeland 当前没有消费该 hint 并产生可观察业务状态，测试不会把它计为 E。

## 已知边界 / 下一项结果

- `output_enter/output_leave`、`parent`、`closed` 尚未稳定构造并逐项断言；`set_rectangle`
  是标准 protocol 的 compositor hint，当前没有 Treeland 业务消费者可读取其结果。
- `set_fullscreen` 当前传 `NULL` hint，未验证指定 `wl_output` 的分支。
- 未验证多个 toplevel、父子窗口、output 热插拔或最终渲染像素。
