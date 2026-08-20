# `virtual-keyboard-unstable-v1` 测试规范

## 范围

- XML / interface：`3rdparty/wlroots/protocol/virtual-keyboard-unstable-v1.xml` /
  `zwp_virtual_keyboard_manager_v1`、`zwp_virtual_keyboard_v1` v1
- 测试源码：`tests/protocols/treeland-shortcut-manager-desktop-v2/`
- Fixture：完整生产 `Treeland` / `Helper`、headless output、mapped 且 keyboard-focused xdg toplevel。
- 覆盖等级：**P / E（作为 shortcut desktop 测试的依赖协议）**。

## 必须观察到的结果

| XML 语义 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 创建并提供 keymap | `create_virtual_keyboard(seat)`，以有效 XKB V1 keymap 调用 `keymap` | keyboard 可用，无协议错误 | P |
| raw key 注入 | 调用 `key` 按下/释放 F1、F2 | 已聚焦的生产 shortcut manager 捕获 F1，并因 F2 激活已注册 shortcut | E |
| modifier 注入 | 有效 keymap 后调用 `modifiers(1,0,0,0)` | 已聚焦 app 从真实 `WSeat` 收到 `wl_keyboard.modifiers`，depressed 为 1 | E |
| keymap 前输入 | 两个独立 client 各自在未 keymap 时调用 `key` 或 `modifiers` | display 因 `zwp_virtual_keyboard_v1.error.no_keymap` 断开 | P |
| 资源生命周期 | `destroy` virtual keyboard | client 正常销毁资源，无 use-after-free 或协议错误 | P |

## 生产结果

wlroots 提供 native `wlr_virtual_keyboard_manager_v1`。waylib 的
`WVirtualKeyboardManagerV1` 创建该 global；它由
`Helper::init() → ShellHandler::init() → WInputMethodHelper` 调用
`WServer::attach<WVirtualKeyboardManagerV1>()` 接入。新 virtual keyboard 被 helper 包装为
`WInputDevice` 并 attach 到生产 `WSeat`，由 wlroots 管理 native resource 的 display 生命周期。

### E 级证据

测试 client 在真实 `WSeat` 上创建 virtual keyboard，并把有效 XKB keymap 交给 wlroots。
注入的 F1/F2 不由测试 fixture 直接模拟：它们先进入生产 seat，再由已聚焦窗口上的生产
shortcut manager 捕获，分别得到 capture 和已注册 shortcut 的 activate 结果。`modifiers(1,0,0,0)`
同样经过真实 seat，已聚焦 app 的 `wl_keyboard` listener 必须收到 depressed 为 1 的 event。

故按键和 modifier 的 seat/shortcut 链路是 E 级；keymap 前的 `no_keymap` 错误与 object destroy
仅为 P 级。

## 已知边界 / 下一项结果

现有客户端直接调用 XML 的全部 **5 / 5 request**：manager `create_virtual_keyboard`、keyboard 的
`keymap`、`key`、`modifiers`、`destroy`。

尚未覆盖：

- compositor 授权策略的 `unauthorized` error。当前测试环境允许 test client 创建 virtual keyboard，
  而 wlroots native implementation 也没有授权 callback 或 trusted-client 判定；在引入实际授权策略前，
  不能构造有意义的拒绝断言；
- modifier 对更多组合快捷键、锁定 modifier 与 layout group 的行为。
