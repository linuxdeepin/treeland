# `wayland-text-input-unstable-v1` 测试规范

## 范围

- XML / interface：`zwp_text_input_manager_v1` / `zwp_text_input_v1`（version 1）
- 测试源码：`tests/protocols/wayland-text-input-unstable-v1/`
- Fixture：headless output fixture
- 覆盖等级：P

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `zwp_text_input_manager_v1`（v1） | 资源创建成功 | P |
| 创建输入 | `create_text_input(seat)` | 返回非 NULL 的 `zwp_text_input_v1` | P |
| 生命周期 | `activate` + `set_content_type` + `commit_state` + `deactivate` | 资源存活、无协议错误 | P |

## 生产结果

测试观察 Treeland 通过 `WTextInputManagerV1`（由 `WInputMethodHelper` 装配）提供的 text-input-v1
服务。activate/commit_state/deactivate 全程无协议错误，证明输入法集成路径接入正常。

## 已知边界 / 下一项结果

headless 环境无输入法（input-method-v2）连接，故未验证 `enter`/`preedit_string`/`commit_string` 等事件负载
（需 I/E 级 input-method fixture）。未验证 v1 与 v3 同时启用时的交互。
