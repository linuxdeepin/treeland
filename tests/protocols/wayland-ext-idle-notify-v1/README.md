# `wayland-ext-idle-notify-v1` 测试规范

## 范围

- XML / interface：`ext_idle_notifier_v1` / `ext_idle_notification_v1`（version 1）
- 测试源码：`tests/protocols/wayland-ext-idle-notify-v1/`
- Fixture：headless output fixture + 跨协议 idle-inhibit
- 覆盖等级：E

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `ext_idle_notifier_v1` | 资源创建成功 | P |
| 空闲触发 | `get_idle_notification(1ms, seat)` | 收到 `idled` 事件 | P |
| **跨协议抑制** | 映射 toplevel + `create_inhibitor(surface)` | 新建 `get_idle_notification(1ms)` 后 500ms 内 `idled` **不**触发 | **E** |
| **解除抑制** | 销毁 inhibitor | 新建 `get_idle_notification(1ms)` 后 `idled` 触发 | **E** |

## 生产结果

测试观察 Treeland 通过 `wlr_idle_notifier_v1_create` 提供的空闲通知服务。
通过跨协议行为交叉检查：在映射的 surface 上创建 idle inhibitor → `idled` 不触发 →
销毁 inhibitor → `idled` 触发。这证明生产 `wlr_idle_notifier_v1` 的 `inhibited` 状态
被 `Helper::onNewIdleInhibitor` 修改，而非仅协议存活。

## 已知边界 / 下一项结果

跨协议测试使用 `ProtocolTest.cmake` 的 `EXTRA_XMLS` 同时包含 idle-inhibit XML。
仅验证 inhibited 布尔行为；未验证 `resumed` 事件和 `notify_activity` 路径。
