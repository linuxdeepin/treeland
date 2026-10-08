# `wayland-text-input-unstable-v3` 测试规范

## 范围

- XML / interface：`zwp_text_input_manager_v3` / `zwp_text_input_v3`（version 1）
- 测试源码：`tests/protocols/wayland-text-input-unstable-v3/`
- Fixture：headless output fixture + 映射 xdg-toplevel；第二条连接作为 fake
  `zwp_input_method_v2`（`EXTRA_XMLS` 引入 `input-method-unstable-v2.xml`）
- 覆盖等级：E

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `zwp_text_input_manager_v3` | 资源创建成功 | P |
| 创建 text_input | `get_text_input(seat)` | 返回非 NULL `zwp_text_input_v3` | P |
| 启用 + 提交 | `enable` + `commit` | 无协议错误 | P |
| **生产回读** | — | 回读真实 `wlr_text_input_v3::current_enabled` == true | **E** |
| 焦点进入 | 创建 text_input（toplevel 已聚焦） | 收到一次 `enter`；fake IM 收到一次 `activate` | E |
| 转发 preedit | fake IM `set_preedit_string("pre",0,3)` + `commit` | 客户端依次收到 `preedit_string("pre")`、`done` | E |
| 清空 preedit | fake IM 不发送 `set_preedit_string` 直接 `commit`（fcitx5 清空 preedit 时的行为） | 客户端收到一次空的 `preedit_string` 且文本为空（此前只在非空时转发，导致应用永远保留旧 preedit） | E |
| 丢焦点时清 preedit | fake IM 被销毁，focused text-input 因此收到 `leave`（与切窗口时走同一条 `sendLeave()` 路径） | 客户端在 `leave` 之前依次收到空的 `preedit_string` 和 `done`（断言校验完整事件顺序 `epdpdpdl`），组合文本被清除而不是留给客户端自行处理 | E |
| 已 disable 时不发状态更新 | 客户端 `disable` + `commit`，再销毁 fake IM 触发丢焦点 | 只收到 `leave`，不补发 `preedit_string`/`done`：已经 disable 的客户端不应收到活动输入会话之外的状态更新 | E |

## 生产结果

测试观察 Treeland 通过 `WTextInputManagerV3` 提供的文本输入 v3 服务。
客户端 `enable` + `commit` 后，setup 通过 `WTextInputManagerV3::newTextInput` 信号捕获
`WTextInputV3*`，回读真实 `wlr_text_input_v3::current_enabled` 断言为 true，
证明文本输入启用请求到达生产文本输入管道。

第二条连接作为 fake input method，让 `WInputMethodHelper::handleIMCommitted` 与
`notifyLeave()` 真正跑到 `WTextInputV3`：客户端按顺序记录 `enter` / `preedit_string` /
`commit_string` / `done` / `leave`，断言「空 preedit + done 必须早于 leave」。
去掉 `WTextInputV3::sendLeave()` 中的清空逻辑，或让它只转发非空 preedit，用例都会失败。

## 已知边界 / 下一项结果

未验证 `surrounding_text`、`content_type`、`cursor_rectangle` 等状态字段。
