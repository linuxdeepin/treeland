# `wayland-text-input-unstable-v1` 测试规范

## 范围

- XML / interface：`zwp_text_input_manager_v1` / `zwp_text_input_v1`（version 1）
- 测试源码：`tests/protocols/wayland-text-input-unstable-v1/`
- Fixture：headless output fixture + 映射 xdg-toplevel
- 覆盖等级：E

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `zwp_text_input_manager_v1` | 资源创建成功 | P |
| 创建 text_input | `create_text_input(seat)` | 返回非 NULL `zwp_text_input_v1` | P |
| 激活 | `activate(seat, surface)` | 无协议错误 | P |
| **生产回读** | — | 回读真实 `WTextInputV1::activate` Q_SIGNAL 被触发 | **E** |

## 生产结果

测试观察 Treeland 通过 `WTextInputManagerV1` 提供的文本输入 v1 服务。
客户端 `activate` 后，setup 通过 `WTextInputManagerV1::newTextInput` 信号捕获
`WTextInputV1*`，连接到其 `activate()` Q_SIGNAL，回读断言信号被触发，
证明文本输入激活请求到达生产文本输入管道。

## 已知边界 / 下一项结果

仅验证 `activate` 信号；未验证 `deactivate`、`set_surrounding_text`、`show_input_panel` 等请求。
