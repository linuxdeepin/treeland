# `wayland-text-input-unstable-v3` 测试规范

## 范围

- XML / interface：`zwp_text_input_manager_v3` / `zwp_text_input_v3`（version 1）
- 测试源码：`tests/protocols/wayland-text-input-unstable-v3/`
- Fixture：headless output fixture + 映射 xdg-toplevel
- 覆盖等级：E

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `zwp_text_input_manager_v3` | 资源创建成功 | P |
| 创建 text_input | `get_text_input(seat)` | 返回非 NULL `zwp_text_input_v3` | P |
| 启用 + 提交 | `enable` + `commit` | 无协议错误 | P |
| **生产回读** | — | 回读真实 `wlr_text_input_v3::current_enabled` == true | **E** |

## 生产结果

测试观察 Treeland 通过 `WTextInputManagerV3` 提供的文本输入 v3 服务。
客户端 `enable` + `commit` 后，setup 通过 `WTextInputManagerV3::newTextInput` 信号捕获
`WTextInputV3*`，回读真实 `wlr_text_input_v3::current_enabled` 断言为 true，
证明文本输入启用请求到达生产文本输入管道。

## 已知边界 / 下一项结果

仅验证 `current_enabled`；未验证 `surrounding_text`、`content_type`、`cursor_rectangle` 等状态字段。
