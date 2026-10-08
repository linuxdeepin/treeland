# `wayland-text-input-unstable-v1` 测试规范

## 范围

- XML / interface：`zwp_text_input_manager_v1` / `zwp_text_input_v1`（version 1）
- 测试源码：`tests/protocols/wayland-text-input-unstable-v1/`
- Fixture：headless output fixture + 映射 xdg-toplevel；第二条连接作为 fake
  `zwp_input_method_v2`（`EXTRA_XMLS` 引入 `input-method-unstable-v2.xml`）
- 覆盖等级：E

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `zwp_text_input_manager_v1` | 资源创建成功 | P |
| 创建 text_input | `create_text_input(seat)` | 返回非 NULL `zwp_text_input_v1` | P |
| 激活 | `activate(seat, surface)` | 无协议错误 | P |
| **生产回读** | — | 回读真实 `WTextInputV1::activate` Q_SIGNAL 被触发 | **E** |
| 焦点进入 | `activate(seat, surface)` | 收到一次 `enter`；fake IM 收到一次 `activate` | E |
| 转发 preedit | `commit_state(7)`，fake IM `set_preedit_string("pre",0,3)` + `commit` | 客户端收到 `commit_string`、`preedit_cursor`、`preedit_styling` 和 `preedit_string("pre")`，且 serial 等于 `commit_state` 传入的 7 | E |
| 清空 preedit | fake IM 不发送 `set_preedit_string` 直接 `commit` | 客户端再收到一次 `preedit_string` 且文本为空、serial 仍为 7（v1 没有 `done`，空的 `preedit_string` 是唯一能清掉组合文本的手段） | E |
| 丢焦点不清 preedit | fake IM 被销毁，focused text-input 因此收到 `leave` | 只收到 `leave`，不补发 `preedit_string`：v1 的 `preedit_string` 需要 `commit_state` 提供的 serial，丢焦点清 preedit 时该 serial 语义不明确 | E |

## 生产结果

测试观察 Treeland 通过 `WTextInputManagerV1` 提供的文本输入 v1 服务。
客户端 `activate` 后，setup 通过 `WTextInputManagerV1::newTextInput` 信号捕获
`WTextInputV1*`，连接到其 `activate()` Q_SIGNAL，回读断言信号被触发，
证明文本输入激活请求到达生产文本输入管道。

第二条连接作为 fake input method，让 `WInputMethodHelper::handleIMCommitted` 真正跑到
`WTextInputV1::handleIMCommitted`（`WTextInputV1::sendEnter()` 无条件发出 `enabled()`，
所以聚焦的 v1 text-input 会成为 `enabledTextInput`）。客户端按顺序记录
`enter` / `preedit_string` / `commit_string` / `leave`，断言完整序列 `ekpkpl`：
去掉空 preedit 转发分支后，用例在 stage 3 失败。

## 已知边界 / 下一项结果

未验证 `deactivate`、`set_surrounding_text`、`show_input_panel` 等请求，
也未覆盖 `delete_surrounding_text` 与 `preedit_styling` 的具体参数值。
