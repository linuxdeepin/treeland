# `wayland-ext-idle-notify-v1` 测试规范

## 范围

- XML / interface：`ext_idle_notifier_v1` / `ext_idle_notification_v1`（version 1）
- 测试源码：`tests/protocols/wayland-ext-idle-notify-v1/`
- Fixture：headless output fixture（seat 无输入活动）
- 覆盖等级：P

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `ext_idle_notifier_v1`（v1） | 资源创建成功 | P |
| 申请空闲通知 | `get_idle_notification(1ms, seat)` | 返回非 NULL 的 notification | P |
| 空闲触发 | 驱动服务端事件循环等待 ≥1ms | 收到 `idled` 事件 | P |

## 生产结果

测试观察 Treeland 通过 `wlr_idle_notifier_v1_create` 提供的空闲通知服务。
headless seat 无任何输入活动，1ms 超时后服务端真实触发 `idled` 事件，证明空闲计时器接入 seat 并工作。

## 已知边界 / 下一项结果

依赖计时器时序，超时取最小值（1ms）以加速用例；未验证 `resumed`（活动恢复）事件
（需 I 级注入输入活动），也未验证多 seat / 多通知的隔离。
