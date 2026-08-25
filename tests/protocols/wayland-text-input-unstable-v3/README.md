# `wayland-text-input-unstable-v3` 测试规范

## 范围

- XML / interface：`zwp_text_input_manager_v3` / `zwp_text_input_v3`（version 1）
- 测试源码：`tests/protocols/wayland-text-input-unstable-v3/`
- Fixture：headless output fixture
- 覆盖等级：P

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `zwp_text_input_manager_v3`（v1） | 资源创建成功 | P |
| 创建输入 | `get_text_input(seat)` | 返回非 NULL 的 `zwp_text_input_v3` | P |
| 状态提交 | `enable` + `set_surrounding_text` + `commit`（两次） | 资源存活、无协议错误 | P |

## 生产结果

测试观察 Treeland 通过 `WTextInputManagerV3`（master 上由 `WInputMethodHelper::onAttach` 装配）
提供的 text-input-v3 服务。enable/disable + commit 全程无协议错误，证明 v3 输入法集成路径接入正常。

## 已知边界 / 下一项结果

headless 环境无输入法连接，故未验证 `enter`/`preedit_string`/`commit_string`/`done` 事件负载
（需 I/E 级 input-method fixture）。
