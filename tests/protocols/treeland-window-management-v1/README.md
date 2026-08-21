# `treeland-window-management-v1` 测试规范

## 范围

- 测试源码：`tests/protocols/treeland-window-management-v1/`、`tests/protocols/treeland-window-management-desktop-v1/`
- Fixture：协议 fixture；带 headless output 与 mapped xdg-toplevel 的 desktop fixture。
- 覆盖等级：show-desktop 为 **E**；manager state 事件为 **P/I**。

## 实际请求与预期结果

| 场景 | 客户端发送 | 生产业务逻辑与断言 |
| --- | --- | --- |
| 初始/状态 API | 绑定并设置 desktop state | 初始事件与状态变化事件匹配生产 manager 状态 |
| Show desktop | 对已有 mapped 窗口设置 show-desktop | 等待生产异步过渡后，真实 wrapper 不在 paint order 且不可见 |
| 恢复 desktop | 设置 normal desktop state | 等待过渡后，同一 wrapper 可见且重新进入 paint order |

## 已证明的生产链路

客户端 map xdg-toplevel 后，fixture 先断言 `ShellHandler` 创建的 wrapper 已加入
`Workspace`、可见、未最小化，且 `WOutputRenderWindow::paintOrderItemList()` 包含该
wrapper；初始 `showDesktopState()` 为 `NORMAL`。

客户端发送 `set_desktop(SHOW)`。除等待 `desktop_state(SHOW)` 事件外，fixture 以
10ms 轮询、最多 2 秒等待生产可见性过渡完成，然后断言 `showDesktopState()==SHOW`、
`wrapper->isVisible()==false`、`wrapper->isMinimized()==false`，并读取 paint order 确认
该窗口不再参与渲染顺序。随后发送 `set_desktop(NORMAL)`，同样等待过渡完成并断言
state event/内部 state 均为 NORMAL、同一 wrapper 重新可见且回到 paint order。

因此证明的不是 manager 回显状态，而是 show-desktop 对真实窗口可见性和渲染顺序的
实际影响。

## 未覆盖

仅覆盖一个 toplevel；多工作区、minimized policy 与 preview UI 仍是独立场景。
