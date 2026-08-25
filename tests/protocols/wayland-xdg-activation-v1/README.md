# `wayland-xdg-activation-v1` 测试规范

## 范围

- XML / interface：`xdg_activation_v1` / `xdg_activation_token_v1`（version 1）
- 测试源码：`tests/protocols/wayland-xdg-activation-v1/`
- Fixture：headless output fixture
- 覆盖等级：P

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `xdg_activation_v1`（v1） | 资源创建成功 | P |
| 申请令牌 | `get_activation_token` + `commit` 后 roundtrip | `done` 事件携带非空 token 字符串 | P |

## 生产结果

测试观察 Treeland 通过 `ActivationManagerInterfaceV1` 包装器真实生成的激活令牌。
非空 token 字符串证明生产令牌签发路径被触发。

## 已知边界 / 下一项结果

仅验证令牌签发。未验证 `activate(token, surface)` 将令牌作用于已映射窗口后的聚焦/激活业务结果（E 级），
也未验证令牌过期、跨 seat serial 校验等语义。
