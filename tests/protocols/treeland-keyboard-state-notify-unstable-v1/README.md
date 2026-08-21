# `treeland-keyboard-state-notify-unstable-v1` 测试规范

## 范围

- XML / interface：`treeland_keyboard_state_notify_manager_v1` / `treeland_keyboard_state_watcher_v1`
- 测试源码：`tests/protocols/treeland-keyboard-state-notify-unstable-v1/`
- Fixture：desktop integration fixture（`protocol_test_setup` 创建 headless output）
- 覆盖等级：**P**。

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 |
| --- | --- | --- |
| Null seat watcher | `get_keyboard_state_watcher(null)` | watcher 创建成功，无 crash |
| Apply with modifiers | `set_modifiers(CAPS_LOCK)` + `set_flags(LOCKED\|UNLOCKED)` + `apply` | 无物理键盘时安全完成；不发送伪造的状态事件 |
| Real seat watcher, no modifiers | 绑定真实 seat 的 watcher 后 `apply` 无 modifiers | 不发送任何事件，不崩溃 |

## 已证明的生产链路

基础用例证明协议接口的绑定、双缓冲配置和空配置行为正常。

客户端绑定 `keyboard_state_notify_manager_v1` 后，`get_keyboard_state_watcher(null)`
创建 watcher。`set_modifiers` + `set_flags` + `apply` 触发 double-buffer 语义：先
累加 modifiers 和 flags，再在 `apply` 时一次性查询当前键盘状态并发送
`current_state`。headless 环境中 `getSeatKeyboard()` 返回 `nullopt`（无物理键盘设备），
因此不会发送 `current_state` 事件。测试断言 watcher 创建成功且 `apply` 不崩溃，证明
空 modifier 和空键盘路径不会触发空遍历。

服务端实现位于
`src/modules/keyboard-state-notify/keyboardstatenotifymanagerinterfacev1.h`、
`keyboardstatenotifymanagerinterfacev1.cpp`。

## 已知边界 / 下一项结果

- 物理键盘产生的 key-event 路径：尚未覆盖。后续应新建仅在
  `TREELAND_ENABLE_UINPUT_PROTOCOL_TESTS=ON` 时注册的 uinput E2E target，验证
  `uinput → libinput → WBackend → keyboardGroupKeyboard() → state_changed`，而不是把
  virtual keyboard 变成当前 seat keyboard。
- `current_state` 的初始值：未单独覆盖 watcher 在创建时已处于 Caps Lock locked 状态的
  初始同步。
- 多 watcher：多个 watcher 同时监听同一或不同 seat 的行为未验证。
- seat 销毁后 watcher 行为：seat 销毁时 watcher 是否正确清理或变为 inert 未验证。
- re-apply 语义：重复 `apply` 相同 modifier 集合是否去重或重发事件未验证。
