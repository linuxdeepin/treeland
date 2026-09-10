# `treeland-shortcut-manager-v3` 测试规范

## 范围

- 测试源码：`tests/protocols/treeland-shortcut-manager-v3/`、`tests/protocols/treeland-shortcut-manager-desktop-v3/`
- Fixture：协议 fixture；包含 mapped、聚焦 xdg-toplevel 和上游 `zwp_virtual_keyboard_v1` 的 desktop fixture。
- 覆盖等级：desktop 用例为 **E**；基础 manager 用例为 **P**。

## 实际请求与预期结果

| 场景 | 客户端发送 | 生产业务逻辑与断言 |
| --- | --- | --- |
| 即时绑定 | `acquire`、`bind_key` | 活动 session 的 manager 立即注册绑定；成功时不发送事件，客户端通过 `wl_display.sync` 确认 |
| 名称冲突 | `bind_key`（重复名称、不同按键） | manager 发送 `bind_failure` 事件，error 为 `name_conflict` |
| 捕获快捷键 | 对已聚焦 mapped 窗口调用 `capture_next_shortcut`，虚拟键盘保持 Ctrl+Shift 并发送 evdev `KEY_C` press/release | 生产 capture filter 返回 `captured("Ctrl+Shift+C")` |
| 激活快捷键 | 注册 F2 的 Notify action（即时生效），虚拟键盘发送 evdev `KEY_F2` press | `ShortcutRunner` 发送 `activated("desktop-shortcut", key-press)` |

## v3 协议变更要点

1. **commit 机制移除**：`bind_key`、`bind_swipe_gesture`、`bind_hold_gesture` 立即生效，无需 `commit`。
2. **bind_failure 事件**：绑定失败时逐条发送 `bind_failure` 事件，不影响同批次其他绑定。
3. **action 枚举重编号**：`notify` 从 1→0，所有条目下移 1。
4. **接口版本重置为 1**：客户端绑定版本为 1。
5. **接口重命名**：`treeland_shortcut_manager_v3`、`treeland_shortcut_capture_v3`。

## 已证明的生产链路

客户端 map xdg-toplevel 后，服务端调用 `Helper::activateSurface()` 聚焦它，并逐项断言
真实 wrapper 已加入 `Workspace`、可见，且 `WSeat::keyboardFocusSurface()` 正是该窗口
的 `WSurface`。客户端通过上游 `zwp_virtual_keyboard_manager_v1` 为该 `wl_seat` 创建
virtual keyboard，先发送 XKB keymap，再发送 Ctrl 与 Shift 的 depressed modifier mask。

接着客户端发送 `acquire()` 和 `capture_next_shortcut(toplevel.surface, seat)`，再发送
evdev `KEY_F1`（键码 59）按下。生产 `WInputMethodHelper` 将 virtual keyboard 附着到
`WSeat`；键盘事件进入 `Helper::beforeDisposeEvent()`，由于 capture active，进入
`ShortcutManagerV3::tryHandleCaptureEvent()`。该函数把真实 `QKeyEvent` 规范化为
`F1`，通过 capture resource 发送 `captured`；测试断言仅收到一次，随后
发送 `KEY_F1` release 验证 drain 路径不会产生额外结果。

然后客户端发送 `bind_key("desktop-shortcut", "F2", KEY_PRESS, NOTIFY)`，由于 v3 立即
生效，断言 `bind_failure==0`。发送 evdev `KEY_F2`（键码 60）按下后，事件经过同一个
`WSeat → Helper::beforeDisposeEvent → ShortcutController` 路径，`ShortcutRunner` 执行
`NOTIFY` 并由 manager 向 active session 客户端发送 `activated("desktop-shortcut", KEY_PRESS)`。
测试逐项断言 name、flags 和计数均匹配。

## 未覆盖

这是虚拟输入的语义覆盖，不是物理键盘驱动覆盖。desktop 用例必须在本地通过后，
才能把上述结果标为该提交的执行证据。

当前基础 fixture 的客户端连接活动 session socket，因此只覆盖即时绑定。inactive
session 的暂存与切换后应用，需要第二个 session socket 的专用 fixture。
