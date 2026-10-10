# `treeland-shortcut-manager-v2` 测试规范

## 范围

- 测试源码：`tests/protocols/treeland-shortcut-manager-v2/`、`tests/protocols/treeland-shortcut-manager-desktop-v2/`
- Fixture：协议 fixture；包含 mapped、聚焦 xdg-toplevel 和上游 `zwp_virtual_keyboard_v1` 的 desktop fixture。
- 覆盖等级：desktop 用例为 **E**；基础 manager 用例为 **P**。

## 实际请求与预期结果

| 场景 | 客户端发送 | 生产业务逻辑与断言 |
| --- | --- | --- |
| 基础提交 | `acquire`、`bind_key`、`commit` | 活动 session 的 manager 发送 `commit_success` |
| 捕获快捷键 | 对已聚焦 mapped 窗口调用 `capture_next_shortcut`，虚拟键盘发送 evdev `KEY_F1` press/release | 生产 capture filter 返回 `captured("F1")` |
| 激活快捷键 | 注册 `F2` 的 Notify action，虚拟键盘发送 evdev `KEY_F2` press | `ShortcutRunner` 发送 `activated("desktop-shortcut", key-press)`；其 release 被镜像吞掉，客户端收不到 `wl_keyboard.key` |
| 快捷键按键不出现在 enter | 注册 `Ctrl+K` 的 Notify action，虚拟键盘发送 `KEY_LEFTCTRL` press + `Control` modifier、`KEY_K` press；按下期间 fixture 同步切换焦点到第二个 toplevel | 第二个 toplevel 的 `wl_keyboard.enter` 按键集合包含仍按住的 `KEY_LEFTCTRL`(29)，但不包含被消费的 `KEY_K`(37) |
| 快捷键 release 被无条件吞掉 | 先释放 modifier（`KEY_LEFTCTRL` release + `modifiers(0)`），再释放 `KEY_K` | 客户端收到 `KEY_LEFTCTRL` 的 `wl_keyboard.key` release，但收不到任何 `KEY_K` 事件（press 或孤立 release） |

## 已证明的生产链路

客户端 map xdg-toplevel 后，服务端调用 `Helper::activateSurface()` 聚焦它，并逐项断言
真实 wrapper 已加入 `Workspace`、可见，且 `WSeat::keyboardFocusSurface()` 正是该窗口
的 `WSurface`。客户端通过上游 `zwp_virtual_keyboard_manager_v1` 为该 `wl_seat` 创建
virtual keyboard，先发送 XKB keymap，再发送 `Control` 的 depressed modifier mask。

接着客户端发送 `acquire()` 和 `capture_next_shortcut(toplevel.surface, seat)`，再发送
evdev `KEY_F1`（键码 59）按下。生产 `WInputMethodHelper` 将 virtual keyboard 附着到
`WSeat`；键盘事件进入 `Helper::beforeDisposeEvent()`，由于 capture active，进入
`ShortcutManagerV2::tryHandleCaptureEvent()`。该函数把真实 `QKeyEvent` 规范化为 `F1`，
通过 capture resource 发送 `captured`；测试断言仅收到一次，随后发送 `KEY_F1` release
验证 drain 路径不会产生额外结果。

然后客户端发送 `bind_key("desktop-shortcut", "F2", KEY_PRESS, NOTIFY)` 和 `commit()`，
断言 `commit_success==1` 且没有 `commit_failure`。发送 evdev `KEY_F2`（键码 60）按下后，
事件经过同一个 `WSeat → Helper::beforeDisposeEvent → ShortcutController` 路径，
`ShortcutRunner` 执行 `NOTIFY` 并由 manager 向 active session 客户端发送
`activated("desktop-shortcut", KEY_PRESS)`。测试逐项断言 name、flags 和计数均匹配，
并断言其 release 被过滤集合吞掉。

### E 级：被消费快捷键的 enter 与 release

客户端在同一连接上创建第二个 xdg-toplevel，并对两个窗口做真实
`activateSurface` 切换。随后注册 `Ctrl+K` 的 Notify action，并把 virtual keyboard 的
modifier mask 设为 `Control`，再依次发送 `KEY_LEFTCTRL`（evdev 29）press 与 `KEY_K`
（evdev 37）press：`Ctrl` 既是真实 key press（进入 `wlr_keyboard.keycodes`），又通过
modifiers 请求更新 xkb state（`WSeat::keyModifiers` 报告 Control），因此 `KEY_K` 匹配
已注册快捷键，被 `Helper::beforeDisposeEvent` 在 `dispatchKeyEvent` 之前记入
`WSeat::addFilteredKey()` 并被消费。测试断言 manager 恰好发送第二个
`activated("held-shortcut")`。

在 `KEY_K` 仍按住时，fixture 同步调用 `Helper::activateSurface(secondary)` 切换键盘焦点。
新的 `wl_keyboard.enter` 按键集合来自 `WSeat::doSetKeyboardFocus()` 中经
`unfilteredKeycodes()` 过滤的 `wlr_keyboard->keycodes`，测试断言它包含仍按住的
`KEY_LEFTCTRL`(29)，但不包含被消费的 `KEY_K`(37)。这正是 `865d3d475` 修复的缺陷：
修复前 `enter_keys` 会带上 37，测试在 stage 8 失败。

最后客户端先释放 modifier（`modifiers(0)` 与 `KEY_LEFTCTRL` release），再释放 `KEY_K`。
`Helper::beforeDisposeEvent` 的镜像 release 分支位于 inhibitor/capture 提前返回之前，
无条件吞掉过滤器中的按键，因此客户端收不到任何 `KEY_K` 的 `wl_keyboard.key`（既不误判
按下，也不产生孤立 release），但仍能收到 `KEY_LEFTCTRL` 的正常 release。

## 未覆盖

这是虚拟输入的语义覆盖，不是物理键盘驱动覆盖。desktop 用例必须在本地通过后，
才能把上述结果标为该提交的执行证据。

当前基础 fixture 的客户端连接活动 session socket，因此只覆盖即时提交。inactive
session 的暂存与切换后应用，需要第二个 session socket 的专用 fixture。

## 回归证据

在移除 `865d3d475` 的 waylib/helper/controller 改动后重建并运行本测试，desktop 用例在
stage 8 失败（`enter_keys=2`，被消费的 `KEY_K` 出现在 enter 中）；恢复修复后通过。
该对照执行证明本用例确实覆盖了被消费快捷键的 enter 过滤与 release 吞掉行为。
