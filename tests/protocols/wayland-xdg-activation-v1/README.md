# `wayland-xdg-activation-v1` 测试规范

## 范围

- XML / interface：`xdg_activation_v1` / `xdg_activation_token_v1`（version 1）
- 测试源码：`tests/protocols/wayland-xdg-activation-v1/`
- Fixture：headless output fixture + 映射 xdg-toplevel
- 覆盖等级：E

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `xdg_activation_v1` | 资源创建成功 | P |
| 获取 token | `get_activation_token` | 收到 `done` 事件 + 非空 token | P |
| 激活 | `activate(token, surface)` | 无协议错误 | P |
| **生产回读** | — | 回读真实 `ActivationManagerInterfaceV1::activateRequested` 信号的 `disposition` != Invalid | **E** |

## 生产结果

测试观察 Treeland 通过 `ActivationManagerInterfaceV1` 提供的激活服务。
客户端 `activate` 后，setup 通过 `ActivationManagerInterfaceV1::activateRequested` Qt 信号
捕获 `TokenDisposition`，回读断言其 != Invalid，证明激活请求到达生产激活管道。

## 已知边界 / 下一项结果

仅验证 `disposition != Invalid`；未验证 Attention vs Active 的具体 disposition 值与焦点切换行为。
