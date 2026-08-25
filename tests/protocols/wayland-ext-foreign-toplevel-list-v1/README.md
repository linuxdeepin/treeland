# `wayland-ext-foreign-toplevel-list-v1` 测试规范

## 范围

- XML / interface：`ext_foreign_toplevel_list_v1` / `ext_foreign_toplevel_handle_v1`（version 1）
- 测试源码：`tests/protocols/wayland-ext-foreign-toplevel-list-v1/`
- Fixture：headless output fixture
- 覆盖等级：P

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `ext_foreign_toplevel_list_v1`（v1） | 资源创建成功 | P |
| 停止列举 | `stop` 后 roundtrip | 收到 `finished` 事件 | P |

## 生产结果

测试观察 Treeland 通过 `WExtForeignToplevelListV1` 包装器提供的外部 toplevel 列举服务。
headless 环境下无已映射 toplevel，列表为空，`finished` 在 `stop` 后如约返回，证明列举生命周期被正确驱动。

## 已知边界 / 下一项结果

未验证存在真实 xdg toplevel 时 `toplevel` 事件的 title/app_id/identifier 负载，以及
toplevel 增删变化（closed/done）的增量推送（需 E 级 mapped 窗口 fixture）。
